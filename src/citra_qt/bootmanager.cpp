// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPainter>
#include <QWindow>
#include "citra_qt/bootmanager.h"
#include "citra_qt/main.h"
#include "common/color.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/perf_stats.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "video_core/custom_textures/custom_tex_manager.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_software/renderer_software.h"

#ifdef ENABLE_OPENGL
#include <glad/glad.h>

#include <QOffscreenSurface>
#include <QOpenGLContext>
#endif

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/objc.h>
#endif

#if !defined(WIN32)
#include <qpa/qplatformnativeinterface.h>
#endif

static Frontend::WindowSystemType GetWindowSystemType();

EmuThread::EmuThread(Core::System& system_, Frontend::GraphicsContext& core_context)
    : system{system_}, core_context(core_context) {}

EmuThread::~EmuThread() = default;

static GMainWindow* GetMainWindow() {
    const auto widgets = qApp->topLevelWidgets();
    for (QWidget* w : widgets) {
        if (GMainWindow* main = qobject_cast<GMainWindow*>(w)) {
            return main;
        }
    }

    return nullptr;
}

void EmuThread::run() {
    MicroProfileOnThreadCreate("EmuThread");
    const auto scope = core_context.Acquire();

    if (Settings::values.preload_textures) {
        emit LoadProgress(VideoCore::LoadCallbackStage::Preload, 0, 0);
        system.CustomTexManager().PreloadTextures(
            stop_run, [this](VideoCore::LoadCallbackStage stage, std::size_t value,
                             std::size_t total) { emit LoadProgress(stage, value, total); });
    }

    emit LoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);

    system.GPU().Renderer().Rasterizer()->LoadDiskResources(
        stop_run, [this](VideoCore::LoadCallbackStage stage, std::size_t value, std::size_t total) {
            emit LoadProgress(stage, value, total);
        });

    emit LoadProgress(VideoCore::LoadCallbackStage::Complete, 0, 0);

    core_context.MakeCurrent();

    if (system.frame_limiter.IsFrameAdvancing()) {
        // Usually the loading screen is hidden after the first frame is drawn. In this case
        // we hide it immediately as we need to wait for user input to start the emulation.
        emit HideLoadingScreen();
        system.frame_limiter.WaitOnce();
    }

    // Holds whether the cpu was running during the last iteration,
    // so that the DebugModeLeft signal can be emitted before the
    // next execution step.
    bool was_active = false;
    while (!stop_run) {
        if (running) {
            if (!was_active)
                emit DebugModeLeft();

            const Core::System::ResultStatus result = system.RunLoop();
            if (result == Core::System::ResultStatus::ShutdownRequested) {
                // Notify frontend we shutdown
                emit ErrorThrown(result, "");
                // End emulation execution
                break;
            }
            if (result != Core::System::ResultStatus::Success) {
                this->SetRunning(false);
                emit ErrorThrown(result, system.GetStatusDetails());
            }

            was_active = running || exec_step;
            if (!was_active && !stop_run)
                emit DebugModeEntered();
        } else if (exec_step) {
            if (!was_active)
                emit DebugModeLeft();

            exec_step = false;
            [[maybe_unused]] const Core::System::ResultStatus result = system.SingleStep();
            emit DebugModeEntered();
            yieldCurrentThread();

            was_active = false;
        } else {
            std::unique_lock lock{running_mutex};
            running_cv.wait(lock, [this] { return IsRunning() || exec_step || stop_run; });
        }
    }

    // Shutdown the core emulation
    system.Shutdown();

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadExit();
#endif
}

#ifdef ENABLE_OPENGL
static std::unique_ptr<QOpenGLContext> CreateQOpenGLContext(bool gles) {
    QSurfaceFormat format;
    if (gles) {
        format.setRenderableType(QSurfaceFormat::RenderableType::OpenGLES);
        format.setVersion(3, 2);
    } else {
        format.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
        format.setVersion(4, 3);
    }
    format.setProfile(QSurfaceFormat::CoreProfile);

    if (Settings::values.renderer_debug) {
        format.setOption(QSurfaceFormat::FormatOption::DebugContext);
    }

    // TODO: expose a setting for buffer value (ie default/single/double/triple)
    format.setSwapBehavior(QSurfaceFormat::DefaultSwapBehavior);
    format.setSwapInterval(0);

    auto context = std::make_unique<QOpenGLContext>();
    context->setFormat(format);
    if (!context->create()) {
        LOG_ERROR(Frontend, "Unable to create OpenGL context with GLES = {}", gles);
        return nullptr;
    }
    return context;
}

