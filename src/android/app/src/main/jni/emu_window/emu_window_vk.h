// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "jni/emu_window/emu_window.h"

struct ANativeWindow;

class EmuWindow_Android_Vulkan : public EmuWindow_Android {
public:
    EmuWindow_Android_Vulkan(ANativeWindow* surface,
                             std::shared_ptr<Common::DynamicLibrary> driver_library);
    ~EmuWindow_Android_Vulkan() override = default;

    void PollEvents() override {}

    std::unique_ptr<GraphicsContext> CreateSharedContext() const override;

    std::shared_ptr<Common::DynamicLibrary> GetDriverLibrary() override;

private:
    bool CreateWindowSurface() override;

private:
    std::shared_ptr<Common::DynamicLibrary> driver_library;
};
