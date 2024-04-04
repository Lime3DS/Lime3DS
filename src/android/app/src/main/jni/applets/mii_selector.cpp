// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/string_util.h"
#include "jni/android_common/android_common.h"
#include "jni/applets/mii_selector.h"
#include "jni/id_cache.h"

static jclass s_mii_selector_class;
static jclass s_mii_selector_config_class;
static jclass s_mii_selector_data_class;
static jmethodID s_mii_selector_execute;

namespace MiiSelector {

AndroidMiiSelector::~AndroidMiiSelector() = default;

void AndroidMiiSelector::Setup(const Frontend::MiiSelectorConfig& config) {
    JNIEnv* env = IDCache::GetEnvForThread();
    auto miis = Frontend::LoadMiis();

    // Create the Java MiiSelectorConfig object
    jobject java_config = env->AllocObject(s_mii_selector_config_class);
    env->SetBooleanField(java_config,
                         env->GetFieldID(s_mii_selector_config_class, "enableCancelButton", "Z"),
                         static_cast<jboolean>(config.enable_cancel_button));
    env->SetObjectField(java_config,
                        env->GetFieldID(s_mii_selector_config_class, "title", "Ljava/lang/String;"),
                        ToJString(env, config.title));
    env->SetLongField(
        java_config, env->GetFieldID(s_mii_selector_config_class, "initiallySelectedMiiIndex", "J"),
        static_cast<jlong>(config.initially_selected_mii_index));

    // List mii names
    // The 'Standard Mii' is not included here as we need Java side to translate it
    const jclass string_class = reinterpret_cast<jclass>(env->FindClass("java/lang/String"));
    const jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(miis.size()), string_class, nullptr);
    for (std::size_t i = 0; i < miis.size(); ++i) {
        const auto name = Common::UTF16BufferToUTF8(miis[i].mii_name);
        env->SetObjectArrayElement(array, static_cast<jsize>(i), ToJString(env, name));
    }
    env->SetObjectField(
        java_config,
        env->GetFieldID(s_mii_selector_config_class, "miiNames", "[Ljava/lang/String;"), array);

    // Invoke backend Execute method
    jobject data =
        env->CallStaticObjectMethod(s_mii_selector_class, s_mii_selector_execute, java_config);

    const u32 return_code = static_cast<u32>(
        env->GetLongField(data, env->GetFieldID(s_mii_selector_data_class, "returnCode", "J")));
    if (return_code == 1) {
        Finalize(return_code, Mii::MiiData{});
        return;
    }

    const int index = static_cast<int>(
        env->GetIntField(data, env->GetFieldID(s_mii_selector_data_class, "index", "I")));
    ASSERT_MSG(index >= 0 && index <= miis.size(), "Index returned is out of bound");
    Finalize(return_code, index == 0
                              ? HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data
                              : miis.at(static_cast<std::size_t>(index - 1)));
}

void InitJNI(JNIEnv* env) {
    s_mii_selector_class = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("io/github/lime3ds/android/applets/MiiSelector")));
    s_mii_selector_config_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("io/github/lime3ds/android/applets/MiiSelector$MiiSelectorConfig")));
    s_mii_selector_data_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("io/github/lime3ds/android/applets/MiiSelector$MiiSelectorData")));

    s_mii_selector_execute = env->GetStaticMethodID(
        s_mii_selector_class, "Execute",
        "(Lio/github/lime3ds/android/applets/MiiSelector$MiiSelectorConfig;)Lio/"
        "github/lime3ds/android/applets/MiiSelector$MiiSelectorData;");
}

void CleanupJNI(JNIEnv* env) {
    env->DeleteGlobalRef(s_mii_selector_class);
    env->DeleteGlobalRef(s_mii_selector_config_class);
    env->DeleteGlobalRef(s_mii_selector_data_class);
}

} // namespace MiiSelector