class OpenGLSharedContext : public Frontend::GraphicsContext {
public:
    /// Create the original context that should be shared from
    explicit OpenGLSharedContext() {
        // First, try to create a context with the requested type.
        context = CreateQOpenGLContext(Settings::values.use_gles.GetValue());
        if (context == nullptr) {
            // On failure, fall back to context with flipped type.
            context = CreateQOpenGLContext(!Settings::values.use_gles.GetValue());
            if (context == nullptr) {
                LOG_ERROR(Frontend, "Unable to create any OpenGL context.");
            }
        }

        offscreen_surface = std::make_unique<QOffscreenSurface>(nullptr);
        offscreen_surface->setFormat(context->format());
        offscreen_surface->create();
        surface = offscreen_surface.get();
    }

    /// Create the shared contexts for rendering and presentation
    explicit OpenGLSharedContext(QOpenGLContext* share_context, QSurface* main_surface) {

        // disable vsync for any shared contexts
        auto format = share_context->format();
        format.setSwapInterval(0);

        context = std::make_unique<QOpenGLContext>();
        context->setShareContext(share_context);
        context->setFormat(format);
        if (!context->create()) {
            LOG_ERROR(Frontend, "Unable to create shared OpenGL context");
        }

        surface = main_surface;
    }

    ~OpenGLSharedContext() {
        OpenGLSharedContext::DoneCurrent();
    }

    bool IsGLES() override {
        return context->format().renderableType() == QSurfaceFormat::RenderableType::OpenGLES;
    }

    void SwapBuffers() override {
        context->swapBuffers(surface);
    }

    void MakeCurrent() override {
        // We can't track the current state of the underlying context in this wrapper class because
        // Qt may make the underlying context not current for one reason or another. In particular,
        // the WebBrowser uses GL, so it seems to conflict if we aren't careful.
        // Instead of always just making the context current (which does not have any caching to
        // check if the underlying context is already current) we can check for the current context
        // in the thread local data by calling `currentContext()` and checking if its ours.
        if (QOpenGLContext::currentContext() != context.get()) {
            context->makeCurrent(surface);
        }
    }

    void DoneCurrent() override {
        if (QOpenGLContext::currentContext() == context.get()) {
            context->doneCurrent();
        }
    }

    QOpenGLContext* GetShareContext() const {
        return context.get();
    }

private:
    // Avoid using Qt parent system here since we might move the QObjects to new threads
    // As a note, this means we should avoid using slots/signals with the objects too
    std::unique_ptr<QOpenGLContext> context;
    std::unique_ptr<QOffscreenSurface> offscreen_surface{};
    QSurface* surface;
};
#endif

class DummyContext : public Frontend::GraphicsContext {};

class RenderWidget : public QWidget {
public:
    RenderWidget(GRenderWindow* parent) : QWidget(parent) {
        setMouseTracking(true);
        update();
    }

    virtual ~RenderWidget() = default;
};

#ifdef ENABLE_OPENGL
class OpenGLRenderWidget : public RenderWidget {
public:
    explicit OpenGLRenderWidget(GRenderWindow* parent, Core::System& system_, bool is_secondary)
        : RenderWidget(parent), system(system_), is_secondary(is_secondary) {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_PaintOnScreen);
        if (GetWindowSystemType() == Frontend::WindowSystemType::Wayland) {
            setAttribute(Qt::WA_DontCreateNativeAncestors);
        }
        windowHandle()->setSurfaceType(QWindow::OpenGLSurface);
    }

    void SetContext(std::unique_ptr<Frontend::GraphicsContext>&& context_) {
        context = std::move(context_);
    }

    void Present() {
        if (!isVisible()) {
            return;
        }
        if (!system.IsPoweredOn()) {
            return;
        }
        context->MakeCurrent();
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        system.GPU().Renderer().TryPresent(100, is_secondary);
        context->SwapBuffers();
        glFinish();
    }

    void paintEvent(QPaintEvent* event) override {
        Present();
        update();
    }

    QPaintEngine* paintEngine() const override {
        return nullptr;
    }

private:
    std::unique_ptr<Frontend::GraphicsContext> context{};
    Core::System& system;
    bool is_secondary;
};
#endif

