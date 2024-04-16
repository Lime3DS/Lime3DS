// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <tuple>
#include <utility>

#include "common/common_types.h"
#include "core/3ds.h"
#include "core/frontend/framebuffer_layout.h"

namespace Common {
class DynamicLibrary;
}

namespace Frontend {

/// Information for the Graphics Backends signifying what type of screen pointer is in
/// WindowInformation
enum class WindowSystemType : u8 {
    Headless,
    Android,
    Windows,
    MacOS,
    X11,
    Wayland,
};

struct Frame;
/**
 * For smooth Vsync rendering, we want to always present the latest frame that the core generates,
 * but also make sure that rendering happens at the pace that the frontend dictates. This is a
 * helper class that the renderer can define to sync frames between the render thread and the
 * presentation thread
 */
class TextureMailbox {
public:
    virtual ~TextureMailbox() = default;

    /**
     * Recreate the render objects attached to this frame with the new specified width/height
     */
    virtual void ReloadRenderFrame(Frontend::Frame* frame, u32 width, u32 height) = 0;

    /**
     * Recreate the presentation objects attached to this frame with the new specified width/height
     */
    virtual void ReloadPresentFrame(Frontend::Frame* frame, u32 width, u32 height) = 0;

    /**
     * Render thread calls this to get an available frame to present
     */
    virtual Frontend::Frame* GetRenderFrame() = 0;

    /**
     * Render thread calls this after draw commands are done to add to the presentation mailbox
     */
    virtual void ReleaseRenderFrame(Frame* frame) = 0;

    /**
     * Presentation thread calls this to get the latest frame available to present. If there is no
     * frame available after timeout, returns the previous frame. If there is no previous frame it
     * returns nullptr
     */
    virtual Frontend::Frame* TryGetPresentFrame(int timeout_ms) = 0;
};

/**
 * Represents a graphics context that can be used for background computation or drawing. If the
 * graphics backend doesn't require the context, then the implementation of these methods can be
 * stubs
 */
class GraphicsContext {
public:
    virtual ~GraphicsContext();

    /// Checks whether this context uses OpenGL ES.
    virtual bool IsGLES() {
        return false;
    }

    /// Inform the driver to swap the front/back buffers and present the current image
    virtual void SwapBuffers() {};

    /// Makes the graphics context current for the caller thread
    virtual void MakeCurrent() {};

    /// Releases (dunno if this is the "right" word) the context from the caller thread
    virtual void DoneCurrent() {};

    /// Gets the GPU driver library (used by Android only)
    virtual std::shared_ptr<Common::DynamicLibrary> GetDriverLibrary() {
        return {};
    }

    class Scoped {
    public:
        explicit Scoped(GraphicsContext& context_) : context(context_) {
            context.MakeCurrent();
        }
        ~Scoped() {
            context.DoneCurrent();
        }

    private:
        GraphicsContext& context;
    };

    /// Calls MakeCurrent on the context and calls DoneCurrent when the scope for the returned value
    /// ends
    [[nodiscard]] Scoped Acquire() {
        return Scoped{*this};
    }
};

/**
 * Abstraction class used to provide an interface between emulation code and the frontend
 * (e.g. SDL, QGLWidget, GLFW, etc...).
 *
 * Design notes on the interaction between EmuWindow and the emulation core:
 * - Generally, decisions on anything visible to the user should be left up to the GUI.
 *   For example, the emulation core should not try to dictate some window title or size.
 *   This stuff is not the core's business and only causes problems with regards to thread-safety
 *   anyway.
 * - Under certain circumstances, it may be desirable for the core to politely request the GUI
 *   to set e.g. a minimum window size. However, the GUI should always be free to ignore any
 *   such hints.
 * - EmuWindow may expose some of its state as read-only to the emulation core, however care
 *   should be taken to make sure the provided information is self-consistent. This requires
 *   some sort of synchronization (most of this is still a TODO).
 * - DO NOT TREAT THIS CLASS AS A GUI TOOLKIT ABSTRACTION LAYER. That's not what it is. Please
 *   re-read the upper points again and think about it if you don't see this.
 */
class EmuWindow : public GraphicsContext {
public:
    class TouchState;

    /// Data structure to store emuwindow configuration
    struct WindowConfig {
        bool fullscreen = false;
        int res_width = 0;
        int res_height = 0;
        std::pair<unsigned, unsigned> min_client_area_size{
            Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight};
    };

    /// Data describing host window system information
    struct WindowSystemInfo {
        // Window system type. Determines which GL context or Vulkan WSI is used.
        WindowSystemType type = WindowSystemType::Headless;

        // Connection to a display server. This is used on X11 and Wayland platforms.
        void* display_connection = nullptr;

        // Render surface. This is a pointer to the native window handle, which depends
        // on the platform. e.g. HWND for Windows, Window for X11. If the surface is
        // set to nullptr, the video backend will run in headless mode.
        void* render_surface = nullptr;

        // Scale of the render surface. For hidpi systems, this will be >1.
        float render_surface_scale = 1.0f;
    };

    /// Polls window events
    virtual void PollEvents() = 0;

