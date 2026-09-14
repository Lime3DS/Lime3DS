// Copyright 2019-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
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
#include "common/file_derived.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/play_time_manager.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/frontend/applets/default_applets.h"
#include "core/frontend/camera/factory.h"
#include "core/guest_shutdown.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hw/unique_data.h"
#include "core/loader/loader.h"
#include "core/savestate.h"
#include "core/system_titles.h"
#include "jni/android_common/android_common.h"
#include "jni/applets/mii_selector.h"
#include "jni/applets/swkbd.h"
#include "jni/camera/ndk_camera.h"
#include "jni/camera/still_image_camera.h"
#include "jni/config.h"
#include "network/announce_multiplayer_session.h"

#ifdef ENABLE_OPENGL
#include "jni/emu_window/emu_window_gl.h"
#endif

#ifdef ENABLE_VULKAN
#include "jni/emu_window/emu_window_vk.h"
#if CITRA_ARCH(arm64)
#include <adrenotools/driver.h>
#endif
#endif

#include "common/android_utils.h"
#include "jni/id_cache.h"
#include "jni/input_manager.h"
#include "jni/ndk_motion.h"
#include "multiplayer.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

namespace {

ANativeWindow* s_surface;
ANativeWindow* s_secondary_surface;

enum class CompressionStatus : jint {
    Success = 0,
    Compress_Unsupported = 1,
    Compress_AlreadyCompressed = 2,
    Compress_Failed = 3,
    Decompress_Unsupported = 4,
    Decompress_NotCompressed = 5,
    Decompress_Failed = 6,
    Installed_Application = 7,
};

std::shared_ptr<Common::DynamicLibrary> vulkan_library{};
std::unique_ptr<EmuWindow_Android> window;
std::unique_ptr<EmuWindow_Android> secondary_window;

std::unique_ptr<PlayTime::PlayTimeManager> play_time_manager;
jlong ptm_current_title_id = std::numeric_limits<jlong>::max(); // Arbitrary default value

std::atomic<bool> stop_run{true};
std::atomic<bool> pause_emulation{false};

std::mutex paused_mutex;
std::mutex running_mutex;
std::condition_variable running_cv;

// Signaled once the emulation thread leaves RunCitra, so a stop can wait for the guest.
std::atomic<bool> emulation_finished{true};
std::mutex emulation_finished_mutex;
std::condition_variable emulation_finished_cv;

// Guards the lifetime of s_surface/s_secondary_surface and the (re)creation of the
// EmuWindow_Android and renderer objects that consume them. Android may destroy or replace the
// Surface passed to us (on rotation, or if the fragment hosting the SurfaceView is torn
// down and recreated) from the UI thread at any time, including while the renderer is still
// being constructed on the emulation thread in RunCitra(). Without this lock, the UI thread can
// release/replace the ANativeWindow while it is concurrently being used to create the
// initial Vulkan/EGL surface, resulting in a use-after-free. A recursive mutex is needed
// due to the locking pattern used in RunCitra().
std::recursive_mutex surface_mutex;

// Signalled by surfaceChanged() whenever s_surface goes from null to non-null. Used by the
// System::Init() callback to block the renderer (re)creation (initial boot or a
// savestate load) until a surface actually exists, instead of giving
// VideoCore a null ANativeWindow.
std::condition_variable_any surface_cv;

std::string inserted_cartridge;

// Android Multiplayer which can be initialized with parameters
std::unique_ptr<AndroidMultiplayer> multiplayer{nullptr};
std::shared_ptr<Network::AnnounceMultiplayerSession> announce_multiplayer_session;

} // Anonymous namespace

static jobject ToJavaCoreError(Core::System::ResultStatus result) {
    static const std::map<Core::System::ResultStatus, const char*> CoreErrorNameMap{
        {Core::System::ResultStatus::ErrorSystemFiles, "ErrorSystemFiles"},
        {Core::System::ResultStatus::ErrorSavestate, "ErrorSavestate"},
        {Core::System::ResultStatus::ErrorArticDisconnected, "ErrorArticDisconnected"},
        {Core::System::ResultStatus::ErrorN3DSApplication, "ErrorN3DSApplication"},
        {Core::System::ResultStatus::ErrorCoreExceptionRaised, "ErrorCoreExceptionRaised"},
        {Core::System::ResultStatus::ErrorSavestateBuildMismatch, "ErrorSavestateBuildMismatch"},
        {Core::System::ResultStatus::ErrorUnknown, "ErrorUnknown"},
    };

    const auto name = CoreErrorNameMap.count(result) ? CoreErrorNameMap.at(result) : "ErrorUnknown";

    JNIEnv* env = IDCache::GetEnvForThread();
    const jclass core_error_class = IDCache::GetCoreErrorClass();
    return env->GetStaticObjectField(
        core_error_class, env->GetStaticFieldID(core_error_class, name,
                                                "Lorg/citra/citra_emu/NativeLibrary$CoreError;"));
}

static bool HandleCoreError(Core::System::ResultStatus result, const std::string& details) {
    JNIEnv* env = IDCache::GetEnvForThread();
    return env->CallStaticBooleanMethod(IDCache::GetNativeLibraryClass(), IDCache::GetOnCoreError(),
                                        ToJavaCoreError(result),
                                        env->NewStringUTF(details.c_str())) != JNI_FALSE;
}

static void LoadDiskCacheProgress(VideoCore::LoadCallbackStage stage, int progress, int max,
                                  const std::string& object) {
    JNIEnv* env = IDCache::GetEnvForThread();
    env->CallStaticVoidMethod(IDCache::GetDiskCacheProgressClass(),
                              IDCache::GetDiskCacheLoadProgress(),
                              IDCache::GetJavaLoadCallbackStage(stage), static_cast<jint>(progress),
                              static_cast<jint>(max), env->NewStringUTF(object.c_str()));
}

