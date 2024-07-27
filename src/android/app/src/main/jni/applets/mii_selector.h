// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <jni.h>
#include "core/frontend/applets/mii_selector.h"

namespace MiiSelector {

class AndroidMiiSelector final : public Frontend::MiiSelector {
public:
    ~AndroidMiiSelector();

    void Setup(const Frontend::MiiSelectorConfig& config) override;
};

// Should be called in JNI_Load
void InitJNI(JNIEnv* env);

// Should be called in JNI_Unload
void CleanupJNI(JNIEnv* env);

} // namespace MiiSelector