    /**
     * Returns a GraphicsContext that the frontend provides that is shared with the emu window. This
     * context can be used from other threads for background graphics computation. If the frontend
     * is using a graphics backend that doesn't need anything specific to run on a different thread,
     * then it can use a stubbed implemenation for GraphicsContext.
     *
     * If the return value is null, then the core should assume that the frontend cannot provide a
     * Shared Context
     */
    virtual std::unique_ptr<GraphicsContext> CreateSharedContext() const {
        return nullptr;
    }

    /**
     * Save current GraphicsContext.
     */
    virtual void SaveContext() {};

    /**
     * Restore saved GraphicsContext.
     */
    virtual void RestoreContext() {};

    /**
     * Signal that a touch pressed event has occurred (e.g. mouse click pressed)
     * @param framebuffer_x Framebuffer x-coordinate that was pressed
     * @param framebuffer_y Framebuffer y-coordinate that was pressed
     * @returns True if the coordinates are within the touchpad, otherwise false
     */
    bool TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y);

    /// Signal that a touch released event has occurred (e.g. mouse click released)
    void TouchReleased();

    /**
     * Signal that a touch movement event has occurred (e.g. mouse was moved over the emu window)
     * @param framebuffer_x Framebuffer x-coordinate
     * @param framebuffer_y Framebuffer y-coordinate
     */
    void TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y);

    /**
     * Returns currently active configuration.
     * @note Accesses to the returned object need not be consistent because it may be modified in
     * another thread
     */
    const WindowConfig& GetActiveConfig() const {
        return active_config;
    }

    bool StrictContextRequired() const {
        return strict_context_required;
    }

    /**
     * Requests the internal configuration to be replaced by the specified argument at some point in
     * the future.
     * @note This method is thread-safe, because it delays configuration changes to the GUI event
     * loop. Hence there is no guarantee on when the requested configuration will be active.
     */
    void SetConfig(const WindowConfig& val) {
        config = val;
    }

    /**
     * Returns system information about the drawing area.
     */
    const WindowSystemInfo& GetWindowInfo() const {
        return window_info;
    }

    /**
     * Gets the framebuffer layout (width, height, and screen regions)
     * @note This method is thread-safe
     */
    const Layout::FramebufferLayout& GetFramebufferLayout() const {
        return framebuffer_layout;
    }

    /**
     * Convenience method to update the current frame layout
     * Read from the current settings to determine which layout to use.
     */
    void UpdateCurrentFramebufferLayout(unsigned width, unsigned height,
                                        bool is_portrait_mode = {});

    std::unique_ptr<TextureMailbox> mailbox = nullptr;

protected:
    EmuWindow();
    EmuWindow(bool is_secondary);
    virtual ~EmuWindow();

    /**
     * Processes any pending configuration changes from the last SetConfig call.
     * This method invokes OnMinimalClientAreaChangeRequest if the corresponding configuration
     * field changed.
     * @note Implementations will usually want to call this from the GUI thread.
     * @todo Actually call this in existing implementations.
     */
    void ProcessConfigurationChanges() {
        // TODO: For proper thread safety, we should eventually implement a proper
        // multiple-writer/single-reader queue...

        if (config.min_client_area_size != active_config.min_client_area_size) {
            OnMinimalClientAreaChangeRequest(config.min_client_area_size);
            active_config.min_client_area_size = config.min_client_area_size;
        }
    }

    /**
     * Update framebuffer layout with the given parameter.
     * @note EmuWindow implementations will usually use this in window resize event handlers.
     */
    void NotifyFramebufferLayoutChanged(const Layout::FramebufferLayout& layout) {
        framebuffer_layout = layout;
    }

    bool is_secondary{};
    bool strict_context_required{};
    WindowSystemInfo window_info;

private:
    /**
     * Handler called when the minimal client area was requested to be changed via SetConfig.
     * For the request to be honored, EmuWindow implementations will usually reimplement this
     * function.
     */
    virtual void OnMinimalClientAreaChangeRequest(
        [[maybe_unused]] std::pair<u32, u32> minimal_size) {
        // By default, ignore this request and do nothing.
    }

    void CreateTouchState();

    /**
     * Check if the given x/y coordinates are within the touchpad specified by the framebuffer
     * layout
     * @param layout FramebufferLayout object describing the framebuffer size and screen positions
     * @param framebuffer_x Framebuffer x-coordinate to check
     * @param framebuffer_y Framebuffer y-coordinate to check
     * @return True if the coordinates are within the touchpad, otherwise false
     */
    bool IsWithinTouchscreen(const Layout::FramebufferLayout& layout, unsigned framebuffer_x,
                             unsigned framebuffer_y);

    Layout::FramebufferLayout framebuffer_layout; ///< Current framebuffer layout

    WindowConfig config{};        ///< Internal configuration (changes pending for being applied in
                                  /// ProcessConfigurationChanges)
    WindowConfig active_config{}; ///< Internal active configuration

    std::shared_ptr<TouchState> touch_state;

    /**
     * Clip the provided coordinates to be inside the touchscreen area.
     */
    std::tuple<unsigned, unsigned> ClipToTouchScreen(unsigned new_x, unsigned new_y) const;

    void UpdateMinimumWindowSize(std::pair<unsigned, unsigned> min_size);
};

} // namespace Frontend