static Camera::NDK::Factory* g_ndk_factory{};

static void TryShutdown() {
    std::scoped_lock surface_lock(surface_mutex);

    if (!window) {
        return;
    }

    window->DoneCurrent();
    if (secondary_window) {
        secondary_window->DoneCurrent();
    }

    Core::System& system{Core::System::GetInstance()};

    system.Shutdown();
    system.EjectCartridge();

    window.reset();
    if (secondary_window) {
        secondary_window.reset();
    }

    InputManager::Shutdown();
    MicroProfileShutdown();
}

static bool CheckMicPermission() {
    return IDCache::GetEnvForThread()->CallStaticBooleanMethod(IDCache::GetNativeLibraryClass(),
                                                               IDCache::GetRequestMicPermission());
}

static Core::System::ResultStatus RunCitra(const std::string& filepath) {
    // Citra core only supports a single running instance
    std::scoped_lock lock(running_mutex);

    LOG_INFO(Frontend, "Azahar starting...");

    MicroProfileOnThreadCreate("EmuThread");

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "Failed to load ROM: No ROM specified");
        return Core::System::ResultStatus::ErrorLoader;
    }

    Core::System& system{Core::System::GetInstance()};

    if (!inserted_cartridge.empty()) {
        system.InsertCartridge(inserted_cartridge);
    }

    // Acquired for the whole window/renderer construction below (and released again once
    // system.Load() has finished setting up the renderer), so that a concurrent
    // surfaceChanged/surfaceDestroyed callback from the UI thread cannot release or replace
    // s_surface/s_secondary_surface while they're still being used.
    std::unique_lock<std::recursive_mutex> surface_lock(surface_mutex);

    // We also need to lock the surface mutex when System::Init() is called.
    // This is because save state saving and loading may call System::Init
    // on its own which does GPU reinitialization.
    // This is why a recursive mutex is needed, as System::Load() also calls
    // System::Init() which in this RunCitra() function would result in a deadlock.
    system.RegisterOnInitCallback([](bool init_start) {
        if (init_start) {
            surface_mutex.lock();
            // A savestate load re-enters here later, well after surface_lock in RunCitra() has
            // already been released. If a rotation destroyed s_surface just before this callback
            // acquired the lock, wait here for surfaceChanged() to hand us a new one rather than
            // proceeding into CreateSurface() with a null window. During the very first call
            // (initial boot) this never actually blocks, since surfaceChanged/surfaceDestroyed
            // cannot run concurrently here, this same thread still holds surface_lock above
            // for the whole boot sequence.
            if (!s_surface) {
                std::unique_lock<std::recursive_mutex> wait_lock(surface_mutex, std::adopt_lock);
                surface_cv.wait(wait_lock, [] { return s_surface != nullptr; });
                wait_lock.release();
            }
        } else {
            surface_mutex.unlock();
        }
    });

    const auto graphics_api = Settings::GetWorkingGraphicsAPI();
    EGLContext* shared_context;
    switch (graphics_api) {
#ifdef ENABLE_OPENGL
    case Settings::GraphicsAPI::OpenGL:
        window = std::make_unique<EmuWindow_Android_OpenGL>(system, s_surface, false);
        shared_context = window->GetEGLContext();
        secondary_window = std::make_unique<EmuWindow_Android_OpenGL>(system, s_secondary_surface,
                                                                      true, shared_context);
        break;
#endif
#ifdef ENABLE_VULKAN
    case Settings::GraphicsAPI::Vulkan:
        window = std::make_unique<EmuWindow_Android_Vulkan>(s_surface, vulkan_library, false);
        secondary_window =
            std::make_unique<EmuWindow_Android_Vulkan>(s_secondary_surface, vulkan_library, true);
        break;
#endif
    default:
        LOG_CRITICAL(Frontend,
                     "Unknown or unsupported graphics API {}, falling back to available default",
                     graphics_api);
#ifdef ENABLE_OPENGL
        window = std::make_unique<EmuWindow_Android_OpenGL>(system, s_surface, false);
        shared_context = window->GetEGLContext();
        secondary_window = std::make_unique<EmuWindow_Android_OpenGL>(system, s_secondary_surface,
                                                                      true, shared_context);

#elif ENABLE_VULKAN
        window = std::make_unique<EmuWindow_Android_Vulkan>(s_surface, vulkan_library, false);
        secondary_window =
            std::make_unique<EmuWindow_Android_Vulkan>(s_secondary_surface, vulkan_library, true);
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

    // No PICA debugging on Android
    if (Settings::values.pica_debugging) {
        Pica::g_debug_context = Pica::DebugContext::Construct();
    } else {
        Pica::g_debug_context.reset();
    }

    InputManager::Init();

    window->MakeCurrent();

    const Core::System::ResultStatus load_result{
        system.Load(*window, filepath, secondary_window.get())};

    // At this point, the surface has already been used, so the mutex can be unlocked.
    surface_lock.unlock();

    if (load_result != Core::System::ResultStatus::Success) {
        return load_result;
    }

    stop_run = false;
    pause_emulation = false;

    LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0, "");

    system.GPU().ApplyPerProgramSettings(program_id);

    std::unique_ptr<Frontend::GraphicsContext> cpu_context;
    system.GPU().Renderer().Rasterizer()->LoadDefaultDiskResources(stop_run,
                                                                   &LoadDiskCacheProgress);

    LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Complete, 0, 0, "");

    SCOPE_EXIT({ TryShutdown(); });

    system.RegisterCoreLoopThreadId();

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
                auto* handler = InputManager::NDKMotionHandler();
                if (handler) {
                    handler->DisableSensors();
                }
                if (!HandleCoreError(result, system.GetStatusDetails())) {
                    // Frontend requests us to abort, return a shutdown request.
                    return Core::System::ResultStatus::ShutdownRequested;
                }
                handler = InputManager::NDKMotionHandler();
                if (handler) {
                    handler->EnableSensors();
                }
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

