// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/android_storage.h"
#include "common/common_paths.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hle/service/am/am.h"
#include "jni/applets/mii_selector.h"
#include "jni/applets/swkbd.h"
#include "jni/camera/still_image_camera.h"
#include "jni/id_cache.h"

#include <jni.h>

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

static JavaVM* s_java_vm;

static jclass s_core_error_class;
static jclass s_savestate_info_class;

static jclass s_native_library_class;
static jmethodID s_on_core_error;
static jmethodID s_is_portrait_mode;
static jmethodID s_landscape_screen_layout;
static jmethodID s_portrait_screen_layout;
static jmethodID s_exit_emulation_activity;
static jmethodID s_request_camera_permission;
static jmethodID s_request_mic_permission;

static jclass s_cheat_class;
static jfieldID s_cheat_pointer;
static jmethodID s_cheat_constructor;

static jfieldID s_game_info_pointer;

static jclass s_disk_cache_progress_class;
static jmethodID s_disk_cache_load_progress;
static std::unordered_map<VideoCore::LoadCallbackStage, jobject> s_java_load_callback_stages;

static jclass s_cia_install_helper_class;
static jmethodID s_cia_install_helper_set_progress;
static std::unordered_map<Service::AM::InstallStatus, jobject> s_java_cia_install_status;

namespace IDCache {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                s_java_vm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                s_java_vm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

jclass GetCoreErrorClass() {
    return s_core_error_class;
}

jclass GetSavestateInfoClass() {
    return s_savestate_info_class;
}

jclass GetNativeLibraryClass() {
    return s_native_library_class;
}

jmethodID GetOnCoreError() {
    return s_on_core_error;
}

jmethodID GetIsPortraitMode() {
    return s_is_portrait_mode;
}

jmethodID GetLandscapeScreenLayout() {
    return s_landscape_screen_layout;
}

jmethodID GetPortraitScreenLayout() {
    return s_portrait_screen_layout;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

jmethodID GetRequestCameraPermission() {
    return s_request_camera_permission;
}

jmethodID GetRequestMicPermission() {
    return s_request_mic_permission;
}

jclass GetCheatClass() {
    return s_cheat_class;
}

jfieldID GetCheatPointer() {
    return s_cheat_pointer;
}

jmethodID GetCheatConstructor() {
    return s_cheat_constructor;
}

jfieldID GetGameInfoPointer() {
    return s_game_info_pointer;
}

jclass GetDiskCacheProgressClass() {
    return s_disk_cache_progress_class;
}

jmethodID GetDiskCacheLoadProgress() {
    return s_disk_cache_load_progress;
}

jobject GetJavaLoadCallbackStage(VideoCore::LoadCallbackStage stage) {
    const auto it = s_java_load_callback_stages.find(stage);
    ASSERT_MSG(it != s_java_load_callback_stages.end(), "Invalid LoadCallbackStage: {}", stage);

    return it->second;
}

jclass GetCiaInstallHelperClass() {
    return s_cia_install_helper_class;
}

jmethodID GetCiaInstallHelperSetProgress() {
    return s_cia_install_helper_set_progress;
}
jobject GetJavaCiaInstallStatus(Service::AM::InstallStatus status) {
    const auto it = s_java_cia_install_status.find(status);
    ASSERT_MSG(it != s_java_cia_install_status.end(), "Invalid InstallStatus: {}", status);

    return it->second;
}
} // namespace IDCache

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    s_java_vm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
        return JNI_ERR;

