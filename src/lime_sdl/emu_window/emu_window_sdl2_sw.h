// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "lime_sdl/emu_window/emu_window_sdl2.h"

struct SDL_Renderer;
struct SDL_Surface;

namespace VideoCore {
enum class ScreenId : u32;
}

namespace Core {
class System;
}

class EmuWindow_SDL2_SW : public EmuWindow_SDL2 {
public:
    explicit EmuWindow_SDL2_SW(Core::System& system, bool fullscreen, bool is_secondary);
    ~EmuWindow_SDL2_SW();

    void Present() override;
    std::unique_ptr<GraphicsContext> CreateSharedContext() const override;
    void MakeCurrent() override {}
    void DoneCurrent() override {}

private:
    /// Loads a framebuffer to an SDL surface
    SDL_Surface* LoadFramebuffer(VideoCore::ScreenId screen_id);

    /// The system class.
    Core::System& system;

    /// The SDL software renderer
    SDL_Renderer* renderer;

    /// The window surface
    SDL_Surface* window_surface;
};
