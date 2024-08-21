// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <type_traits>
#include <jni.h>
#include "video_core/rasterizer_interface.h"

namespace Service::AM {
enum class InstallStatus : u32;
} // namespace Service::AM

namespace IDCache {

JNIEnv* GetEnvForThread();

jclass GetCoreErrorClass();
jclass GetSavestateInfoClass();

jclass GetNativeLibraryClass();
jmethodID GetOnCoreError();
jmethodID GetDisplayAlertMsg();
jmethodID GetDisplayAlertPrompt();
jmethodID GetAlertPromptButton();
jmethodID GetIsPortraitMode();
jmethodID GetLandscapeScreenLayout();
jmethodID GetPortraitScreenLayout();
jmethodID GetExitEmulationActivity();
jmethodID GetRequestCameraPermission();
jmethodID GetRequestMicPermission();

jclass GetCheatClass();
jfieldID GetCheatPointer();
jmethodID GetCheatConstructor();

jfieldID GetGameInfoPointer();

jclass GetDiskCacheProgressClass();
jmethodID GetDiskCacheLoadProgress();
jobject GetJavaLoadCallbackStage(VideoCore::LoadCallbackStage stage);

jclass GetCiaInstallHelperClass();
jmethodID GetCiaInstallHelperSetProgress();
jobject GetJavaCiaInstallStatus(Service::AM::InstallStatus status);

} // namespace IDCache

template <typename T = jobject>
using SharedGlobalRef = std::shared_ptr<std::remove_pointer_t<T>>;

struct SharedGlobalRefDeleter {
    void operator()(jobject ptr) {
        JNIEnv* env = IDCache::GetEnvForThread();
        env->DeleteGlobalRef(ptr);
    }
};

template <typename T = jobject>
SharedGlobalRef<T> NewSharedGlobalRef(T object) {
    JNIEnv* env = IDCache::GetEnvForThread();
    auto* global_ref = reinterpret_cast<T>(env->NewGlobalRef(object));
    return SharedGlobalRef<T>(global_ref, SharedGlobalRefDeleter());
}