    // Initialize misc classes
    s_savestate_info_class = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("io/github/lime3ds/android/NativeLibrary$SaveStateInfo")));
    s_core_error_class = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("io/github/lime3ds/android/NativeLibrary$CoreError")));

    // Initialize NativeLibrary
    const jclass native_library_class = env->FindClass("io/github/lime3ds/android/NativeLibrary");
    s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
    s_on_core_error = env->GetStaticMethodID(
        s_native_library_class, "onCoreError",
        "(Lio/github/lime3ds/android/NativeLibrary$CoreError;Ljava/lang/String;)Z");
    s_is_portrait_mode = env->GetStaticMethodID(s_native_library_class, "isPortraitMode", "()Z");
    s_exit_emulation_activity =
        env->GetStaticMethodID(s_native_library_class, "exitEmulationActivity", "(I)V");
    s_request_camera_permission =
        env->GetStaticMethodID(s_native_library_class, "requestCameraPermission", "()Z");
    s_request_mic_permission =
        env->GetStaticMethodID(s_native_library_class, "requestMicPermission", "()Z");
    env->DeleteLocalRef(native_library_class);

    // Initialize Cheat
    const jclass cheat_class =
        env->FindClass("io/github/lime3ds/android/features/cheats/model/Cheat");
    s_cheat_class = reinterpret_cast<jclass>(env->NewGlobalRef(cheat_class));
    s_cheat_pointer = env->GetFieldID(cheat_class, "mPointer", "J");
    s_cheat_constructor = env->GetMethodID(cheat_class, "<init>", "(J)V");
    env->DeleteLocalRef(cheat_class);

    // Initialize GameInfo
    const jclass game_info_class = env->FindClass("io/github/lime3ds/android/model/GameInfo");
    s_game_info_pointer = env->GetFieldID(game_info_class, "pointer", "J");
    env->DeleteLocalRef(game_info_class);

    // Initialize Disk Shader Cache Progress Dialog
    s_disk_cache_progress_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("io/github/lime3ds/android/utils/DiskShaderCacheProgress")));
    jclass load_callback_stage_class =
        env->FindClass("io/github/lime3ds/android/utils/DiskShaderCacheProgress$LoadCallbackStage");
    s_disk_cache_load_progress = env->GetStaticMethodID(
        s_disk_cache_progress_class, "loadProgress",
        "(Lio/github/lime3ds/android/utils/DiskShaderCacheProgress$LoadCallbackStage;II)V");
    // Initialize LoadCallbackStage map
    const auto to_java_load_callback_stage = [env,
                                              load_callback_stage_class](const std::string& stage) {
        return env->NewGlobalRef(env->GetStaticObjectField(
            load_callback_stage_class,
            env->GetStaticFieldID(load_callback_stage_class, stage.c_str(),
                                  "Lio/github/lime3ds/android/utils/"
                                  "DiskShaderCacheProgress$LoadCallbackStage;")));
    };
    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Prepare,
                                        to_java_load_callback_stage("Prepare"));
    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Decompile,
                                        to_java_load_callback_stage("Decompile"));
    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Build,
                                        to_java_load_callback_stage("Build"));
    s_java_load_callback_stages.emplace(VideoCore::LoadCallbackStage::Complete,
                                        to_java_load_callback_stage("Complete"));
    env->DeleteLocalRef(load_callback_stage_class);

    // CIA Install
    s_cia_install_helper_class = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("io/github/lime3ds/android/utils/CiaInstallWorker")));
    s_cia_install_helper_set_progress =
        env->GetMethodID(s_cia_install_helper_class, "setProgressCallback", "(II)V");
    // Initialize CIA InstallStatus map
    jclass cia_install_status_class =
        env->FindClass("io/github/lime3ds/android/NativeLibrary$InstallStatus");
    const auto to_java_cia_install_status = [env,
                                             cia_install_status_class](const std::string& stage) {
        return env->NewGlobalRef(env->GetStaticObjectField(
            cia_install_status_class, env->GetStaticFieldID(cia_install_status_class, stage.c_str(),
                                                            "Lio/github/lime3ds/android/"
                                                            "NativeLibrary$InstallStatus;")));
    };
    s_java_cia_install_status.emplace(Service::AM::InstallStatus::Success,
                                      to_java_cia_install_status("Success"));
    s_java_cia_install_status.emplace(Service::AM::InstallStatus::ErrorFailedToOpenFile,
                                      to_java_cia_install_status("ErrorFailedToOpenFile"));
    s_java_cia_install_status.emplace(Service::AM::InstallStatus::ErrorFileNotFound,
                                      to_java_cia_install_status("ErrorFileNotFound"));
    s_java_cia_install_status.emplace(Service::AM::InstallStatus::ErrorAborted,
                                      to_java_cia_install_status("ErrorAborted"));
    s_java_cia_install_status.emplace(Service::AM::InstallStatus::ErrorInvalid,
                                      to_java_cia_install_status("ErrorInvalid"));
    s_java_cia_install_status.emplace(Service::AM::InstallStatus::ErrorEncrypted,
                                      to_java_cia_install_status("ErrorEncrypted"));
    env->DeleteLocalRef(cia_install_status_class);

    MiiSelector::InitJNI(env);
    SoftwareKeyboard::InitJNI(env);
    Camera::StillImage::InitJNI(env);
    AndroidStorage::InitJNI(env, s_native_library_class);

    return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
        return;
    }

    env->DeleteGlobalRef(s_savestate_info_class);
    env->DeleteGlobalRef(s_core_error_class);
    env->DeleteGlobalRef(s_disk_cache_progress_class);
    env->DeleteGlobalRef(s_native_library_class);
    env->DeleteGlobalRef(s_cheat_class);
    env->DeleteGlobalRef(s_cia_install_helper_class);

    for (auto& [key, object] : s_java_load_callback_stages) {
        env->DeleteGlobalRef(object);
    }

    for (auto& [key, object] : s_java_cia_install_status) {
        env->DeleteGlobalRef(object);
    }

    MiiSelector::CleanupJNI(env);
    SoftwareKeyboard::CleanupJNI(env);
    Camera::StillImage::CleanupJNI(env);
    AndroidStorage::CleanupJNI();
}

#ifdef __cplusplus
}
#endif