void Java_org_citra_citra_1emu_NativeLibrary_surfaceChanged(JNIEnv* env,
                                                            [[maybe_unused]] jobject obj,
                                                            jobject surf) {
    std::scoped_lock lock(surface_mutex);

    if (s_surface) {
        ANativeWindow_release(s_surface);
        s_surface = nullptr;
    }
    s_surface = ANativeWindow_fromSurface(env, surf);
    if (!s_surface) {
        LOG_WARNING(Frontend, "Surface changed, but failed to acquire the native window");
        return;
    }
    // Wake up any System::Init() currently blocked waiting for a live surface
    // (can happen when loading savestates and the screen rotates).
    surface_cv.notify_all();

    bool notify = false;
    if (window) {
        notify = window->OnSurfaceChanged(s_surface);
    }

    auto& system = Core::System::GetInstance();
    if (notify && system.IsPoweredOn()) {
        system.GPU().Renderer().NotifySurfaceChanged(false);
    }

    LOG_INFO(Frontend, "Surface changed");
}

void Java_org_citra_citra_1emu_NativeLibrary_secondarySurfaceChanged(JNIEnv* env,
                                                                     [[maybe_unused]] jobject obj,
                                                                     jobject surf) {
    std::scoped_lock lock(surface_mutex);

    auto& system = Core::System::GetInstance();

    if (s_secondary_surface) {
        ANativeWindow_release(s_secondary_surface);
        s_secondary_surface = nullptr;
    }
    s_secondary_surface = ANativeWindow_fromSurface(env, surf);
    if (!s_secondary_surface) {
        return;
    }

    bool notify = false;
    if (secondary_window) {
        // Second window already created, so update it
        notify = secondary_window->OnSurfaceChanged(s_secondary_surface);

        // Log the dimensions for debugging
        int32_t width = ANativeWindow_getWidth(s_secondary_surface);
        int32_t height = ANativeWindow_getHeight(s_secondary_surface);
        LOG_INFO(Frontend, "Secondary Surface changed to {}x{}", width, height);
    } else {
        LOG_WARNING(Frontend,
                    "Second Window does not exist in native.cpp but surface changed. Ignoring.");
    }

    if (notify && system.IsPoweredOn()) {
        system.GPU().Renderer().NotifySurfaceChanged(true);
    }

    LOG_INFO(Frontend, "Secondary Surface changed");
}

void Java_org_citra_citra_1emu_NativeLibrary_secondarySurfaceDestroyed(
    JNIEnv* env, [[maybe_unused]] jobject obj) {
    std::scoped_lock lock(surface_mutex);

    if (s_secondary_surface != nullptr) {
        ANativeWindow_release(s_secondary_surface);
        s_secondary_surface = nullptr;
    }

    LOG_INFO(Frontend, "Secondary Surface Destroyed");
}

void Java_org_citra_citra_1emu_NativeLibrary_surfaceDestroyed([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jobject obj) {
    std::scoped_lock lock(surface_mutex);

    if (s_surface != nullptr) {
        ANativeWindow_release(s_surface);
        s_surface = nullptr;
        if (window) {
            window->OnSurfaceChanged(s_surface);
        }
    }
}

void Java_org_citra_citra_1emu_NativeLibrary_doFrame([[maybe_unused]] JNIEnv* env,
                                                     [[maybe_unused]] jobject obj) {
    if (stop_run || pause_emulation) {
        return;
    }
    if (window) {
        window->TryPresenting();
    }
    if (secondary_window) {
        secondary_window->TryPresenting();
    }
}

void JNICALL Java_org_citra_citra_1emu_NativeLibrary_initializeGpuDriver(
    JNIEnv* env, jobject obj, jstring hook_lib_dir, jstring custom_driver_dir,
    jstring custom_driver_name, jstring file_redirect_dir) {
    InitializeGpuDriver(GetJString(env, hook_lib_dir), GetJString(env, custom_driver_dir),
                        GetJString(env, custom_driver_name), GetJString(env, file_redirect_dir));
}

void Java_org_citra_citra_1emu_NativeLibrary_notifyOrientationChange([[maybe_unused]] JNIEnv* env,
                                                                     [[maybe_unused]] jobject obj,
                                                                     jint layout_option,
                                                                     jint rotation,
                                                                     jboolean portrait) {
    Settings::values.layout_option = static_cast<Settings::LayoutOption>(layout_option);
}
void Java_org_citra_citra_1emu_NativeLibrary_updateFramebuffer([[maybe_unused]] JNIEnv* env,
                                                               [[maybe_unused]] jobject obj,
                                                               jboolean is_portrait_mode) {
    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        system.GPU().Renderer().UpdateCurrentFramebufferLayout(is_portrait_mode);
    }
}

