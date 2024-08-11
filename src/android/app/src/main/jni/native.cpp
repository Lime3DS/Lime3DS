// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <codecvt>
#include <thread>
#include <dlfcn.h>

#include <android/api-level.h>
#include <android/native_window_jni.h>
#include <core/hw/aes/key.h>
#include <core/loader/smdh.h>
#include <core/system_titles.h>

#include <core/hle/service/cfg/cfg.h>
#include "audio_core/dsp_interface.h"
#include "common/arch.h"
#if CITRA_ARCH(arm64)
#include "common/aarch64/cpu_detect.h"
#elif CITRA_ARCH(x86_64)
#include "common/x64/cpu_detect.h"
#endif
#include "common/common_paths.h"
#include "common/dynamic_library/dynamic_library.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend/applets/default_applets.h"
#include "core/frontend/camera/factory.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/loader/loader.h"
#include "core/savestate.h"
#include "jni/android_common/android_common.h"
#include "jni/applets/mii_selector.h"
#include "jni/applets/swkbd.h"
#include "jni/camera/ndk_camera.h"
#include "jni/camera/still_image_camera.h"
#include "jni/config.h"
#ifdef ENABLE_OPENGL
#include "jni/emu_window/emu_window_gl.h"
#endif
#ifdef ENABLE_VULKAN
#include "jni/emu_window/emu_window_vk.h"
#endif
#include "jni/game_settings.h"
#include "jni/id_cache.h"
#include "jni/input_manager.h"
#include "jni/ndk_motion.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

#if defined(ENABLE_VULKAN) && CITRA_ARCH(arm64)
#include <adrenotools/driver.h>
#endif

namespace {

ANativeWindow* s_surf;

std::shared_ptr<Common::DynamicLibrary> vulkan_library{};
std::unique_ptr<EmuWindow_Android> window;

std::atomic<bool> stop_run{true};
std::atomic<bool> pause_emulation{false};

std::mutex paused_mutex;
std::mutex running_mutex;
std::condition_variable running_cv;

} // Anonymous namespace

static jobject ToJavaCoreError(Core::System::ResultStatus result) {
    static const std::map<Core::System::ResultStatus, const char*> CoreErrorNameMap{
        {Core::System::ResultStatus::ErrorSystemFiles, "ErrorSystemFiles"},
        {Core::System::ResultStatus::ErrorSavestate, "ErrorSavestate"},
        {Core::System::ResultStatus::ErrorArticDisconnected, "ErrorArticDisconnected"},
        {Core::System::ResultStatus::ErrorUnknown, "ErrorUnknown"},
    };

    const auto name = CoreErrorNameMap.count(result) ? CoreErrorNameMap.at(result) : "ErrorUnknown";

    JNIEnv* env = IDCache::GetEnvForThread();
    const jclass core_error_class = IDCache::GetCoreErrorClass();
    return env->GetStaticObjectField(
        core_error_class,
        env->GetStaticFieldID(core_error_class, name,
                              "Lio/github/lime3ds/android/NativeLibrary$CoreError;"));
}

static bool HandleCoreError(Core::System::ResultStatus result, const std::string& details) {
    JNIEnv* env = IDCache::GetEnvForThread();
    return env->CallStaticBooleanMethod(IDCache::GetNativeLibraryClass(), IDCache::GetOnCoreError(),
                                        ToJavaCoreError(result),
                                        env->NewStringUTF(details.c_str())) != JNI_FALSE;
}

static void LoadDiskCacheProgress(VideoCore::LoadCallbackStage stage, int progress, int max) {
    JNIEnv* env = IDCache::GetEnvForThread();
    env->CallStaticVoidMethod(IDCache::GetDiskCacheProgressClass(),
                              IDCache::GetDiskCacheLoadProgress(),
                              IDCache::GetJavaLoadCallbackStage(stage), static_cast<jint>(progress),
                              static_cast<jint>(max));
}

static Camera::NDK::Factory* g_ndk_factory{};

