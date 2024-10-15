// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>
#include "common/common_types.h"
#include "core/frontend/emu_window.h"

union SDL_Event;
struct SDL_Window;

namespace Core {
class System;
}

class EmuWindow_SDL2 : public Frontend::EmuWindow {
public:
    explicit EmuWindow_SDL2(Core::System& system_, bool is_secondary);
    ~EmuWindow_SDL2();

    /// Initializes SDL2
    static void InitializeSDL2();

    /// Presents the most recent frame from the video backend
    virtual void Present() {}

    /// Polls window events
    void PollEvents() override;

    /// Whether the window is still open, and a close request hasn't yet been sent
    bool IsOpen() const;

    /// Close the window.
    void RequestClose();

protected:
    /// Gets the ID of the window an event originated from.
    u32 GetEventWindowId(const SDL_Event& event) const;

    /// Called by PollEvents when a key is pressed or released.
    void OnKeyEvent(int key, u8 state);

    /// Called by PollEvents when the mouse moves.
    void OnMouseMotion(s32 x, s32 y);

    /// Called by PollEvents when a mouse button is pressed or released
    void OnMouseButton(u32 button, u8 state, s32 x, s32 y);

    /// Translates pixel position (0..1) to pixel positions
    std::pair<unsigned, unsigned> TouchToPixelPos(float touch_x, float touch_y) const;

    /// Called by PollEvents when a finger starts touching the touchscreen
    void OnFingerDown(float x, float y);

    /// Called by PollEvents when a finger moves while touching the touchscreen
    void OnFingerMotion(float x, float y);

    /// Called by PollEvents when a finger stops touching the touchscreen
    void OnFingerUp();

    /// Called by PollEvents when any event that may cause the window to be resized occurs
    void OnResize();

    /// Called when user passes the fullscreen parameter flag
    void Fullscreen();

    /// Called when a configuration change affects the minimal size of the window
    void OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) override;

    /// Called when polling to update framerate
    void UpdateFramerateCounter();

    /// Is the window still open?
    bool is_open = true;

    /// Internal SDL2 render window
    SDL_Window* render_window;

    /// Internal SDL2 window ID
    u32 render_window_id{};

    /// Fake hidden window for the core context
    SDL_Window* dummy_window;

    /// Keeps track of how often to update the title bar during gameplay
    u32 last_time = 0;

    Core::System& system;
};