#ifdef ENABLE_VULKAN
class VulkanRenderWidget : public RenderWidget {
public:
    explicit VulkanRenderWidget(GRenderWindow* parent) : RenderWidget(parent) {
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_PaintOnScreen);
        if (GetWindowSystemType() == Frontend::WindowSystemType::Wayland) {
            setAttribute(Qt::WA_DontCreateNativeAncestors);
        }
#ifdef __APPLE__
        windowHandle()->setSurfaceType(QWindow::MetalSurface);
#else
        windowHandle()->setSurfaceType(QWindow::VulkanSurface);
#endif
    }

    QPaintEngine* paintEngine() const override {
        return nullptr;
    }
};
#endif

#ifdef ENABLE_SOFTWARE_RENDERER
struct SoftwareRenderWidget : public RenderWidget {
    explicit SoftwareRenderWidget(GRenderWindow* parent, Core::System& system_)
        : RenderWidget(parent), system(system_) {}

    void Present() {
        if (!isVisible()) {
            return;
        }
        if (!system.IsPoweredOn()) {
            return;
        }

        using VideoCore::ScreenId;

        const auto layout{Layout::DefaultFrameLayout(width(), height(), false, false)};
        QPainter painter(this);

        const auto draw_screen = [&](ScreenId screen_id) {
            const auto rect =
                screen_id == ScreenId::TopLeft ? layout.top_screen : layout.bottom_screen;
            const QImage screen =
                LoadFramebuffer(screen_id).scaled(rect.GetWidth(), rect.GetHeight());
            painter.drawImage(rect.left, rect.top, screen);
        };

        painter.fillRect(rect(), qRgb(Settings::values.bg_red.GetValue() * 255,
                                      Settings::values.bg_green.GetValue() * 255,
                                      Settings::values.bg_blue.GetValue() * 255));
        draw_screen(ScreenId::TopLeft);
        draw_screen(ScreenId::Bottom);

        painter.end();
    }

    void paintEvent(QPaintEvent* event) override {
        Present();
        update();
    }

    QImage LoadFramebuffer(VideoCore::ScreenId screen_id) {
        const auto& renderer = static_cast<SwRenderer::RendererSoftware&>(system.GPU().Renderer());
        const auto& info = renderer.Screen(screen_id);
        const int width = static_cast<int>(info.width);
        const int height = static_cast<int>(info.height);
        QImage image{height, width, QImage::Format_RGBA8888};
        std::memcpy(image.bits(), info.pixels.data(), info.pixels.size());
        return image;
    }

private:
    Core::System& system;
};
#endif

static Frontend::WindowSystemType GetWindowSystemType() {
    // Determine WSI type based on Qt platform.
    const QString platform_name = QGuiApplication::platformName();
    if (platform_name == QStringLiteral("windows"))
        return Frontend::WindowSystemType::Windows;
    else if (platform_name == QStringLiteral("xcb"))
        return Frontend::WindowSystemType::X11;
    else if (platform_name == QStringLiteral("wayland") ||
             platform_name == QStringLiteral("wayland-egl"))
        return Frontend::WindowSystemType::Wayland;
    else if (platform_name == QStringLiteral("cocoa") || platform_name == QStringLiteral("ios"))
        return Frontend::WindowSystemType::MacOS;

    LOG_CRITICAL(Frontend, "Unknown Qt platform!");
    return Frontend::WindowSystemType::Windows;
}

static Frontend::EmuWindow::WindowSystemInfo GetWindowSystemInfo(QWindow* window) {
    Frontend::EmuWindow::WindowSystemInfo wsi;
    wsi.type = GetWindowSystemType();

    if (window) {
#if defined(WIN32)
        // Our Win32 Qt external doesn't have the private API.
        wsi.render_surface = reinterpret_cast<void*>(window->winId());
#elif defined(__APPLE__)
        wsi.render_surface = reinterpret_cast<void* (*)(id, SEL)>(objc_msgSend)(
            reinterpret_cast<id>(window->winId()), sel_registerName("layer"));
#else
        QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
        wsi.display_connection = pni->nativeResourceForWindow("display", window);
        if (wsi.type == Frontend::WindowSystemType::Wayland)
            wsi.render_surface = pni->nativeResourceForWindow("surface", window);
        else
            wsi.render_surface = reinterpret_cast<void*>(window->winId());
#endif
        wsi.render_surface_scale = static_cast<float>(window->devicePixelRatio());
    } else {
        wsi.render_surface = nullptr;
        wsi.render_surface_scale = 1.0f;
    }

    return wsi;
}

std::unique_ptr<Frontend::GraphicsContext> GRenderWindow::main_context;