void Java_org_citra_citra_1emu_NativeLibrary_swapScreens([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jobject obj,
                                                         jboolean swap_screens, jint rotation) {
    Settings::values.swap_screen = swap_screens;
    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        system.GPU().Renderer().UpdateCurrentFramebufferLayout(AndroidUtils::IsPortraitMode());
    }
    InputManager::screen_rotation = rotation;
    Camera::NDK::g_rotation = rotation;
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_areKeysAvailable([[maybe_unused]] JNIEnv* env,
                                                                  [[maybe_unused]] jobject obj) {
    HW::AES::InitKeys();
    return HW::AES::IsKeyXAvailable(HW::AES::KeySlotID::NCCHSecure1) &&
           HW::AES::IsKeyXAvailable(HW::AES::KeySlotID::NCCHSecure2);
}

jstring Java_org_citra_citra_1emu_NativeLibrary_getHomeMenuPath(JNIEnv* env,
                                                                [[maybe_unused]] jobject obj,
                                                                jint region) {
    const std::string path = Core::GetHomeMenuNcchPath(region);
    if (FileUtil::Exists(path)) {
        return ToJString(env, path);
    }
    return ToJString(env, "");
}

static CompressionStatus GetCompressFileInfo(Loader::AppLoader::CompressFileInfo& out_info,
                                             size_t& out_frame_size, const std::string& filepath,
                                             bool compress) {

    if (Service::FS::IsInstalledApplication(filepath)) {
        return CompressionStatus::Installed_Application;
    }

    Loader::AppLoader::CompressFileInfo compress_info{};
    compress_info.is_supported = false;
    size_t frame_size{};
    auto loader = Loader::GetLoader(filepath);
    if (loader) {
        compress_info = loader->GetCompressFileInfo();
        frame_size = FileUtil::Z3DSWriteIOFile::DEFAULT_FRAME_SIZE;
    } else {
        bool is_compressed = false;
        if (Service::AM::CheckCIAToInstall(filepath, is_compressed, compress ? true : false) ==
            Service::AM::InstallStatus::Success) {
            compress_info.is_supported = true;
            compress_info.is_compressed = is_compressed;
            compress_info.recommended_compressed_extension = "zcia";
            compress_info.recommended_uncompressed_extension = "cia";
            compress_info.underlying_magic = std::array<u8, 4>({'C', 'I', 'A', '\0'});
            frame_size = FileUtil::Z3DSWriteIOFile::DEFAULT_CIA_FRAME_SIZE;
            if (compress) {
                auto meta_info = Service::AM::GetCIAInfos(filepath);
                if (meta_info.Succeeded()) {
                    const auto& meta_info_val = meta_info.Unwrap();
                    std::vector<u8> value(sizeof(Service::AM::TitleInfo));
                    memcpy(value.data(), &meta_info_val.first, sizeof(Service::AM::TitleInfo));
                    compress_info.default_metadata.emplace("titleinfo", value);
                    if (meta_info_val.second) {
                        value.resize(sizeof(Loader::SMDH));
                        memcpy(value.data(), meta_info_val.second.get(), sizeof(Loader::SMDH));
                        compress_info.default_metadata.emplace("smdh", value);
                    }
                }
            }
        }
    }

    if (!compress_info.is_supported) {
        LOG_ERROR(Frontend,
                  "Error {} file {}, the selected file is not a compatible 3DS ROM format or is "
                  "encrypted.",
                  compress ? "compressing" : "decompressing", filepath);
        return compress ? CompressionStatus::Compress_Unsupported
                        : CompressionStatus::Decompress_Unsupported;
    }
    if (compress_info.is_compressed && compress) {
        LOG_ERROR(Frontend, "Error compressing file {}, the selected file is already compressed",
                  filepath);
        return CompressionStatus::Compress_AlreadyCompressed;
    }
    if (!compress_info.is_compressed && !compress) {
        LOG_ERROR(Frontend,
                  "Error decompressing file {}, the selected file is already decompressed",
                  filepath);
        return CompressionStatus::Decompress_NotCompressed;
    }

    out_info = compress_info;
    out_frame_size = frame_size;
    return CompressionStatus::Success;
}

jint Java_org_citra_citra_1emu_NativeLibrary_compressFileNative(JNIEnv* env, jobject obj,
                                                                jstring j_input_path,
                                                                jstring j_output_path) {
    const std::string input_path = GetJString(env, j_input_path);
    const std::string output_path = GetJString(env, j_output_path);

    Loader::AppLoader::CompressFileInfo compress_info{};
    size_t frame_size{};
    CompressionStatus stat = GetCompressFileInfo(compress_info, frame_size, input_path, true);
    if (stat != CompressionStatus::Success) {
        return static_cast<jint>(stat);
    }

    auto progress = [](std::size_t processed, std::size_t total) {
        JNIEnv* env = IDCache::GetEnvForThread();
        env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                  IDCache::GetCompressProgressMethod(), static_cast<jlong>(total),
                                  static_cast<jlong>(processed));
    };

    bool success =
        FileUtil::CompressZ3DSFile(input_path, output_path, compress_info.underlying_magic,
                                   frame_size, progress, compress_info.default_metadata);
    if (!success) {
        FileUtil::Delete(output_path);
        return static_cast<jint>(CompressionStatus::Compress_Failed);
    }

    return static_cast<jint>(CompressionStatus::Success);
}

jint Java_org_citra_citra_1emu_NativeLibrary_decompressFileNative(JNIEnv* env, jobject obj,
                                                                  jstring j_input_path,
                                                                  jstring j_output_path) {
    const std::string input_path = GetJString(env, j_input_path);
    const std::string output_path = GetJString(env, j_output_path);

    Loader::AppLoader::CompressFileInfo compress_info{};
    size_t frame_size{};
    CompressionStatus stat = GetCompressFileInfo(compress_info, frame_size, input_path, false);
    if (stat != CompressionStatus::Success) {
        return static_cast<jint>(stat);
    }

    auto progress = [](std::size_t processed, std::size_t total) {
        JNIEnv* env = IDCache::GetEnvForThread();
        env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                  IDCache::GetCompressProgressMethod(), static_cast<jlong>(total),
                                  static_cast<jlong>(processed));
    };

    bool success = FileUtil::DeCompressZ3DSFile(input_path, output_path, progress);
    if (!success) {
        FileUtil::Delete(output_path);
        return static_cast<jint>(CompressionStatus::Decompress_Failed);
    }

    return static_cast<jint>(CompressionStatus::Success);
}