static void TryShutdown() {
    if (!window) {
        return;
    }

    window->DoneCurrent();
    Core::System::GetInstance().Shutdown();
    window.reset();
    InputManager::Shutdown();
    MicroProfileShutdown();
}

static bool CheckMicPermission() {
    return IDCache::GetEnvForThread()->CallStaticBooleanMethod(IDCache::GetNativeLibraryClass(),
                                                               IDCache::GetRequestMicPermission());
}

static Core::System::ResultStatus RunCitra(const std::string& filepath) {
    // Lime3DS core only supports a single running instance
    std::scoped_lock lock(running_mutex);

    LOG_INFO(Frontend, "Lime3DS starting...");

    MicroProfileOnThreadCreate("EmuThread");

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return Core::System::ResultStatus::ErrorLoader;
    }

    Core::System& system{Core::System::GetInstance()};

    const auto graphics_api = Settings::values.graphics_api.GetValue();
    switch (graphics_api) {
#ifdef ENABLE_OPENGL
    case Settings::GraphicsAPI::OpenGL:
        window = std::make_unique<EmuWindow_Android_OpenGL>(system, s_surf);
        break;
#endif
#ifdef ENABLE_VULKAN
    case Settings::GraphicsAPI::Vulkan:
        window = std::make_unique<EmuWindow_Android_Vulkan>(s_surf, vulkan_library);
        break;
#endif
    default:
        LOG_CRITICAL(Frontend,
                     "Unknown or unsupported graphics API {}, falling back to available default",
                     graphics_api);
#ifdef ENABLE_OPENGL
        window = std::make_unique<EmuWindow_Android_OpenGL>(system, s_surf);
#elif ENABLE_VULKAN
        window = std::make_unique<EmuWindow_Android_Vulkan>(s_surf, vulkan_library);
#else
        // TODO: Add a null renderer backend for this, perhaps.
#error "At least one renderer must be enabled."
#endif
        break;
    }

    // Forces a config reload on game boot, if the user changed settings in the UI
    Config{};
    // Replace with game-specific settings
    u64 program_id{};
    FileUtil::SetCurrentRomPath(filepath);
    auto app_loader = Loader::GetLoader(filepath);
    if (app_loader) {
        app_loader->ReadProgramId(program_id);
        system.RegisterAppLoaderEarly(app_loader);
        GameSettings::LoadOverrides(program_id);
    }
    system.ApplySettings();
    Settings::LogSettings();

    Camera::RegisterFactory("image", std::make_unique<Camera::StillImage::Factory>());

    auto ndk_factory = std::make_unique<Camera::NDK::Factory>();
    g_ndk_factory = ndk_factory.get();
    Camera::RegisterFactory("ndk", std::move(ndk_factory));

    // Register frontend applets
    Frontend::RegisterDefaultApplets(system);
    system.RegisterMiiSelector(std::make_shared<MiiSelector::AndroidMiiSelector>());
    system.RegisterSoftwareKeyboard(std::make_shared<SoftwareKeyboard::AndroidKeyboard>());

    // Register microphone permission check
    system.RegisterMicPermissionCheck(&CheckMicPermission);

    Pica::g_debug_context = Pica::DebugContext::Construct();
    InputManager::Init();

    window->MakeCurrent();
    const Core::System::ResultStatus load_result{system.Load(*window, filepath)};
    if (load_result != Core::System::ResultStatus::Success) {
        return load_result;
    }

    stop_run = false;
    pause_emulation = false;

    LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);

    std::unique_ptr<Frontend::GraphicsContext> cpu_context;
    system.GPU().Renderer().Rasterizer()->LoadDiskResources(stop_run, &LoadDiskCacheProgress);

    LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Complete, 0, 0);

    SCOPE_EXIT({ TryShutdown(); });

    // Start running emulation
    while (!stop_run) {
        if (!pause_emulation) {
            const auto result = system.RunLoop();
            if (result == Core::System::ResultStatus::Success) {
                continue;
            }
            if (result == Core::System::ResultStatus::ShutdownRequested) {
                return result; // This also exits the emulation activity
            } else {
                InputManager::NDKMotionHandler()->DisableSensors();
                if (!HandleCoreError(result, system.GetStatusDetails())) {
                    // Frontend requests us to abort
                    // If the error was an Artic disconnect, return shutdown request.
                    if (result == Core::System::ResultStatus::ErrorArticDisconnected) {
                        return Core::System::ResultStatus::ShutdownRequested;
                    }
                    return result;
                }
                InputManager::NDKMotionHandler()->EnableSensors();
            }
        } else {
            // Ensure no audio bleeds out while game is paused
            const float volume = Settings::values.volume.GetValue();
            SCOPE_EXIT({ Settings::values.volume = volume; });
            Settings::values.volume = 0;

            std::unique_lock pause_lock{paused_mutex};
            running_cv.wait(pause_lock, [] { return !pause_emulation || stop_run; });
            window->PollEvents();
        }
    }

    return Core::System::ResultStatus::Success;
}

