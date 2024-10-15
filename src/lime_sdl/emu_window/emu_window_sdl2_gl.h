// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "lime_sdl/emu_window/emu_window_sdl2.h"

struct SDL_Window;

namespace Core {
class System;
}

class EmuWindow_SDL2_GL : public EmuWindow_SDL2 {
public:
    explicit EmuWindow_SDL2_GL(Core::System& system_, bool fullscreen, bool is_secondary);
    ~EmuWindow_SDL2_GL();

    void Present() override;
    std::unique_ptr<GraphicsContext> CreateSharedContext() const override;
    void MakeCurrent() override;
    void DoneCurrent() override;
    void SaveContext() override;
    void RestoreContext() override;

private:
    using SDL_GLContext = void*;

    /// The OpenGL context associated with the window
    SDL_GLContext window_context;

    /// Used by SaveContext and RestoreContext
    SDL_GLContext last_saved_context;

    /// The OpenGL context associated with the core
    std::unique_ptr<Frontend::GraphicsContext> core_context;
};
