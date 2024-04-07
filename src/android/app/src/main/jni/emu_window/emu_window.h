// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "core/frontend/emu_window.h"

namespace Core {
class System;
}

class EmuWindow_Android : public Frontend::EmuWindow {
public:
    EmuWindow_Android(ANativeWindow* surface);
    ~EmuWindow_Android();

    /// Called by the onSurfaceChanges() method to change the surface
    bool OnSurfaceChanged(ANativeWindow* surface);

    /// Handles touch event that occur.(Touched or released)
    bool OnTouchEvent(int x, int y, bool pressed);

    /// Handles movement of touch pointer
    void OnTouchMoved(int x, int y);

    void MakeCurrent() override;

    void DoneCurrent() override;

    virtual void TryPresenting() {}

    virtual void StopPresenting() {}

protected:
    void OnFramebufferSizeChanged();

    /// Creates the API specific window surface
    virtual bool CreateWindowSurface() {
        return false;
    }

    /// Destroys the API specific window surface
    virtual void DestroyWindowSurface() {}

    /// Destroys the graphics context
    virtual void DestroyContext() {}

protected:
    ANativeWindow* render_window{};
    ANativeWindow* host_window{};

    int window_width{};
    int window_height{};

    std::unique_ptr<Frontend::GraphicsContext> core_context;
};