void EnableAdrenoTurboMode(bool enable) {
#if defined(ENABLE_VULKAN) && CITRA_ARCH(arm64)
    adrenotools_set_turbo(enable);
#endif
}

void InitializeGpuDriver(const std::string& hook_lib_dir, const std::string& custom_driver_dir,
                         const std::string& custom_driver_name,
                         const std::string& file_redirect_dir) {
#if defined(ENABLE_VULKAN) && CITRA_ARCH(arm64)
    void* handle{};
    const char* file_redirect_dir_{};
    int featureFlags{};

    // Enable driver file redirection when renderer debugging is enabled.
    if (Settings::values.renderer_debug && file_redirect_dir.size()) {
        featureFlags |= ADRENOTOOLS_DRIVER_FILE_REDIRECT;
        file_redirect_dir_ = file_redirect_dir.c_str();
    }

    // Try to load a custom driver.
    if (custom_driver_name.size()) {
        handle = adrenotools_open_libvulkan(
            RTLD_NOW, featureFlags | ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hook_lib_dir.c_str(),
            custom_driver_dir.c_str(), custom_driver_name.c_str(), file_redirect_dir_, nullptr);
    }

    // Try to load the system driver.
    if (!handle) {
        handle = adrenotools_open_libvulkan(RTLD_NOW, featureFlags, nullptr, hook_lib_dir.c_str(),
                                            nullptr, nullptr, file_redirect_dir_, nullptr);
    }

    vulkan_library = std::make_shared<Common::DynamicLibrary>(handle);
#endif
}

