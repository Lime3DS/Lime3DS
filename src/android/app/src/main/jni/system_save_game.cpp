// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <common/string_util.h>
#include <core/core.h>
#include <core/hle/service/cfg/cfg.h>
#include <core/hle/service/ptm/ptm.h>
#include "android_common/android_common.h"

std::shared_ptr<Service::CFG::Module> cfg;

extern "C" {

void Java_io_github_lime3ds_android_utils_SystemSaveGame_save([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jobject obj) {
    cfg->UpdateConfigNANDSavegame();
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_load([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jobject obj) {
    cfg = Service::CFG::GetModule(Core::System::GetInstance());
}

jboolean Java_io_github_lime3ds_android_utils_SystemSaveGame_getIsSystemSetupNeeded(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return cfg->IsSystemSetupNeeded();
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_setSystemSetupNeeded(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, jboolean needed) {
    cfg->SetSystemSetupNeeded(needed);
}

jstring Java_io_github_lime3ds_android_utils_SystemSaveGame_getUsername(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return ToJString(env, Common::UTF16ToUTF8(cfg->GetUsername()));
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_setUsername([[maybe_unused]] JNIEnv* env,
                                                                     [[maybe_unused]] jobject obj,
                                                                     jstring username) {
    cfg->SetUsername(Common::UTF8ToUTF16(GetJString(env, username)));
}

jshortArray Java_io_github_lime3ds_android_utils_SystemSaveGame_getBirthday(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    jshortArray jbirthdayArray = env->NewShortArray(2);
    auto birthday = cfg->GetBirthday();
    jshort birthdayArray[2]{static_cast<jshort>(get<0>(birthday)),
                            static_cast<jshort>(get<1>(birthday))};
    env->SetShortArrayRegion(jbirthdayArray, 0, 2, birthdayArray);
    return jbirthdayArray;
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_setBirthday([[maybe_unused]] JNIEnv* env,
                                                                     [[maybe_unused]] jobject obj,
                                                                     jshort jmonth, jshort jday) {
    cfg->SetBirthday(static_cast<u8>(jmonth), static_cast<u8>(jday));
}

jint Java_io_github_lime3ds_android_utils_SystemSaveGame_getSystemLanguage(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return cfg->GetSystemLanguage();
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_setSystemLanguage(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, jint jsystemLanguage) {
    cfg->SetSystemLanguage(static_cast<Service::CFG::SystemLanguage>(jsystemLanguage));
}

jint Java_io_github_lime3ds_android_utils_SystemSaveGame_getSoundOutputMode(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return cfg->GetSoundOutputMode();
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_setSoundOutputMode(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, jint jmode) {
    cfg->SetSoundOutputMode(static_cast<Service::CFG::SoundOutputMode>(jmode));
}

jshort Java_io_github_lime3ds_android_utils_SystemSaveGame_getCountryCode(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return cfg->GetCountryCode();
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_setCountryCode(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj, jshort jmode) {
    cfg->SetCountryCode(static_cast<u8>(jmode));
}

jint Java_io_github_lime3ds_android_utils_SystemSaveGame_getPlayCoins(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return Service::PTM::Module::GetPlayCoins();
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_setPlayCoins([[maybe_unused]] JNIEnv* env,
                                                                      [[maybe_unused]] jobject obj,
                                                                      jint jcoins) {
    Service::PTM::Module::SetPlayCoins(static_cast<u16>(jcoins));
}

jlong Java_io_github_lime3ds_android_utils_SystemSaveGame_getConsoleId(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    return cfg->GetConsoleUniqueId();
}

void Java_io_github_lime3ds_android_utils_SystemSaveGame_regenerateConsoleId(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jobject obj) {
    const auto [random_number, console_id] = cfg->GenerateConsoleUniqueId();
    cfg->SetConsoleUniqueId(random_number, console_id);
    cfg->UpdateConfigNANDSavegame();
}

} // extern "C"
