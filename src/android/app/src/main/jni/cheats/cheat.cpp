// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "jni/cheats/cheat.h"

#include <memory>
#include <string>
#include <vector>

#include <jni.h>

#include "common/string_util.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/gateway_cheat.h"
#include "jni/android_common/android_common.h"
#include "jni/id_cache.h"

std::shared_ptr<Cheats::CheatBase>* CheatFromJava(JNIEnv* env, jobject cheat) {
    return reinterpret_cast<std::shared_ptr<Cheats::CheatBase>*>(
        env->GetLongField(cheat, IDCache::GetCheatPointer()));
}

jobject CheatToJava(JNIEnv* env, std::shared_ptr<Cheats::CheatBase> cheat) {
    return env->NewObject(
        IDCache::GetCheatClass(), IDCache::GetCheatConstructor(),
        reinterpret_cast<jlong>(new std::shared_ptr<Cheats::CheatBase>(std::move(cheat))));
}

extern "C" {

JNIEXPORT void JNICALL
Java_io_github_lime3ds_android_features_cheats_model_Cheat_finalize(JNIEnv* env, jobject obj) {
    delete CheatFromJava(env, obj);
}

JNIEXPORT jstring JNICALL
Java_io_github_lime3ds_android_features_cheats_model_Cheat_getName(JNIEnv* env, jobject obj) {
    return ToJString(env, (*CheatFromJava(env, obj))->GetName());
}

JNIEXPORT jstring JNICALL
Java_io_github_lime3ds_android_features_cheats_model_Cheat_getNotes(JNIEnv* env, jobject obj) {
    return ToJString(env, (*CheatFromJava(env, obj))->GetComments());
}

JNIEXPORT jstring JNICALL
Java_io_github_lime3ds_android_features_cheats_model_Cheat_getCode(JNIEnv* env, jobject obj) {
    return ToJString(env, (*CheatFromJava(env, obj))->GetCode());
}

JNIEXPORT jboolean JNICALL
Java_io_github_lime3ds_android_features_cheats_model_Cheat_getEnabled(JNIEnv* env, jobject obj) {
    return static_cast<jboolean>((*CheatFromJava(env, obj))->IsEnabled());
}

JNIEXPORT void JNICALL Java_io_github_lime3ds_android_features_cheats_model_Cheat_setEnabledImpl(
    JNIEnv* env, jobject obj, jboolean j_enabled) {
    (*CheatFromJava(env, obj))->SetEnabled(static_cast<bool>(j_enabled));
}

JNIEXPORT jint JNICALL
Java_io_github_lime3ds_android_features_cheats_model_Cheat_isValidGatewayCode(JNIEnv* env, jclass,
                                                                              jstring j_code) {
    const std::string code = GetJString(env, j_code);
    const auto code_lines = Common::SplitString(code, '\n');

    for (int i = 0; i < code_lines.size(); ++i) {
        Cheats::GatewayCheat::CheatLine cheat_line(code_lines[i]);
        if (!cheat_line.valid) {
            return i + 1;
        }
    }

    return 0;
}

JNIEXPORT jobject JNICALL
Java_io_github_lime3ds_android_features_cheats_model_Cheat_createGatewayCode(JNIEnv* env, jclass,
                                                                             jstring j_name,
                                                                             jstring j_notes,
                                                                             jstring j_code) {
    return CheatToJava(env, std::make_shared<Cheats::GatewayCheat>(GetJString(env, j_name),
                                                                   GetJString(env, j_code),
                                                                   GetJString(env, j_notes)));
}
}