extern "C" {

void Java_io_github_lime3ds_android_NativeLibrary_surfaceChanged(JNIEnv* env,
                                                                 [[maybe_unused]] jobject obj,
                                                                 jobject surf) {
    s_surf = ANativeWindow_fromSurface(env, surf);

    bool notify = false;
    if (window) {
        notify = window->OnSurfaceChanged(s_surf);
    }

    auto& system = Core::System::GetInstance();
    if (notify && system.IsPoweredOn()) {
        system.GPU().Renderer().NotifySurfaceChanged();
    }

    LOG_INFO(Frontend, "Surface changed");
}

void Java_io_github_lime3ds_android_NativeLibrary_surfaceDestroyed([[maybe_unused]] JNIEnv* env,
                                                                   [[maybe_unused]] jobject obj) {
    ANativeWindow_release(s_surf);
    s_surf = nullptr;
    if (window) {
        window->OnSurfaceChanged(s_surf);
    }
}

void Java_io_github_lime3ds_android_NativeLibrary_doFrame([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jobject obj) {
    if (stop_run || pause_emulation) {
        return;
    }
    if (window) {
        window->TryPresenting();
    }
}

void JNICALL Java_io_github_lime3ds_android_NativeLibrary_initializeGpuDriver(
    JNIEnv* env, jobject obj, jstring hook_lib_dir, jstring custom_driver_dir,
    jstring custom_driver_name, jstring file_redirect_dir) {
    InitializeGpuDriver(GetJString(env, hook_lib_dir), GetJString(env, custom_driver_dir),
                        GetJString(env, custom_driver_name), GetJString(env, file_redirect_dir));
}

void JNICALL Java_io_github_lime3ds_android_NativeLibrary_enableAdrenoTurboMode(JNIEnv* env,
                                                                                jobject obj,
                                                                                jboolean enable) {
    EnableAdrenoTurboMode(enable);
}

void Java_io_github_lime3ds_android_NativeLibrary_updateFramebuffer([[maybe_unused]] JNIEnv* env,
                                                                    [[maybe_unused]] jobject obj,
                                                                    jboolean is_portrait_mode) {
    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        system.GPU().Renderer().UpdateCurrentFramebufferLayout(is_portrait_mode);
    }
}

void Java_io_github_lime3ds_android_NativeLibrary_swapScreens([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jobject obj,
                                                              jboolean swap_screens,
                                                              jint rotation) {
    Settings::values.swap_screen = swap_screens;
    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        system.GPU().Renderer().UpdateCurrentFramebufferLayout(!(rotation % 2));
    }
    InputManager::screen_rotation = rotation;
    Camera::NDK::g_rotation = rotation;
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_areKeysAvailable(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    HW::AES::InitKeys();
    return HW::AES::IsKeyXAvailable(HW::AES::KeySlotID::NCCHSecure1) &&
           HW::AES::IsKeyXAvailable(HW::AES::KeySlotID::NCCHSecure2);
}

jstring Java_io_github_lime3ds_android_NativeLibrary_getHomeMenuPath(JNIEnv* env,
                                                                     [[maybe_unused]] jobject obj,
                                                                     jint region) {
    const std::string path = Core::GetHomeMenuNcchPath(region);
    if (FileUtil::Exists(path)) {
        return ToJString(env, path);
    }
    return ToJString(env, "");
}

void Java_io_github_lime3ds_android_NativeLibrary_setUserDirectory(JNIEnv* env,
                                                                   [[maybe_unused]] jobject obj,
                                                                   jstring j_directory) {
    FileUtil::SetCurrentDir(GetJString(env, j_directory));
}

jobjectArray Java_io_github_lime3ds_android_NativeLibrary_getInstalledGamePaths(
    JNIEnv* env, [[maybe_unused]] jclass clazz) {
    std::vector<std::string> games;
    const FileUtil::DirectoryEntryCallable ScanDir =
        [&games, &ScanDir](u64*, const std::string& directory, const std::string& virtual_name) {
            std::string path = directory + virtual_name;
            if (FileUtil::IsDirectory(path)) {
                path += '/';
                FileUtil::ForeachDirectoryEntry(nullptr, path, ScanDir);
            } else {
                if (!FileUtil::Exists(path))
                    return false;
                auto loader = Loader::GetLoader(path);
                if (loader) {
                    bool executable{};
                    const Loader::ResultStatus result = loader->IsExecutable(executable);
                    if (Loader::ResultStatus::Success == result && executable) {
                        games.emplace_back(path);
                    }
                }
            }
            return true;
        };
    ScanDir(nullptr, "",
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) +
                "Nintendo "
                "3DS/00000000000000000000000000000000/"
                "00000000000000000000000000000000/title/00040000");
    ScanDir(nullptr, "",
            FileUtil::GetUserPath(FileUtil::UserPath::NANDDir) +
                "00000000000000000000000000000000/title/00040010");
    jobjectArray jgames = env->NewObjectArray(static_cast<jsize>(games.size()),
                                              env->FindClass("java/lang/String"), nullptr);
    for (jsize i = 0; i < games.size(); ++i)
        env->SetObjectArrayElement(jgames, i, env->NewStringUTF(games[i].c_str()));
    return jgames;
}

jlongArray Java_io_github_lime3ds_android_NativeLibrary_getSystemTitleIds(
    JNIEnv* env, [[maybe_unused]] jobject obj, jint system_type, jint region) {
    const auto mode = static_cast<Core::SystemTitleSet>(system_type);
    const std::vector<u64> titles = Core::GetSystemTitleIds(mode, region);
    jlongArray jTitles = env->NewLongArray(titles.size());
    env->SetLongArrayRegion(jTitles, 0, titles.size(),
                            reinterpret_cast<const jlong*>(titles.data()));
    return jTitles;
}

jobject Java_io_github_lime3ds_android_NativeLibrary_downloadTitleFromNus(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, jlong title) {
    const auto title_id = static_cast<u64>(title);
    Service::AM::InstallStatus status = Service::AM::InstallFromNus(title_id);
    if (status != Service::AM::InstallStatus::Success) {
        return IDCache::GetJavaCiaInstallStatus(status);
    }
    return IDCache::GetJavaCiaInstallStatus(Service::AM::InstallStatus::Success);
}

[[maybe_unused]] static bool CheckKgslPresent() {
    constexpr auto KgslPath{"/dev/kgsl-3d0"};

    return access(KgslPath, F_OK) == 0;
}

[[maybe_unused]] bool SupportsCustomDriver() {
    return android_get_device_api_level() >= 28 && CheckKgslPresent();
}

jboolean JNICALL Java_io_github_lime3ds_android_utils_GpuDriverHelper_supportsCustomDriverLoading(
    JNIEnv* env, jobject instance) {
#ifdef CITRA_ARCH_arm64
    // If the KGSL device exists custom drivers can be loaded using adrenotools
    return SupportsCustomDriver();
#else
    return false;
#endif
}

// TODO(xperia64): ensure these cannot be called in an invalid state (e.g. after StopEmulation)
void Java_io_github_lime3ds_android_NativeLibrary_unPauseEmulation([[maybe_unused]] JNIEnv* env,
                                                                   [[maybe_unused]] jobject obj) {
    pause_emulation = false;
    running_cv.notify_all();
    InputManager::NDKMotionHandler()->EnableSensors();
}

void Java_io_github_lime3ds_android_NativeLibrary_pauseEmulation([[maybe_unused]] JNIEnv* env,
                                                                 [[maybe_unused]] jobject obj) {
    pause_emulation = true;
    InputManager::NDKMotionHandler()->DisableSensors();
}

void Java_io_github_lime3ds_android_NativeLibrary_stopEmulation([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jobject obj) {
    stop_run = true;
    pause_emulation = false;
    window->StopPresenting();
    running_cv.notify_all();
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_isRunning([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jobject obj) {
    return static_cast<jboolean>(!stop_run);
}

jlong Java_io_github_lime3ds_android_NativeLibrary_getRunningTitleId([[maybe_unused]] JNIEnv* env,
                                                                     [[maybe_unused]] jobject obj) {
    u64 title_id{};
    Core::System::GetInstance().GetAppLoader().ReadProgramId(title_id);
    return static_cast<jlong>(title_id);
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_onGamePadEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, [[maybe_unused]] jstring j_device,
    jint j_button, jint action) {
    bool consumed{};
    if (action) {
        consumed = InputManager::ButtonHandler()->PressKey(j_button);
    } else {
        consumed = InputManager::ButtonHandler()->ReleaseKey(j_button);
    }

    return static_cast<jboolean>(consumed);
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_onGamePadMoveEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, [[maybe_unused]] jstring j_device,
    jint axis, jfloat x, jfloat y) {
    // Clamp joystick movement to supported minimum and maximum
    // Citra uses an inverted y axis sent by the frontend
    x = std::clamp(x, -1.f, 1.f);
    y = std::clamp(-y, -1.f, 1.f);

    // Clamp the input to a circle (while touch input is already clamped in the frontend, gamepad is
    // unknown)
    float r = x * x + y * y;
    if (r > 1.0f) {
        r = std::sqrt(r);
        x /= r;
        y /= r;
    }
    return static_cast<jboolean>(InputManager::AnalogHandler()->MoveJoystick(axis, x, y));
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_onGamePadAxisEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, [[maybe_unused]] jstring j_device,
    jint axis_id, jfloat axis_val) {
    return static_cast<jboolean>(
        InputManager::ButtonHandler()->AnalogButtonEvent(axis_id, axis_val));
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_onTouchEvent([[maybe_unused]] JNIEnv* env,
                                                                   [[maybe_unused]] jobject obj,
                                                                   jfloat x, jfloat y,
                                                                   jboolean pressed) {
    return static_cast<jboolean>(
        window->OnTouchEvent(static_cast<int>(x + 0.5), static_cast<int>(y + 0.5), pressed));
}

void Java_io_github_lime3ds_android_NativeLibrary_onTouchMoved([[maybe_unused]] JNIEnv* env,
                                                               [[maybe_unused]] jobject obj,
                                                               jfloat x, jfloat y) {
    window->OnTouchMoved((int)x, (int)y);
}

jlong Java_io_github_lime3ds_android_NativeLibrary_getTitleId(JNIEnv* env,
                                                              [[maybe_unused]] jobject obj,
                                                              jstring j_filename) {
    std::string filepath = GetJString(env, j_filename);
    const auto loader = Loader::GetLoader(filepath);

    u64 title_id{};
    if (loader) {
        loader->ReadProgramId(title_id);
    }
    return static_cast<jlong>(title_id);
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_getIsSystemTitle(JNIEnv* env,
                                                                       [[maybe_unused]] jobject obj,
                                                                       jstring path) {
    const std::string filepath = GetJString(env, path);
    const auto loader = Loader::GetLoader(filepath);

    // Since we also read through invalid file extensions, we have to check if the loader is valid
    if (loader == nullptr) {
        return false;
    }

    u64 program_id = 0;
    loader->ReadProgramId(program_id);
    return ((program_id >> 32) & 0xFFFFFFFF) == 0x00040010;
}

void Java_io_github_lime3ds_android_NativeLibrary_createConfigFile([[maybe_unused]] JNIEnv* env,
                                                                   [[maybe_unused]] jobject obj) {
    Config{};
}

void Java_io_github_lime3ds_android_NativeLibrary_createLogFile([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jobject obj) {
    Common::Log::Initialize();
    Common::Log::Start();
    LOG_INFO(Frontend, "Logging backend initialised");
}

void Java_io_github_lime3ds_android_NativeLibrary_logUserDirectory(JNIEnv* env,
                                                                   [[maybe_unused]] jobject obj,
                                                                   jstring j_path) {
    std::string_view path = env->GetStringUTFChars(j_path, 0);
    LOG_INFO(Frontend, "User directory path: {}", path);
    env->ReleaseStringUTFChars(j_path, path.data());
}

void Java_io_github_lime3ds_android_NativeLibrary_reloadSettings([[maybe_unused]] JNIEnv* env,
                                                                 [[maybe_unused]] jobject obj) {
    Config{};
    Core::System& system{Core::System::GetInstance()};

    // Replace with game-specific settings
    if (system.IsPoweredOn()) {
        u64 program_id{};
        system.GetAppLoader().ReadProgramId(program_id);
        GameSettings::LoadOverrides(program_id);
    }

    system.ApplySettings();
}

jdoubleArray Java_io_github_lime3ds_android_NativeLibrary_getPerfStats(
    JNIEnv* env, [[maybe_unused]] jobject obj) {
    auto& core = Core::System::GetInstance();
    jdoubleArray j_stats = env->NewDoubleArray(4);

    if (core.IsPoweredOn()) {
        auto results = core.GetAndResetPerfStats();

        // Converting the structure into an array makes it easier to pass it to the frontend
        double stats[4] = {results.system_fps, results.game_fps, results.frametime,
                           results.emulation_speed};

        env->SetDoubleArrayRegion(j_stats, 0, 4, stats);
    }

    return j_stats;
}

void Java_io_github_lime3ds_android_NativeLibrary_run__Ljava_lang_String_2(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring j_path) {
    const std::string path = GetJString(env, j_path);

    if (!stop_run) {
        stop_run = true;
        running_cv.notify_all();
    }

    const Core::System::ResultStatus result{RunCitra(path)};
    if (result != Core::System::ResultStatus::Success) {
        env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                  IDCache::GetExitEmulationActivity(), static_cast<int>(result));
    }
}

void Java_io_github_lime3ds_android_NativeLibrary_reloadCameraDevices(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    if (g_ndk_factory) {
        g_ndk_factory->ReloadCameraDevices();
    }
}

jboolean Java_io_github_lime3ds_android_NativeLibrary_loadAmiibo(JNIEnv* env,
                                                                 [[maybe_unused]] jobject obj,
                                                                 jstring j_file) {
    std::string filepath = GetJString(env, j_file);
    Core::System& system{Core::System::GetInstance()};
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc == nullptr) {
        return static_cast<jboolean>(false);
    }

    return static_cast<jboolean>(nfc->LoadAmiibo(filepath));
}

void Java_io_github_lime3ds_android_NativeLibrary_removeAmiibo([[maybe_unused]] JNIEnv* env,
                                                               [[maybe_unused]] jobject obj) {
    Core::System& system{Core::System::GetInstance()};
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc == nullptr) {
        return;
    }

    nfc->RemoveAmiibo();
}

JNIEXPORT jobject JNICALL Java_io_github_lime3ds_android_utils_CiaInstallWorker_installCIA(
    JNIEnv* env, jobject jobj, jstring jpath) {
    std::string path = GetJString(env, jpath);
    Service::AM::InstallStatus res = Service::AM::InstallCIA(
        path, [env, jobj](std::size_t total_bytes_read, std::size_t file_size) {
            env->CallVoidMethod(jobj, IDCache::GetCiaInstallHelperSetProgress(),
                                static_cast<jint>(file_size), static_cast<jint>(total_bytes_read));
        });

    return IDCache::GetJavaCiaInstallStatus(res);
}

jobjectArray Java_io_github_lime3ds_android_NativeLibrary_getSavestateInfo(
    JNIEnv* env, [[maybe_unused]] jobject obj) {
    const jclass date_class = env->FindClass("java/util/Date");
    const auto date_constructor = env->GetMethodID(date_class, "<init>", "(J)V");

    const jclass savestate_info_class = IDCache::GetSavestateInfoClass();
    const auto slot_field = env->GetFieldID(savestate_info_class, "slot", "I");
    const auto date_field = env->GetFieldID(savestate_info_class, "time", "Ljava/util/Date;");

    const Core::System& system{Core::System::GetInstance()};
    if (!system.IsPoweredOn()) {
        return nullptr;
    }

    u64 title_id;
    if (system.GetAppLoader().ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        return nullptr;
    }

    const auto savestates = Core::ListSaveStates(title_id, system.Movie().GetCurrentMovieID());
    const jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(savestates.size()), savestate_info_class, nullptr);
    for (std::size_t i = 0; i < savestates.size(); ++i) {
        const jobject object = env->AllocObject(savestate_info_class);
        env->SetIntField(object, slot_field, static_cast<jint>(savestates[i].slot));
        env->SetObjectField(object, date_field,
                            env->NewObject(date_class, date_constructor,
                                           static_cast<jlong>(savestates[i].time * 1000)));

        env->SetObjectArrayElement(array, i, object);
    }
    return array;
}

void Java_io_github_lime3ds_android_NativeLibrary_saveState([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jobject obj,
                                                            jint slot) {
    Core::System::GetInstance().SendSignal(Core::System::Signal::Save, slot);
}

void Java_io_github_lime3ds_android_NativeLibrary_loadState([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jobject obj,
                                                            jint slot) {
    Core::System::GetInstance().SendSignal(Core::System::Signal::Load, slot);
}

void Java_io_github_lime3ds_android_NativeLibrary_logDeviceInfo([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jobject obj) {
    LOG_INFO(Frontend, "Lime3DS Version: {} | {}-{}", Common::g_build_fullname,
             Common::g_scm_branch, Common::g_scm_desc);
    LOG_INFO(Frontend, "Host CPU: {}", Common::GetCPUCaps().cpu_string);
    // There is no decent way to get the OS version, so we log the API level instead.
    LOG_INFO(Frontend, "Host OS: Android API level {}", android_get_device_api_level());
}

} // extern "C"
