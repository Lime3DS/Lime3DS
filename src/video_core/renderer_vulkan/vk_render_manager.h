// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>

#include "common/math_util.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace VideoCore {
enum class PixelFormat : u32;
}

namespace Vulkan {

class Instance;
class Scheduler;
class Framebuffer;

struct RenderPass {
    vk::Framebuffer framebuffer;
    vk::RenderPass render_pass;
    vk::Rect2D render_area;
    vk::ClearValue clear;
    u32 do_clear;

    bool operator==(const RenderPass& other) const noexcept {
        return std::memcmp(this, &other, sizeof(RenderPass)) == 0;
    }
};

class RenderManager {
    static constexpr std::size_t MAX_COLOR_FORMATS = 13;
    static constexpr std::size_t MAX_DEPTH_FORMATS = 4;

public:
    explicit RenderManager(const Instance& instance, Scheduler& scheduler);
    ~RenderManager();

    /// Begins a new renderpass with the provided framebuffer as render target.
    void BeginRendering(const Framebuffer* framebuffer, Common::Rectangle<u32> draw_rect);

    /// Begins a new renderpass with the provided render state.
    void BeginRendering(const RenderPass& new_pass);

    /// Exits from any currently active renderpass instance
    void EndRendering();

    /// Returns the renderpass associated with the color-depth format pair
    vk::RenderPass GetRenderpass(VideoCore::PixelFormat color, VideoCore::PixelFormat depth,
                                 bool is_clear);

private:
    /// Creates a renderpass configured appropriately and stores it in cached_renderpasses
    vk::UniqueRenderPass CreateRenderPass(vk::Format color, vk::Format depth,
                                          vk::AttachmentLoadOp load_op) const;

private:
    const Instance& instance;
    Scheduler& scheduler;
    vk::UniqueRenderPass cached_renderpasses[MAX_COLOR_FORMATS + 1][MAX_DEPTH_FORMATS + 1][2];
    std::mutex cache_mutex;
    std::array<vk::Image, 2> images;
    std::array<vk::ImageAspectFlags, 2> aspects;
    RenderPass pass{};
    u32 num_draws{};
};

} // namespace Vulkan