jstring Java_org_citra_citra_1emu_NativeLibrary_getRecommendedExtension(
    JNIEnv* env, jobject obj, jstring j_input_path, jboolean j_should_compress) {
    const std::string input_path = GetJString(env, j_input_path);

    std::string compressed_ext;
    std::string uncompressed_ext;

    auto loader = Loader::GetLoader(input_path);
    if (loader) {
        auto compress_info = loader->GetCompressFileInfo();
        if (compress_info.is_supported) {
            compressed_ext = compress_info.recommended_compressed_extension;
            uncompressed_ext = compress_info.recommended_uncompressed_extension;
        }
    } else {
        bool is_compressed = false;
        if (Service::AM::CheckCIAToInstall(input_path, is_compressed, true) ==
            Service::AM::InstallStatus::Success) {
            compressed_ext = "zcia";
            uncompressed_ext = "cia";
        }
    }

    if (compressed_ext.empty()) {
        return env->NewStringUTF("");
    }

    return env->NewStringUTF(j_should_compress ? compressed_ext.c_str() : uncompressed_ext.c_str());
}

void Java_org_citra_citra_1emu_NativeLibrary_setUserDirectory(JNIEnv* env,
                                                              [[maybe_unused]] jobject obj,
                                                              jstring j_directory) {
    FileUtil::SetCurrentDir(GetJString(env, j_directory));
}

jobjectArray Java_org_citra_citra_1emu_NativeLibrary_getInstalledGamePathsImpl(
    JNIEnv* env, [[maybe_unused]] jclass clazz) {
    std::vector<std::string> games;
    Service::FS::MediaType media_type;
    const FileUtil::DirectoryEntryCallable ScanDir = [&games, &ScanDir, &media_type](
                                                         u64*, const std::string& directory,
                                                         const std::string& virtual_name) {
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
                    games.emplace_back(path + "|" + std::to_string(static_cast<int>(media_type)));
                }
            }
        }
        return true;
    };
    media_type = Service::FS::MediaType::SDMC;
    ScanDir(nullptr, "",
            FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir) +
                "Nintendo "
                "3DS/00000000000000000000000000000000/"
                "00000000000000000000000000000000/title/00040000");
    media_type = Service::FS::MediaType::NAND;
    ScanDir(nullptr, "",
            FileUtil::GetUserPath(FileUtil::UserPath::NANDDir) +
                "00000000000000000000000000000000/title/00040010");
    jobjectArray jgames = env->NewObjectArray(static_cast<jsize>(games.size()),
                                              env->FindClass("java/lang/String"), nullptr);
    for (jsize i = 0; i < games.size(); ++i)
        env->SetObjectArrayElement(jgames, i, env->NewStringUTF(games[i].c_str()));
    return jgames;
}

jlongArray Java_org_citra_citra_1emu_NativeLibrary_getSystemTitleIds(JNIEnv* env,
                                                                     [[maybe_unused]] jobject obj,
                                                                     jint system_type,
                                                                     jint region) {
    const auto mode = static_cast<Core::SystemTitleSet>(system_type);
    const std::vector<u64> titles = Core::GetSystemTitleIds(mode, region);
    jlongArray jTitles = env->NewLongArray(titles.size());
    env->SetLongArrayRegion(jTitles, 0, titles.size(),
                            reinterpret_cast<const jlong*>(titles.data()));
    return jTitles;
}

jbooleanArray Java_org_citra_citra_1emu_NativeLibrary_areSystemTitlesInstalled(
    JNIEnv* env, [[maybe_unused]] jobject obj) {
    const auto installed = Core::AreSystemTitlesInstalled();
    jbooleanArray jInstalled = env->NewBooleanArray(2);
    jboolean* elements = env->GetBooleanArrayElements(jInstalled, nullptr);

    elements[0] = installed.first ? JNI_TRUE : JNI_FALSE;
    elements[1] = installed.second ? JNI_TRUE : JNI_FALSE;

    env->ReleaseBooleanArrayElements(jInstalled, elements, 0);

    return jInstalled;
}

void Java_org_citra_citra_1emu_NativeLibrary_uninstallSystemFiles(JNIEnv* env,
                                                                  [[maybe_unused]] jobject obj,
                                                                  jboolean old3ds) {
    Core::UninstallSystemFiles(old3ds ? Core::SystemTitleSet::Old3ds
                                      : Core::SystemTitleSet::New3ds);
}

[[maybe_unused]] static bool CheckKgslPresent() {
    constexpr auto KgslPath{"/dev/kgsl-3d0"};

    return access(KgslPath, F_OK) == 0;
}

[[maybe_unused]] bool SupportsCustomDriver() {
    return android_get_device_api_level() >= 28 && CheckKgslPresent();
}

jboolean JNICALL Java_org_citra_citra_1emu_utils_GpuDriverHelper_supportsCustomDriverLoading(
    JNIEnv* env, jobject instance) {
#ifdef CITRA_ARCH_arm64
    // If the KGSL device exists custom drivers can be loaded using adrenotools
    return SupportsCustomDriver();
#else
    return false;
#endif
}

