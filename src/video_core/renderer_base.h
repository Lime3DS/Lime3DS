// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/frontend/framebuffer_layout.h"
#include "video_core/rasterizer_interface.h"

namespace Frontend {
class EmuWindow;
}

namespace Core {
class System;
}

namespace VideoCore {

enum class ScreenId : u32 {
    TopLeft,
    TopRight,
    Bottom,
};

struct RendererSettings {
    // Screenshot
    std::atomic_bool screenshot_requested{false};
    void* screenshot_bits{};
    std::function<void(bool)> screenshot_complete_callback;
    Layout::FramebufferLayout screenshot_framebuffer_layout;
    // Renderer
    std::atomic_bool bg_color_update_requested{false};
    std::atomic_bool shader_update_requested{false};
};

class RendererBase : NonCopyable {
public:
    explicit RendererBase(Core::System& system, Frontend::EmuWindow& window,
                          Frontend::EmuWindow* secondary_window);
    virtual ~RendererBase();

    /// Returns the rasterizer owned by the renderer
    virtual VideoCore::RasterizerInterface* Rasterizer() = 0;

    /// Finalize rendering the guest frame and draw into the presentation texture
    virtual void SwapBuffers() = 0;

    /// Draws the latest frame to the window waiting timeout_ms for a frame to arrive (Renderer
    /// specific implementation)
    virtual void TryPresent(int timeout_ms, bool is_secondary) = 0;
    virtual void TryPresent(int timeout_ms) {
        TryPresent(timeout_ms, false);
    }

    /// Prepares for video dumping (e.g. create necessary buffers, etc)
    virtual void PrepareVideoDumping() {}

    /// Cleans up after video dumping is ended
    virtual void CleanupVideoDumping() {}

    /// Synchronizes fixed function renderer state
    virtual void Sync() {}

    /// This is called to notify the rendering backend of a surface change
    virtual void NotifySurfaceChanged() {}

    /// Returns the resolution scale factor relative to the native 3DS screen resolution
    u32 GetResolutionScaleFactor();

    /// Updates the framebuffer layout of the contained render window handle.
    void UpdateCurrentFramebufferLayout(bool is_portrait_mode = {});

    /// Ends the current frame
    void EndFrame();

    f32 GetCurrentFPS() const {
        return current_fps;
    }

    s32 GetCurrentFrame() const {
        return current_frame;
    }

    Frontend::EmuWindow& GetRenderWindow() {
        return render_window;
    }

    const Frontend::EmuWindow& GetRenderWindow() const {
        return render_window;
    }

    [[nodiscard]] RendererSettings& Settings() {
        return settings;
    }

    [[nodiscard]] const RendererSettings& Settings() const {
        return settings;
    }

    /// Returns true if a screenshot is being processed
    [[nodiscard]] bool IsScreenshotPending() const;

    /// Request a screenshot of the next frame
    void RequestScreenshot(void* data, std::function<void(bool)> callback,
                           const Layout::FramebufferLayout& layout);

protected:
    Core::System& system;
    RendererSettings settings;
    Frontend::EmuWindow& render_window;    ///< Reference to the render window handle.
    Frontend::EmuWindow* secondary_window; ///< Reference to the secondary render window handle.
    f32 current_fps = 0.0f;                ///< Current framerate, should be set by the renderer
    s32 current_frame = 0;                 ///< Current frame, should be set by the renderer
};

} // namespace VideoCore
