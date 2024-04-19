// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_present_window.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Core {
class System;
}

namespace Memory {
class MemorySystem;
}

namespace Pica {
class PicaCore;
}

namespace Layout {
struct FramebufferLayout;
}

namespace VideoCore {
class GPU;
}

namespace Vulkan {

struct TextureInfo {
    u32 width;
    u32 height;
    Pica::PixelFormat format;
    vk::Image image;
    vk::ImageView image_view;
    VmaAllocation allocation;
};

struct ScreenInfo {
    TextureInfo texture;
    Common::Rectangle<f32> texcoords;
    vk::ImageView image_view;
};

struct PresentUniformData {
    std::array<f32, 4 * 4> modelview;
    Common::Vec4f i_resolution;
    Common::Vec4f o_resolution;
    int screen_id_l = 0;
    int screen_id_r = 0;
    int layer = 0;
    int reverse_interlaced = 0;
};
static_assert(sizeof(PresentUniformData) == 112,
              "PresentUniformData does not structure in shader!");

class RendererVulkan : public VideoCore::RendererBase {
    static constexpr std::size_t PRESENT_PIPELINES = 3;

public:
    explicit RendererVulkan(Core::System& system, Pica::PicaCore& pica, Frontend::EmuWindow& window,
                            Frontend::EmuWindow* secondary_window);
    ~RendererVulkan() override;

    [[nodiscard]] VideoCore::RasterizerInterface* Rasterizer() override {
        return &rasterizer;
    }

    void NotifySurfaceChanged() override {
        main_window.NotifySurfaceChanged();
    }

    void SwapBuffers() override;
    void TryPresent(int timeout_ms, bool is_secondary) override {}
    void Sync() override;

private:
    void ReloadPipeline();
    void CompileShaders();
    void BuildLayouts();
    void BuildPipelines();
    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const Pica::FramebufferConfig& framebuffer);
    void ConfigureRenderPipeline();
    void PrepareRendertarget();
    void RenderScreenshot();
    void RenderScreenshotWithStagingCopy();
    bool TryRenderScreenshotWithHostMemory();
    void PrepareDraw(Frame* frame, const Layout::FramebufferLayout& layout);
    void RenderToWindow(PresentWindow& window, const Layout::FramebufferLayout& layout,
                        bool flipped);

    void DrawScreens(Frame* frame, const Layout::FramebufferLayout& layout, bool flipped);
    void DrawBottomScreen(const Layout::FramebufferLayout& layout,
                          const Common::Rectangle<u32>& bottom_screen);
    void DrawTopScreen(const Layout::FramebufferLayout& layout,
                       const Common::Rectangle<u32>& top_screen);
    void DrawSingleScreen(u32 screen_id, float x, float y, float w, float h,
                          Layout::DisplayOrientation orientation);
    void DrawSingleScreenStereo(u32 screen_id_l, u32 screen_id_r, float x, float y, float w,
                                float h, Layout::DisplayOrientation orientation);
    void LoadFBToScreenInfo(const Pica::FramebufferConfig& framebuffer, ScreenInfo& screen_info,
                            bool right_eye);
    void FillScreen(Common::Vec3<u8> color, const TextureInfo& texture);

private:
    Memory::MemorySystem& memory;
    Pica::PicaCore& pica;

    Instance instance;
    Scheduler scheduler;
    RenderManager renderpass_cache;
    PresentWindow main_window;
    StreamBuffer vertex_buffer;
    DescriptorUpdateQueue update_queue;
    RasterizerVulkan rasterizer;
    std::unique_ptr<PresentWindow> second_window;

    DescriptorHeap present_heap;
    vk::UniquePipelineLayout present_pipeline_layout;
    std::array<vk::Pipeline, PRESENT_PIPELINES> present_pipelines;
    std::array<vk::ShaderModule, PRESENT_PIPELINES> present_shaders;
    std::array<vk::Sampler, 2> present_samplers;
    vk::ShaderModule present_vertex_shader;
    u32 current_pipeline = 0;

    std::array<ScreenInfo, 3> screen_infos{};
    PresentUniformData draw_info{};
    vk::ClearColorValue clear_color{};
};

} // namespace Vulkan
