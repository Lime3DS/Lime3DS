// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_vulkan/vk_descriptor_pool.h"

namespace VideoCore {
struct TextureBlit;
struct TextureCopy;
struct BufferTextureCopy;
} // namespace VideoCore

namespace Vulkan {

class Instance;
class RenderManager;
class Scheduler;
class Surface;

class BlitHelper {
    friend class TextureRuntime;

public:
    BlitHelper(const Instance& instance, Scheduler& scheduler, DescriptorPool& pool,
               RenderManager& render_manager);
    ~BlitHelper();

    bool BlitDepthStencil(Surface& source, Surface& dest, const VideoCore::TextureBlit& blit);

    bool ConvertDS24S8ToRGBA8(Surface& source, Surface& dest, const VideoCore::TextureCopy& copy);

    bool DepthToBuffer(Surface& source, vk::Buffer buffer,
                       const VideoCore::BufferTextureCopy& copy);

private:
    vk::Pipeline MakeComputePipeline(vk::ShaderModule shader, vk::PipelineLayout layout);
    vk::Pipeline MakeDepthStencilBlitPipeline();

private:
    const Instance& instance;
    Scheduler& scheduler;
    RenderManager& render_manager;

    vk::Device device;
    vk::RenderPass r32_renderpass;

    DescriptorSetProvider compute_provider;
    DescriptorSetProvider compute_buffer_provider;
    DescriptorSetProvider two_textures_provider;
    vk::PipelineLayout compute_pipeline_layout;
    vk::PipelineLayout compute_buffer_pipeline_layout;
    vk::PipelineLayout two_textures_pipeline_layout;

    vk::ShaderModule full_screen_vert;
    vk::ShaderModule d24s8_to_rgba8_comp;
    vk::ShaderModule depth_to_buffer_comp;
    vk::ShaderModule blit_depth_stencil_frag;

    vk::Pipeline d24s8_to_rgba8_pipeline;
    vk::Pipeline depth_to_buffer_pipeline;
    vk::Pipeline depth_blit_pipeline;
    vk::Sampler linear_sampler;
    vk::Sampler nearest_sampler;
};

} // namespace Vulkan