GRenderWindow::GRenderWindow(QWidget* parent_, EmuThread* emu_thread_, Core::System& system_,
                             bool is_secondary_)
    : QWidget(parent_), EmuWindow(is_secondary_), emu_thread(emu_thread_), system{system_} {

    setAttribute(Qt::WA_AcceptTouchEvents);
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    this->setMouseTracking(true);
    strict_context_required = QGuiApplication::platformName() == QStringLiteral("wayland") ||
                              QGuiApplication::platformName() == QStringLiteral("wayland-egl");

    GMainWindow* parent = GetMainWindow();
    connect(this, &GRenderWindow::FirstFrameDisplayed, parent, &GMainWindow::OnLoadComplete);
}

GRenderWindow::~GRenderWindow() = default;

void GRenderWindow::MakeCurrent() {
    main_context->MakeCurrent();
}

void GRenderWindow::DoneCurrent() {
    main_context->DoneCurrent();
}

void GRenderWindow::PollEvents() {
    if (!first_frame) {
        first_frame = true;
        emit FirstFrameDisplayed();
    }
}

// On Qt 5.0+, this correctly gets the size of the framebuffer (pixels).
//
// Older versions get the window size (density independent pixels),
// and hence, do not support DPI scaling ("retina" displays).
// The result will be a viewport that is smaller than the extent of the window.
void GRenderWindow::OnFramebufferSizeChanged() {
    // Screen changes potentially incur a change in screen DPI, hence we should update the
    // framebuffer size
    const qreal pixel_ratio = windowPixelRatio();
    const u32 width = static_cast<u32>(this->width() * pixel_ratio);
    const u32 height = static_cast<u32>(this->height() * pixel_ratio);
    UpdateCurrentFramebufferLayout(width, height);
}

void GRenderWindow::BackupGeometry() {
    geometry = QWidget::saveGeometry();
}

void GRenderWindow::RestoreGeometry() {
    // We don't want to back up the geometry here (obviously)
    QWidget::restoreGeometry(geometry);
}

void GRenderWindow::restoreGeometry(const QByteArray& geometry) {
    // Make sure users of this class don't need to deal with backing up the geometry themselves
    QWidget::restoreGeometry(geometry);
    BackupGeometry();
}

QByteArray GRenderWindow::saveGeometry() {
    // If we are a top-level widget, store the current geometry
    // otherwise, store the last backup
    if (parent() == nullptr) {
        return QWidget::saveGeometry();
    }

    return geometry;
}

qreal GRenderWindow::windowPixelRatio() const {
    return devicePixelRatioF();
}

std::pair<u32, u32> GRenderWindow::ScaleTouch(const QPointF pos) const {
    const qreal pixel_ratio = windowPixelRatio();
    return {static_cast<u32>(std::max(std::round(pos.x() * pixel_ratio), qreal{0.0})),
            static_cast<u32>(std::max(std::round(pos.y() * pixel_ratio), qreal{0.0}))};
}

void GRenderWindow::closeEvent(QCloseEvent* event) {
    emit Closed();
    QWidget::closeEvent(event);
}

void GRenderWindow::keyPressEvent(QKeyEvent* event) {
    InputCommon::GetKeyboard()->PressKey(event->key());
}

void GRenderWindow::keyReleaseEvent(QKeyEvent* event) {
    InputCommon::GetKeyboard()->ReleaseKey(event->key());
}

void GRenderWindow::mousePressEvent(QMouseEvent* event) {
    if (event->source() == Qt::MouseEventSynthesizedBySystem) {
        return; // touch input is handled in TouchBeginEvent
    }

    auto pos = event->pos();
    if (event->button() == Qt::LeftButton) {
        const auto [x, y] = ScaleTouch(pos);
        this->TouchPressed(x, y);
    } else if (event->button() == Qt::RightButton) {
        InputCommon::GetMotionEmu()->BeginTilt(pos.x(), pos.y());
    }
    emit MouseActivity();
}

void GRenderWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->source() == Qt::MouseEventSynthesizedBySystem) {
        return; // touch input is handled in TouchUpdateEvent
    }

    auto pos = event->pos();
    const auto [x, y] = ScaleTouch(pos);
    this->TouchMoved(x, y);
    InputCommon::GetMotionEmu()->Tilt(pos.x(), pos.y());
    emit MouseActivity();
}

void GRenderWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->source() == Qt::MouseEventSynthesizedBySystem) {
        return; // touch input is handled in TouchEndEvent
    }

    if (event->button() == Qt::LeftButton)
        this->TouchReleased();
    else if (event->button() == Qt::RightButton)
        InputCommon::GetMotionEmu()->EndTilt();
    emit MouseActivity();
}