// TODO(xperia64): ensure these cannot be called in an invalid state (e.g. after StopEmulation)
void Java_org_citra_citra_1emu_NativeLibrary_unPauseEmulation([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jobject obj) {
    pause_emulation = false;
    running_cv.notify_all();
    auto* handler = InputManager::NDKMotionHandler();
    if (handler) {
        handler->EnableSensors();
    }
}

void Java_org_citra_citra_1emu_NativeLibrary_pauseEmulation([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jobject obj) {
    pause_emulation = true;
    auto* handler = InputManager::NDKMotionHandler();
    if (handler) {
        handler->DisableSensors();
    }
}

// stopEmulation() runs on the UI thread, so this is a freeze budget, not a save budget. It also
// caps the save data copy, which is otherwise as large as the title's save directory.
constexpr auto GUEST_SHUTDOWN_TIMEOUT = std::chrono::milliseconds(1000);
// Android kills an app that stops answering input for 5s. The copy is charged against this, so
// the whole call returns within the ceiling however long the copy takes.
constexpr auto GUEST_SHUTDOWN_MID_SAVE_CEILING = std::chrono::milliseconds(2500);

/// Android side of Core::PerformGuestShutdown(); only the wait differs from
/// GMainWindow::RequestGuestShutdown() in src/citra_qt/citra_qt.cpp.
static void RequestGuestShutdown() {
    Core::System& system{Core::System::GetInstance()};
    if (stop_run || !system.IsPoweredOn()) {
        return;
    }

    const Core::GuestShutdownTimeouts timeouts{GUEST_SHUTDOWN_TIMEOUT,
                                               GUEST_SHUTDOWN_MID_SAVE_CEILING};
    Core::PerformGuestShutdown(
        system, timeouts,
        [](std::chrono::milliseconds slice) {
            std::unique_lock lock{emulation_finished_mutex};
            return emulation_finished_cv.wait_for(lock, slice,
                                                  [] { return emulation_finished.load(); });
        },
        [] {
            // A paused loop never sees the notification; paused_mutex keeps the wakeup from being
            // missed.
            {
                std::scoped_lock pause_guard{paused_mutex};
                pause_emulation = false;
            }
            running_cv.notify_all();
        });
}

void Java_org_citra_citra_1emu_NativeLibrary_stopEmulation([[maybe_unused]] JNIEnv* env,
                                                           [[maybe_unused]] jobject obj) {
    RequestGuestShutdown();

    if (stop_run.exchange(true)) {
        // stop_run was already true
        return;
    }
    pause_emulation = false;
    if (window) {
        window->StopPresenting();
    }
    if (secondary_window) {
        secondary_window->StopPresenting();
    }
    running_cv.notify_all();
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_isRunning([[maybe_unused]] JNIEnv* env,
                                                           [[maybe_unused]] jobject obj) {
    return static_cast<jboolean>(!stop_run);
}

jlong Java_org_citra_citra_1emu_NativeLibrary_getRunningTitleId([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jobject obj) {
    u64 title_id{};
    Core::System::GetInstance().GetAppLoader().ReadProgramId(title_id);
    return static_cast<jlong>(title_id);
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_onGamePadEvent([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jobject obj,
                                                                [[maybe_unused]] jstring j_device,
                                                                jint j_button, jint action) {
    bool consumed{};
    if (action) {
        consumed = InputManager::ButtonHandler()->PressKey(j_button);
    } else {
        consumed = InputManager::ButtonHandler()->ReleaseKey(j_button);
    }

    return static_cast<jboolean>(consumed);
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_onGamePadMoveEvent(
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

jboolean Java_org_citra_citra_1emu_NativeLibrary_onGamePadAxisEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, [[maybe_unused]] jstring j_device,
    jint axis_id, jfloat axis_val) {
    return static_cast<jboolean>(
        InputManager::ButtonHandler()->AnalogButtonEvent(axis_id, axis_val));
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_onTouchEvent([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jobject obj,
                                                              jfloat x, jfloat y,
                                                              jboolean pressed) {
    return static_cast<jboolean>(
        window->OnTouchEvent(static_cast<int>(x + 0.5), static_cast<int>(y + 0.5), pressed));
}

void Java_org_citra_citra_1emu_NativeLibrary_onTouchMoved([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jobject obj, jfloat x,
                                                          jfloat y) {
    window->OnTouchMoved((int)x, (int)y);
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_onSecondaryTouchEvent([[maybe_unused]] JNIEnv* env,
                                                                       [[maybe_unused]] jobject obj,
                                                                       jfloat x, jfloat y,
                                                                       jboolean pressed) {
    if (!secondary_window) {
        return JNI_FALSE;
    }
    return static_cast<jboolean>(secondary_window->OnTouchEvent(
        static_cast<int>(x + 0.5), static_cast<int>(y + 0.5), pressed));
}

void Java_org_citra_citra_1emu_NativeLibrary_onSecondaryTouchMoved([[maybe_unused]] JNIEnv* env,
                                                                   [[maybe_unused]] jobject obj,
                                                                   jfloat x, jfloat y) {
    if (secondary_window) {
        secondary_window->OnTouchMoved((int)x, (int)y);
    }
}

jlong Java_org_citra_citra_1emu_NativeLibrary_getTitleId(JNIEnv* env, [[maybe_unused]] jobject obj,
                                                         jstring j_filename) {
    std::string filepath = GetJString(env, j_filename);
    const auto loader = Loader::GetLoader(filepath);

    u64 title_id{};
    if (loader) {
        loader->ReadProgramId(title_id);
    }
    return static_cast<jlong>(title_id);
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_getIsSystemTitle(JNIEnv* env,
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

void Java_org_citra_citra_1emu_NativeLibrary_createConfigFile([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jobject obj) {
    Config{};
}

void Java_org_citra_citra_1emu_NativeLibrary_createLogFile([[maybe_unused]] JNIEnv* env,
                                                           [[maybe_unused]] jobject obj) {
    Common::Log::Initialize();
    Common::Log::Start();
    LOG_INFO(Frontend, "Logging backend initialised");
}

void Java_org_citra_citra_1emu_NativeLibrary_logUserDirectory(JNIEnv* env,
                                                              [[maybe_unused]] jobject obj,
                                                              jstring j_path) {
    std::string_view path = env->GetStringUTFChars(j_path, 0);
    LOG_INFO(Frontend, "User directory path: {}", path);
    env->ReleaseStringUTFChars(j_path, path.data());
}

void Java_org_citra_citra_1emu_NativeLibrary_reloadSettings([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jobject obj) {
    Config{};
    Core::System& system{Core::System::GetInstance()};

    // Replace with game-specific settings
    if (system.IsPoweredOn()) {
        u64 program_id{};
        system.GetAppLoader().ReadProgramId(program_id);
    }

    if (multiplayer) {
        multiplayer->UpdateCredentials();
    }

    system.ApplySettings();
}

jdoubleArray Java_org_citra_citra_1emu_NativeLibrary_getPerfStats(JNIEnv* env,
                                                                  [[maybe_unused]] jobject obj) {
    auto& core = Core::System::GetInstance();
    jdoubleArray j_stats = env->NewDoubleArray(9);

    if (core.IsPoweredOn()) {
        auto results = core.GetAndResetPerfStats();

        // Converting the structure into an array makes it easier to pass it to the frontend
        double stats[9] = {results.system_fps,      results.game_fps,
                           results.emulation_speed, results.time_vblank_interval,
                           results.time_hle_svc,    results.time_hle_ipc,
                           results.time_gpu,        results.time_swap,
                           results.time_remaining};

        env->SetDoubleArrayRegion(j_stats, 0, 9, stats);
    }

    return j_stats;
}

void Java_org_citra_citra_1emu_NativeLibrary_run__Ljava_lang_String_2(JNIEnv* env,
                                                                      [[maybe_unused]] jobject obj,
                                                                      jstring j_path) {
    const std::string path = GetJString(env, j_path);

    if (!stop_run) {
        stop_run = true;
        running_cv.notify_all();
    }

    {
        std::scoped_lock lock{emulation_finished_mutex};
        emulation_finished = false;
    }
    const Core::System::ResultStatus result{RunCitra(path)};
    {
        std::scoped_lock lock{emulation_finished_mutex};
        emulation_finished = true;
    }
    emulation_finished_cv.notify_all();

    if (result != Core::System::ResultStatus::Success) {
        env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                  IDCache::GetExitEmulationActivity(), static_cast<int>(result));
    }
}

void Java_org_citra_citra_1emu_NativeLibrary_reloadCameraDevices([[maybe_unused]] JNIEnv* env,
                                                                 [[maybe_unused]] jobject obj) {
    if (g_ndk_factory) {
        g_ndk_factory->ReloadCameraDevices();
    }
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_loadAmiibo(JNIEnv* env,
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

void Java_org_citra_citra_1emu_NativeLibrary_removeAmiibo([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jobject obj) {
    Core::System& system{Core::System::GetInstance()};
    Service::SM::ServiceManager& sm = system.ServiceManager();
    auto nfc = sm.GetService<Service::NFC::Module::Interface>("nfc:u");
    if (nfc == nullptr) {
        return;
    }

    nfc->RemoveAmiibo();
}

// init multiplayer class
JNIEXPORT void JNICALL
Java_org_citra_citra_1emu_NativeLibrary_initMultiplayer(JNIEnv* env, [[maybe_unused]] jobject obj) {
    if (multiplayer) {
        return;
    }

    announce_multiplayer_session = std::make_shared<Network::AnnounceMultiplayerSession>(
        Service::CFG::GetUsername(Core::System::GetInstance()));

    multiplayer = std::make_unique<AndroidMultiplayer>(Core::System::GetInstance(),
                                                       announce_multiplayer_session);
    multiplayer->NetworkInit();
}

JNIEXPORT jobjectArray JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayGetPublicRooms(
    JNIEnv* env, [[maybe_unused]] jobject obj) {
    return ToJStringArray(env, multiplayer->NetPlayGetPublicRooms());
}

JNIEXPORT jint JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayCreateRoom(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring ipaddress, jint port, jstring username,
    jstring preferedGameName, jlong preferedGameId, jstring password, jstring room_name,
    jint max_players) {
    return static_cast<jint>(multiplayer->NetPlayCreateRoom(
        GetJString(env, ipaddress), port, GetJString(env, username),
        GetJString(env, preferedGameName), preferedGameId, GetJString(env, password),
        GetJString(env, room_name), max_players));
}

JNIEXPORT jint JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayJoinRoom(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring ipaddress, jint port, jstring username,
    jstring password) {
    return static_cast<jint>(multiplayer->NetPlayJoinRoom(
        GetJString(env, ipaddress), port, GetJString(env, username), GetJString(env, password)));
}

JNIEXPORT jobjectArray JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayRoomInfo(
    JNIEnv* env, [[maybe_unused]] jobject obj) {
    return ToJStringArray(env, multiplayer->NetPlayRoomInfo());
}

JNIEXPORT jboolean JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayIsJoined(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return multiplayer->NetPlayIsJoined();
}

JNIEXPORT jboolean JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayIsHostedRoom(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return multiplayer->NetPlayIsHostedRoom();
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlaySendMessage(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring msg) {
    multiplayer->NetPlaySendMessage(GetJString(env, msg));
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayKickUser(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring username) {
    multiplayer->NetPlayKickUser(GetJString(env, username));
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayLeaveRoom(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    multiplayer->NetPlayLeaveRoom();
}

JNIEXPORT jboolean JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayIsModerator(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return multiplayer->NetPlayIsModerator();
}

JNIEXPORT jobjectArray JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayGetBanList(
    JNIEnv* env, [[maybe_unused]] jobject obj) {
    return ToJStringArray(env, multiplayer->NetPlayGetBanList());
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayBanUser(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring username) {
    multiplayer->NetPlayBanUser(GetJString(env, username));
}

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_utils_NetPlayManager_netPlayUnbanUser(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring username) {
    multiplayer->NetPlayUnbanUser(GetJString(env, username));
}

JNIEXPORT jobject JNICALL Java_org_citra_citra_1emu_utils_CiaInstallWorker_installCIA(
    JNIEnv* env, jobject jobj, jstring jpath) {
    std::string path = GetJString(env, jpath);
    Service::AM::InstallStatus res = Service::AM::InstallCIA(
        path, [env, jobj](std::size_t total_bytes_read, std::size_t file_size) {
            env->CallVoidMethod(jobj, IDCache::GetCiaInstallHelperSetProgress(),
                                static_cast<jint>(file_size), static_cast<jint>(total_bytes_read));
        });

    return IDCache::GetJavaCiaInstallStatus(res);
}

jobjectArray Java_org_citra_citra_1emu_NativeLibrary_getSavestateInfo(
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

void Java_org_citra_citra_1emu_NativeLibrary_saveState([[maybe_unused]] JNIEnv* env,
                                                       [[maybe_unused]] jobject obj, jint slot) {
    Core::System::GetInstance().SendSignal(Core::System::Signal::Save, slot);
}

void Java_org_citra_citra_1emu_NativeLibrary_loadState([[maybe_unused]] JNIEnv* env,
                                                       [[maybe_unused]] jobject obj, jint slot) {
    Core::System::GetInstance().SendSignal(Core::System::Signal::Load, slot);
}

void Java_org_citra_citra_1emu_NativeLibrary_logDeviceInfo([[maybe_unused]] JNIEnv* env,
                                                           [[maybe_unused]] jobject obj) {
    LOG_INFO(Frontend, "Azahar Version: {} | {}-{}", Common::g_build_fullname, Common::g_scm_branch,
             Common::g_scm_desc);
    LOG_INFO(Frontend, "Host CPU: {}", Common::GetCPUCaps().cpu_string);
    // There is no decent way to get the OS version, so we log the API level instead.
    LOG_INFO(Frontend, "Host OS: Android API level {}", android_get_device_api_level());
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_isFullConsoleLinked(JNIEnv* env, jobject obj) {
    return HW::UniqueData::IsFullConsoleLinked();
}

void Java_org_citra_citra_1emu_NativeLibrary_unlinkConsole(JNIEnv* env, jobject obj) {
    HW::UniqueData::UnlinkConsole();
}

void Java_org_citra_citra_1emu_NativeLibrary_setTemporaryFrameLimit(JNIEnv* env, jobject obj,
                                                                    jdouble speed) {
    Settings::temporary_frame_limit = speed;
    Settings::is_temporary_frame_limit = true;
}

void Java_org_citra_citra_1emu_NativeLibrary_disableTemporaryFrameLimit(JNIEnv* env, jobject obj) {
    Settings::is_temporary_frame_limit = false;
}

void Java_org_citra_citra_1emu_NativeLibrary_playTimeManagerInit(JNIEnv* env, jobject obj) {
    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>();
}

void Java_org_citra_citra_1emu_NativeLibrary_playTimeManagerStart(JNIEnv* env, jobject obj,
                                                                  jlong title_id) {
    ptm_current_title_id = title_id;
    if (play_time_manager) {
        play_time_manager->SetProgramId(static_cast<u64>(title_id));
        play_time_manager->Start();
    }
}

void Java_org_citra_citra_1emu_NativeLibrary_playTimeManagerStop(JNIEnv* env, jobject obj) {
    play_time_manager->Stop();
}

jlong Java_org_citra_citra_1emu_NativeLibrary_playTimeManagerGetPlayTime(JNIEnv* env, jobject obj,
                                                                         jlong title_id) {
    return static_cast<jlong>(play_time_manager->GetPlayTime(title_id));
}

jlong Java_org_citra_citra_1emu_NativeLibrary_playTimeManagerGetCurrentTitleId(JNIEnv* env,
                                                                               jobject obj) {
    return ptm_current_title_id;
}

void Java_org_citra_citra_1emu_NativeLibrary_setInsertedCartridge(JNIEnv* env, jobject obj,
                                                                  jstring path) {
    inserted_cartridge = GetJString(env, path);
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_uninstallTitle(JNIEnv* env, jobject obj,
                                                                jlong j_titleid, jint j_mediatype) {
    const auto titleid = static_cast<u64>(j_titleid);
    const auto result =
        Service::AM::UninstallProgram(static_cast<Service::FS::MediaType>(j_mediatype), titleid);
    if (result.IsError()) {
        LOG_ERROR(Frontend, "Failed to uninstall '{}': 0x{:08X}", std::to_string(titleid),
                  result.raw);
        return false;
    }
    return true;
}

jboolean Java_org_citra_citra_1emu_NativeLibrary_nativeFileExists(JNIEnv* env, jobject obj,
                                                                  jstring j_path) {
    const auto path = GetJString(env, j_path);
    return FileUtil::Exists(path);
}

void Java_org_citra_citra_1emu_NativeLibrary_deleteOpenGLShaderCache(JNIEnv* env, jobject obj,
                                                                     jlong title_id) {
    for (const std::string_view cache_type : {"separable", "conventional"}) {
        const std::string path =
            fmt::format("{}opengl/precompiled/{}/{:016X}.bin",
                        FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir), cache_type, title_id);
        LOG_INFO(Frontend, "Deleting shader file: {}", path);
        FileUtil::Delete(path);
    }
    const std::string path =
        fmt::format("{}opengl/transferable/{:016X}.bin",
                    FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir), title_id);
    LOG_INFO(Frontend, "Deleting shader file: {}", path);
    FileUtil::Delete(path);
}

void Java_org_citra_citra_1emu_NativeLibrary_deleteVulkanShaderCache(JNIEnv* env, jobject obj,
                                                                     jlong title_id) {
    for (const std::string_view cache_type : {"vs", "fs", "gs", "pl"}) {
        const std::string path =
            fmt::format("{}vulkan/transferable/{:016X}_{}.vkch",
                        FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir), title_id, cache_type);
        LOG_INFO(Frontend, "Deleting shader file: {}", path);
        FileUtil::Delete(path);
    }

    FileUtil::ForeachDirectoryEntry(
        nullptr,
        fmt::format("{}vulkan/pipeline", FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir)),
        [title_id]([[maybe_unused]] u64* num_entries_out, const std::string& directory,
                   const std::string& virtual_name) {
            if (virtual_name.starts_with(fmt::format("{:016X}", title_id))) {
                std::string path = directory + DIR_SEP + virtual_name;
                LOG_INFO(Frontend, "Deleting shader file: {}", path);
                FileUtil::Delete(path);
            }
            return true;
        });
}

} // extern "C"