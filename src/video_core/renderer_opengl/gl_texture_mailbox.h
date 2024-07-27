// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>

#include "core/frontend/emu_window.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Frontend {
struct Frame {
    u32 width{};                      ///< Width of the frame (to detect resize)
    u32 height{};                     ///< Height of the frame
    bool color_reloaded = false;      ///< Texture attachment was recreated (ie: resized)
    OpenGL::OGLRenderbuffer color{};  ///< Buffer shared between the render/present FBO
    OpenGL::OGLFramebuffer render{};  ///< FBO created on the render thread
    OpenGL::OGLFramebuffer present{}; ///< FBO created on the present thread
    GLsync render_fence{};            ///< Fence created on the render thread
    GLsync present_fence{};           ///< Fence created on the presentation thread
};
} // namespace Frontend

namespace OpenGL {

// If the size of this is too small, it ends up creating a soft cap on FPS as the renderer will have
// to wait on available presentation frames. There doesn't seem to be much of a downside to a larger
// number but 9 swap textures at 60FPS presentation allows for 800% speed so thats probably fine
#ifdef ANDROID
// Reduce the size of swap_chain, since the UI only allows upto 200% speed.
constexpr std::size_t SWAP_CHAIN_SIZE = 6;
#else
constexpr std::size_t SWAP_CHAIN_SIZE = 9;
#endif

class OGLTextureMailbox : public Frontend::TextureMailbox {
public:
    explicit OGLTextureMailbox(bool has_debug_tool = false);
    ~OGLTextureMailbox() override;

    void ReloadPresentFrame(Frontend::Frame* frame, u32 height, u32 width) override;
    void ReloadRenderFrame(Frontend::Frame* frame, u32 width, u32 height) override;
    void ReleaseRenderFrame(Frontend::Frame* frame) override;

    Frontend::Frame* GetRenderFrame() override;
    Frontend::Frame* TryGetPresentFrame(int timeout_ms) override;

    /// This is virtual as it is to be overriden in OGLVideoDumpingMailbox below.
    virtual void LoadPresentFrame();

private:
    /// Signal that a new frame is available (called from GPU thread)
    void DebugNotifyNextFrame();

    /// Wait for a new frame to be available (called from presentation thread)
    void DebugWaitForNextFrame();

public:
    std::mutex swap_chain_lock;
    std::condition_variable free_cv;
    std::condition_variable present_cv;
    std::array<Frontend::Frame, SWAP_CHAIN_SIZE> swap_chain{};
    std::queue<Frontend::Frame*> free_queue{};
    std::deque<Frontend::Frame*> present_queue{};
    Frontend::Frame* previous_frame = nullptr;
    std::mutex debug_synch_mutex;
    std::condition_variable debug_synch_condition;
    std::atomic_int frame_for_debug{};
    const bool has_debug_tool; ///< When true, using a GPU debugger, so keep frames in lock-step
};

class OGLTextureMailboxException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// This mailbox is different in that it will never discard rendered frames
class OGLVideoDumpingMailbox : public OGLTextureMailbox {
public:
    void LoadPresentFrame() override;
    Frontend::Frame* GetRenderFrame() override;
    Frontend::Frame* TryGetPresentFrame(int timeout_ms) override;

public:
    bool quit = false;
};

} // namespace OpenGL