void GRenderWindow::TouchBeginEvent(const QTouchEvent* event) {
    // TouchBegin always has exactly one touch point, so take the .first()
    const auto [x, y] = ScaleTouch(event->points().first().position());
    this->TouchPressed(x, y);
}

void GRenderWindow::TouchUpdateEvent(const QTouchEvent* event) {
    QPointF pos;
    int active_points = 0;

    // average all active touch points
    for (const auto& tp : event->points()) {
        if (tp.state() & (Qt::TouchPointPressed | Qt::TouchPointMoved | Qt::TouchPointStationary)) {
            active_points++;
            pos += tp.position();
        }
    }

    pos /= active_points;

    const auto [x, y] = ScaleTouch(pos);
    this->TouchMoved(x, y);
}

void GRenderWindow::TouchEndEvent() {
    this->TouchReleased();
}

bool GRenderWindow::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::TouchBegin:
        TouchBeginEvent(static_cast<QTouchEvent*>(event));
        return true;
    case QEvent::TouchUpdate:
        TouchUpdateEvent(static_cast<QTouchEvent*>(event));
        return true;
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        TouchEndEvent();
        return true;
    default:
        break;
    }

    return QWidget::event(event);
}

void GRenderWindow::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    if (auto* keyboard = InputCommon::GetKeyboard(); keyboard) {
        keyboard->ReleaseAllKeys();
    }
    has_focus = false;
}

void GRenderWindow::focusInEvent(QFocusEvent* event) {
    QWidget::focusInEvent(event);
    has_focus = true;
}

void GRenderWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    OnFramebufferSizeChanged();
}

bool GRenderWindow::InitRenderTarget() {
    {
        // Create a dummy render widget so that Qt
        // places the render window at the correct position.
        const RenderWidget dummy_widget{this};
    }

    first_frame = false;

    const auto graphics_api = Settings::values.graphics_api.GetValue();
    switch (graphics_api) {
#ifdef ENABLE_SOFTWARE_RENDERER
    case Settings::GraphicsAPI::Software:
        InitializeSoftware();
        break;
#endif
#ifdef ENABLE_OPENGL
    case Settings::GraphicsAPI::OpenGL:
        if (!InitializeOpenGL() || !LoadOpenGL()) {
            return false;
        }
        break;
#endif
#ifdef ENABLE_VULKAN
    case Settings::GraphicsAPI::Vulkan:
        InitializeVulkan();
        break;
#endif
    default:
        LOG_CRITICAL(Frontend,
                     "Unknown or unsupported graphics API {}, falling back to available default",
                     graphics_api);
#ifdef ENABLE_OPENGL
        if (!InitializeOpenGL() || !LoadOpenGL()) {
            return false;
        }
#elif ENABLE_VULKAN
        InitializeVulkan();
#elif ENABLE_SOFTWARE_RENDERER
        InitializeSoftware();
#else
// TODO: Add a null renderer backend for this, perhaps.
#error "At least one renderer must be enabled."
#endif
        break;
    }

    // Update the Window System information with the new render target
    window_info = GetWindowSystemInfo(child_widget->windowHandle());

    child_widget->resize(Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight);

    layout()->addWidget(child_widget);
    // Reset minimum required size to avoid resizing issues on the main window after restarting.
    setMinimumSize(1, 1);

    resize(Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight);
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    OnFramebufferSizeChanged();
    BackupGeometry();

    return true;
}

void GRenderWindow::ReleaseRenderTarget() {
    if (child_widget) {
        layout()->removeWidget(child_widget);
        child_widget->deleteLater();
        child_widget = nullptr;
    }
    main_context.reset();
}

void GRenderWindow::CaptureScreenshot(u32 res_scale, const QString& screenshot_path) {
    auto& renderer = system.GPU().Renderer();
    if (res_scale == 0) {
        res_scale = renderer.GetResolutionScaleFactor();
    }

    const auto layout{Layout::FrameLayoutFromResolutionScale(res_scale, is_secondary)};
    screenshot_image = QImage(QSize(layout.width, layout.height), QImage::Format_RGB32);
    renderer.RequestScreenshot(
        screenshot_image.bits(),
        [this, screenshot_path](bool invert_y) {
            const std::string std_screenshot_path = screenshot_path.toStdString();
            if (screenshot_image.mirrored(false, invert_y).save(screenshot_path)) {
                LOG_INFO(Frontend, "Screenshot saved to \"{}\"", std_screenshot_path);
            } else {
                LOG_ERROR(Frontend, "Failed to save screenshot to \"{}\"", std_screenshot_path);
            }
        },
        layout);
}

