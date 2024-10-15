// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "lime_sdl/emu_window/emu_window_sdl2.h"

namespace Frontend {
class GraphicsContext;
}

namespace Core {
class System;
}

class EmuWindow_SDL2_VK final : public EmuWindow_SDL2 {
public:
    explicit EmuWindow_SDL2_VK(Core::System& system_, bool fullscreen, bool is_secondary);
    ~EmuWindow_SDL2_VK() override;

    std::unique_ptr<Frontend::GraphicsContext> CreateSharedContext() const override;
};
