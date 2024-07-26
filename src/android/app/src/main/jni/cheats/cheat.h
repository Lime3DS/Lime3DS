// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include <jni.h>

namespace Cheats {
class CheatBase;
}

std::shared_ptr<Cheats::CheatBase>* CheatFromJava(JNIEnv* env, jobject cheat);
jobject CheatToJava(JNIEnv* env, std::shared_ptr<Cheats::CheatBase> cheat);
