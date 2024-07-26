// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include <jni.h>

#include "core/core.h"
#include "jni/android_common/android_common.h"
#include "jni/applets/swkbd.h"
#include "jni/id_cache.h"

static jclass s_software_keyboard_class;
static jclass s_keyboard_config_class;
static jclass s_keyboard_data_class;
static jclass s_validation_error_class;
static jmethodID s_swkbd_execute;
static jmethodID s_swkbd_show_error;

namespace SoftwareKeyboard {

static jobject ToJavaKeyboardConfig(const Frontend::KeyboardConfig& config) {
    JNIEnv* env = IDCache::GetEnvForThread();
    jobject object = env->AllocObject(s_keyboard_config_class);
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "buttonConfig", "I"),
                     static_cast<jint>(config.button_config));
    env->SetIntField(object, env->GetFieldID(s_keyboard_config_class, "maxTextLength", "I"),
                     static_cast<jint>(config.max_text_length));
    env->SetBooleanField(object, env->GetFieldID(s_keyboard_config_class, "multilineMode", "Z"),
                         static_cast<jboolean>(config.multiline_mode));
    env->SetObjectField(object,
                        env->GetFieldID(s_keyboard_config_class, "hintText", "Ljava/lang/String;"),
                        ToJString(env, config.hint_text));

    const jclass string_class = reinterpret_cast<jclass>(env->FindClass("java/lang/String"));
    const jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(config.button_text.size()), string_class,
                            ToJString(env, config.button_text[0]));
    for (std::size_t i = 1; i < config.button_text.size(); ++i) {
        env->SetObjectArrayElement(array, static_cast<jsize>(i),
                                   ToJString(env, config.button_text[i]));
    }
    env->SetObjectField(
        object, env->GetFieldID(s_keyboard_config_class, "buttonText", "[Ljava/lang/String;"),
        array);

    return object;
}

static Frontend::KeyboardData ToFrontendKeyboardData(jobject object) {
    JNIEnv* env = IDCache::GetEnvForThread();
    const jstring string = reinterpret_cast<jstring>(env->GetObjectField(
        object, env->GetFieldID(s_keyboard_data_class, "text", "Ljava/lang/String;")));
    return Frontend::KeyboardData{
        GetJString(env, string),
        static_cast<u8>(
            env->GetIntField(object, env->GetFieldID(s_keyboard_data_class, "button", "I")))};
}

AndroidKeyboard::~AndroidKeyboard() = default;

void AndroidKeyboard::Execute(const Frontend::KeyboardConfig& config) {
    SoftwareKeyboard::Execute(config);

    const auto data = ToFrontendKeyboardData(IDCache::GetEnvForThread()->CallStaticObjectMethod(
        s_software_keyboard_class, s_swkbd_execute, ToJavaKeyboardConfig(config)));
    Finalize(data.text, data.button);
}

void AndroidKeyboard::ShowError(const std::string& error) {
    JNIEnv* env = IDCache::GetEnvForThread();
    env->CallStaticVoidMethod(s_software_keyboard_class, s_swkbd_show_error, ToJString(env, error));
}

void InitJNI(JNIEnv* env) {
    s_software_keyboard_class = reinterpret_cast<jclass>(
        env->NewGlobalRef(env->FindClass("io/github/lime3ds/android/applets/SoftwareKeyboard")));
    s_keyboard_config_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("io/github/lime3ds/android/applets/SoftwareKeyboard$KeyboardConfig")));
    s_keyboard_data_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("io/github/lime3ds/android/applets/SoftwareKeyboard$KeyboardData")));
    s_validation_error_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("io/github/lime3ds/android/applets/SoftwareKeyboard$ValidationError")));

    s_swkbd_execute =
        env->GetStaticMethodID(s_software_keyboard_class, "Execute",
                               "(Lio/github/lime3ds/android/applets/"
                               "SoftwareKeyboard$KeyboardConfig;)Lio/github/lime3ds/android/"
                               "applets/SoftwareKeyboard$KeyboardData;");
    s_swkbd_show_error =
        env->GetStaticMethodID(s_software_keyboard_class, "ShowError", "(Ljava/lang/String;)V");
}

void CleanupJNI(JNIEnv* env) {
    env->DeleteGlobalRef(s_software_keyboard_class);
    env->DeleteGlobalRef(s_keyboard_config_class);
    env->DeleteGlobalRef(s_keyboard_data_class);
    env->DeleteGlobalRef(s_validation_error_class);
}

} // namespace SoftwareKeyboard

jobject ToJavaValidationError(Frontend::ValidationError error) {
    static const std::map<Frontend::ValidationError, const char*> ValidationErrorNameMap{{
        {Frontend::ValidationError::None, "None"},
        {Frontend::ValidationError::ButtonOutOfRange, "ButtonOutOfRange"},
        {Frontend::ValidationError::MaxDigitsExceeded, "MaxDigitsExceeded"},
        {Frontend::ValidationError::AtSignNotAllowed, "AtSignNotAllowed"},
        {Frontend::ValidationError::PercentNotAllowed, "PercentNotAllowed"},
        {Frontend::ValidationError::BackslashNotAllowed, "BackslashNotAllowed"},
        {Frontend::ValidationError::ProfanityNotAllowed, "ProfanityNotAllowed"},
        {Frontend::ValidationError::CallbackFailed, "CallbackFailed"},
        {Frontend::ValidationError::FixedLengthRequired, "FixedLengthRequired"},
        {Frontend::ValidationError::MaxLengthExceeded, "MaxLengthExceeded"},
        {Frontend::ValidationError::BlankInputNotAllowed, "BlankInputNotAllowed"},
        {Frontend::ValidationError::EmptyInputNotAllowed, "EmptyInputNotAllowed"},
    }};
    ASSERT(ValidationErrorNameMap.count(error));

    JNIEnv* env = IDCache::GetEnvForThread();
    return env->GetStaticObjectField(
        s_validation_error_class,
        env->GetStaticFieldID(
            s_validation_error_class, ValidationErrorNameMap.at(error),
            "Lio/github/lime3ds/android/applets/SoftwareKeyboard$ValidationError;"));
}

jobject Java_io_github_lime3ds_android_applets_SoftwareKeyboard_ValidateFilters(JNIEnv* env,
                                                                                jclass clazz,
                                                                                jstring text) {

    const auto ret =
        Core::System::GetInstance().GetSoftwareKeyboard()->ValidateFilters(GetJString(env, text));
    return ToJavaValidationError(ret);
}

jobject Java_io_github_lime3ds_android_applets_SoftwareKeyboard_ValidateInput(JNIEnv* env,
                                                                              jclass clazz,
                                                                              jstring text) {

    const auto ret =
        Core::System::GetInstance().GetSoftwareKeyboard()->ValidateInput(GetJString(env, text));
    return ToJavaValidationError(ret);
}
