// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <vector>

#include <jni.h>

#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "jni/cheats/cheat.h"
#include "jni/id_cache.h"

extern "C" {

static Cheats::CheatEngine& GetEngine() {
    Core::System& system{Core::System::GetInstance()};
    return system.CheatEngine();
}

JNIEXPORT void JNICALL
Java_io_github_lime3ds_android_features_cheats_model_CheatEngine_loadCheatFile(JNIEnv* env, jclass,
                                                                               jlong title_id) {
    GetEngine().LoadCheatFile(title_id);
}

JNIEXPORT void JNICALL
Java_io_github_lime3ds_android_features_cheats_model_CheatEngine_saveCheatFile(JNIEnv* env, jclass,
                                                                               jlong title_id) {
    GetEngine().SaveCheatFile(title_id);
}

JNIEXPORT jobjectArray JNICALL
Java_io_github_lime3ds_android_features_cheats_model_CheatEngine_getCheats(JNIEnv* env, jclass) {
    auto cheats = GetEngine().GetCheats();

    const jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(cheats.size()), IDCache::GetCheatClass(), nullptr);

    jsize i = 0;
    for (auto& cheat : cheats)
        env->SetObjectArrayElement(array, i++, CheatToJava(env, std::move(cheat)));

    return array;
}

JNIEXPORT void JNICALL Java_io_github_lime3ds_android_features_cheats_model_CheatEngine_addCheat(
    JNIEnv* env, jclass, jobject j_cheat) {
    auto cheat = *CheatFromJava(env, j_cheat);
    GetEngine().AddCheat(std::move(cheat));
}

JNIEXPORT void JNICALL Java_io_github_lime3ds_android_features_cheats_model_CheatEngine_removeCheat(
    JNIEnv* env, jclass, jint index) {
    GetEngine().RemoveCheat(index);
}

JNIEXPORT void JNICALL Java_io_github_lime3ds_android_features_cheats_model_CheatEngine_updateCheat(
    JNIEnv* env, jclass, jint index, jobject j_new_cheat) {
    auto cheat = *CheatFromJava(env, j_new_cheat);
    GetEngine().UpdateCheat(index, std::move(cheat));
}
}