void GRenderWindow::OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) {
    setMinimumSize(minimal_size.first, minimal_size.second);
}

#ifdef ENABLE_OPENGL
bool GRenderWindow::InitializeOpenGL() {
    if (!QOpenGLContext::supportsThreadedOpenGL()) {
        QMessageBox::warning(this, tr("OpenGL not available!"),
                             tr("OpenGL shared contexts are not supported."));
        return false;
    }

    // TODO: One of these flags might be interesting: WA_OpaquePaintEvent, WA_NoBackground,
    // WA_DontShowOnScreen, WA_DeleteOnClose
    auto child = new OpenGLRenderWidget(this, system, is_secondary);
    child_widget = child;
    child_widget->windowHandle()->create();

    if (!main_context) {
        main_context = std::make_unique<OpenGLSharedContext>();
    }

    auto child_context = CreateSharedContext();
    child->SetContext(std::move(child_context));

    auto format = child_widget->windowHandle()->format();
    format.setSwapInterval(Settings::values.use_vsync_new.GetValue());
    child_widget->windowHandle()->setFormat(format);

    return true;
}

static void* GetProcAddressGL(const char* name) {
    return reinterpret_cast<void*>(QOpenGLContext::currentContext()->getProcAddress(name));
}

bool GRenderWindow::LoadOpenGL() {
    auto context = CreateSharedContext();
    auto scope = context->Acquire();
    const auto gles = context->IsGLES();

    auto gl_load_func = gles ? gladLoadGLES2Loader : gladLoadGLLoader;
    if (!gl_load_func(GetProcAddressGL)) {
        QMessageBox::warning(
            this, tr("Error while initializing OpenGL!"),
            tr("Your GPU may not support OpenGL, or you do not have the latest graphics driver."));
        return false;
    }

    const QString renderer =
        QString::fromUtf8(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    if (!gles && !GLAD_GL_VERSION_4_3) {
        LOG_ERROR(Frontend, "GPU does not support OpenGL 4.3: {}", renderer.toStdString());
        QMessageBox::warning(this, tr("Error while initializing OpenGL 4.3!"),
                             tr("Your GPU may not support OpenGL 4.3, or you do not have the "
                                "latest graphics driver.<br><br>GL Renderer:<br>%1")
                                 .arg(renderer));
        return false;
    } else if (gles && !GLAD_GL_ES_VERSION_3_2) {
        LOG_ERROR(Frontend, "GPU does not support OpenGL ES 3.2: {}", renderer.toStdString());
        QMessageBox::warning(this, tr("Error while initializing OpenGL ES 3.2!"),
                             tr("Your GPU may not support OpenGL ES 3.2, or you do not have the "
                                "latest graphics driver.<br><br>GL Renderer:<br>%1")
                                 .arg(renderer));
        return false;
    }

    return true;
}
#endif

#ifdef ENABLE_VULKAN
void GRenderWindow::InitializeVulkan() {
    auto child = new VulkanRenderWidget(this);
    child_widget = child;
    child_widget->windowHandle()->create();
    main_context = std::make_unique<DummyContext>();
}
#endif

#ifdef ENABLE_SOFTWARE_RENDERER
void GRenderWindow::InitializeSoftware() {
    child_widget = new SoftwareRenderWidget(this, system);
    main_context = std::make_unique<DummyContext>();
}
#endif

void GRenderWindow::OnEmulationStarting(EmuThread* emu_thread) {
    this->emu_thread = emu_thread;
}

void GRenderWindow::OnEmulationStopping() {
    emu_thread = nullptr;
}

void GRenderWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

std::unique_ptr<Frontend::GraphicsContext> GRenderWindow::CreateSharedContext() const {
#ifdef ENABLE_OPENGL
    const auto graphics_api = Settings::values.graphics_api.GetValue();
    if (graphics_api == Settings::GraphicsAPI::OpenGL) {
        auto gl_context = static_cast<OpenGLSharedContext*>(main_context.get());
        // Bind the shared contexts to the main surface in case the backend wants to take over
        // presentation
        return std::make_unique<OpenGLSharedContext>(gl_context->GetShareContext(),
                                                     child_widget->windowHandle());
    }
#endif
    return std::make_unique<DummyContext>();
}
