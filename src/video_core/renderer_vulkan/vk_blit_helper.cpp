// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/vector_math.h"
#include "video_core/renderer_vulkan/vk_blit_helper.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"

#include "video_core/host_shaders/format_reinterpreter/vulkan_d24s8_to_rgba8_comp.h"
#include "video_core/host_shaders/full_screen_triangle_vert.h"
#include "video_core/host_shaders/vulkan_blit_depth_stencil_frag.h"
#include "video_core/host_shaders/vulkan_depth_to_buffer_comp.h"

namespace Vulkan {

using VideoCore::PixelFormat;

namespace {
struct PushConstants {
    std::array<float, 2> tex_scale;
    std::array<float, 2> tex_offset;
};

struct ComputeInfo {
    Common::Vec2i src_offset;
    Common::Vec2i dst_offset;
    Common::Vec2i src_extent;
};

inline constexpr vk::PushConstantRange COMPUTE_PUSH_CONSTANT_RANGE{
    .stageFlags = vk::ShaderStageFlagBits::eCompute,
    .offset = 0,
    .size = sizeof(ComputeInfo),
};

constexpr std::array<vk::DescriptorSetLayoutBinding, 3> COMPUTE_BINDINGS = {{
    {0, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eCompute},
    {1, vk::DescriptorType::eSampledImage, 1, vk::ShaderStageFlagBits::eCompute},
    {2, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eCompute},
}};

constexpr std::array<vk::DescriptorSetLayoutBinding, 3> COMPUTE_BUFFER_BINDINGS = {{
    {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute},
    {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eCompute},
    {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
}};

constexpr std::array<vk::DescriptorSetLayoutBinding, 2> TWO_TEXTURES_BINDINGS = {{
    {0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
    {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment},
}};

inline constexpr vk::PushConstantRange PUSH_CONSTANT_RANGE{
    .stageFlags = vk::ShaderStageFlagBits::eVertex,
    .offset = 0,
    .size = sizeof(PushConstants),
};
constexpr vk::PipelineVertexInputStateCreateInfo PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO{
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = nullptr,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = nullptr,
};
constexpr vk::PipelineInputAssemblyStateCreateInfo PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO{
    .topology = vk::PrimitiveTopology::eTriangleList,
    .primitiveRestartEnable = VK_FALSE,
};
constexpr vk::PipelineViewportStateCreateInfo PIPELINE_VIEWPORT_STATE_CREATE_INFO{
    .viewportCount = 1,
    .pViewports = nullptr,
    .scissorCount = 1,
    .pScissors = nullptr,
};
constexpr vk::PipelineRasterizationStateCreateInfo PIPELINE_RASTERIZATION_STATE_CREATE_INFO{
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = vk::PolygonMode::eFill,
    .cullMode = vk::CullModeFlagBits::eBack,
    .frontFace = vk::FrontFace::eClockwise,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f,
    .lineWidth = 1.0f,
};
constexpr vk::PipelineMultisampleStateCreateInfo PIPELINE_MULTISAMPLE_STATE_CREATE_INFO{
    .rasterizationSamples = vk::SampleCountFlagBits::e1,
    .sampleShadingEnable = VK_FALSE,
    .minSampleShading = 0.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE,
};
constexpr std::array DYNAMIC_STATES{
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor,
};
constexpr vk::PipelineDynamicStateCreateInfo PIPELINE_DYNAMIC_STATE_CREATE_INFO{
    .dynamicStateCount = static_cast<u32>(DYNAMIC_STATES.size()),
    .pDynamicStates = DYNAMIC_STATES.data(),
};
constexpr vk::PipelineColorBlendStateCreateInfo PIPELINE_COLOR_BLEND_STATE_EMPTY_CREATE_INFO{
    .logicOpEnable = VK_FALSE,
    .logicOp = vk::LogicOp::eClear,
    .attachmentCount = 0,
    .pAttachments = nullptr,
    .blendConstants = std::array{0.0f, 0.0f, 0.0f, 0.0f},
};
constexpr vk::PipelineDepthStencilStateCreateInfo PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO{
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = vk::CompareOp::eAlways,
    .depthBoundsTestEnable = VK_FALSE,
    .stencilTestEnable = VK_FALSE,
    .front = vk::StencilOpState{},
    .back = vk::StencilOpState{},
    .minDepthBounds = 0.0f,
    .maxDepthBounds = 0.0f,
};

template <vk::Filter filter>
inline constexpr vk::SamplerCreateInfo SAMPLER_CREATE_INFO{
    .magFilter = filter,
    .minFilter = filter,
    .mipmapMode = vk::SamplerMipmapMode::eNearest,
    .addressModeU = vk::SamplerAddressMode::eClampToBorder,
    .addressModeV = vk::SamplerAddressMode::eClampToBorder,
    .addressModeW = vk::SamplerAddressMode::eClampToBorder,
    .mipLodBias = 0.0f,
    .anisotropyEnable = VK_FALSE,
    .maxAnisotropy = 0.0f,
    .compareEnable = VK_FALSE,
    .compareOp = vk::CompareOp::eNever,
    .minLod = 0.0f,
    .maxLod = 0.0f,
    .borderColor = vk::BorderColor::eFloatOpaqueWhite,
    .unnormalizedCoordinates = VK_FALSE,
};

constexpr vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo(
    const vk::DescriptorSetLayout* set_layout, bool compute = false) {
    return vk::PipelineLayoutCreateInfo{
        .setLayoutCount = 1,
        .pSetLayouts = set_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = (compute ? &COMPUTE_PUSH_CONSTANT_RANGE : &PUSH_CONSTANT_RANGE),
    };
}

constexpr std::array<vk::PipelineShaderStageCreateInfo, 2> MakeStages(
    vk::ShaderModule vertex_shader, vk::ShaderModule fragment_shader) {
    return std::array{
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertex_shader,
            .pName = "main",
        },
        vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragment_shader,
            .pName = "main",
        },
    };
}

constexpr vk::PipelineShaderStageCreateInfo MakeStages(vk::ShaderModule compute_shader) {
    return vk::PipelineShaderStageCreateInfo{
        .stage = vk::ShaderStageFlagBits::eCompute,
        .module = compute_shader,
        .pName = "main",
    };
}

} // Anonymous namespace

BlitHelper::BlitHelper(const Instance& instance_, Scheduler& scheduler_, DescriptorPool& pool,
                       RenderManager& render_manager_)
    : instance{instance_}, scheduler{scheduler_}, render_manager{render_manager_},
      device{instance.GetDevice()}, compute_provider{instance, pool, COMPUTE_BINDINGS},
      compute_buffer_provider{instance, pool, COMPUTE_BUFFER_BINDINGS},
      two_textures_provider{instance, pool, TWO_TEXTURES_BINDINGS},
      compute_pipeline_layout{
          device.createPipelineLayout(PipelineLayoutCreateInfo(&compute_provider.Layout(), true))},
      compute_buffer_pipeline_layout{device.createPipelineLayout(
          PipelineLayoutCreateInfo(&compute_buffer_provider.Layout(), true))},
      two_textures_pipeline_layout{
          device.createPipelineLayout(PipelineLayoutCreateInfo(&two_textures_provider.Layout()))},
      full_screen_vert{Compile(HostShaders::FULL_SCREEN_TRIANGLE_VERT,
                               vk::ShaderStageFlagBits::eVertex, device)},
      d24s8_to_rgba8_comp{Compile(HostShaders::VULKAN_D24S8_TO_RGBA8_COMP,
                                  vk::ShaderStageFlagBits::eCompute, device)},
      depth_to_buffer_comp{Compile(HostShaders::VULKAN_DEPTH_TO_BUFFER_COMP,
                                   vk::ShaderStageFlagBits::eCompute, device)},
      blit_depth_stencil_frag{Compile(HostShaders::VULKAN_BLIT_DEPTH_STENCIL_FRAG,
                                      vk::ShaderStageFlagBits::eFragment, device)},
      d24s8_to_rgba8_pipeline{MakeComputePipeline(d24s8_to_rgba8_comp, compute_pipeline_layout)},
      depth_to_buffer_pipeline{
          MakeComputePipeline(depth_to_buffer_comp, compute_buffer_pipeline_layout)},
      depth_blit_pipeline{MakeDepthStencilBlitPipeline()},
      linear_sampler{device.createSampler(SAMPLER_CREATE_INFO<vk::Filter::eLinear>)},
      nearest_sampler{device.createSampler(SAMPLER_CREATE_INFO<vk::Filter::eNearest>)} {

    if (instance.HasDebuggingToolAttached()) {
        SetObjectName(device, compute_pipeline_layout, "BlitHelper: compute_pipeline_layout");
        SetObjectName(device, compute_buffer_pipeline_layout,
                      "BlitHelper: compute_buffer_pipeline_layout");
        SetObjectName(device, two_textures_pipeline_layout,
                      "BlitHelper: two_textures_pipeline_layout");
        SetObjectName(device, full_screen_vert, "BlitHelper: full_screen_vert");
        SetObjectName(device, d24s8_to_rgba8_comp, "BlitHelper: d24s8_to_rgba8_comp");
        SetObjectName(device, depth_to_buffer_comp, "BlitHelper: depth_to_buffer_comp");
        SetObjectName(device, blit_depth_stencil_frag, "BlitHelper: blit_depth_stencil_frag");
        SetObjectName(device, d24s8_to_rgba8_pipeline, "BlitHelper: d24s8_to_rgba8_pipeline");
        SetObjectName(device, depth_to_buffer_pipeline, "BlitHelper: depth_to_buffer_pipeline");
        if (depth_blit_pipeline) {
            SetObjectName(device, depth_blit_pipeline, "BlitHelper: depth_blit_pipeline");
        }
        SetObjectName(device, linear_sampler, "BlitHelper: linear_sampler");
        SetObjectName(device, nearest_sampler, "BlitHelper: nearest_sampler");
    }
}

BlitHelper::~BlitHelper() {
    device.destroyPipelineLayout(compute_pipeline_layout);
    device.destroyPipelineLayout(compute_buffer_pipeline_layout);
    device.destroyPipelineLayout(two_textures_pipeline_layout);
    device.destroyShaderModule(full_screen_vert);
    device.destroyShaderModule(d24s8_to_rgba8_comp);
    device.destroyShaderModule(depth_to_buffer_comp);
    device.destroyShaderModule(blit_depth_stencil_frag);
    device.destroyPipeline(depth_to_buffer_pipeline);
    device.destroyPipeline(d24s8_to_rgba8_pipeline);
    device.destroyPipeline(depth_blit_pipeline);
    device.destroySampler(linear_sampler);
    device.destroySampler(nearest_sampler);
}

void BindBlitState(vk::CommandBuffer cmdbuf, vk::PipelineLayout layout,
                   const VideoCore::TextureBlit& blit) {
    const vk::Offset2D offset{
        .x = std::min<s32>(blit.dst_rect.left, blit.dst_rect.right),
        .y = std::min<s32>(blit.dst_rect.bottom, blit.dst_rect.top),
    };
    const vk::Extent2D extent{
        .width = blit.dst_rect.GetWidth(),
        .height = blit.dst_rect.GetHeight(),
    };
    const vk::Viewport viewport{
        .x = static_cast<float>(offset.x),
        .y = static_cast<float>(offset.y),
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    const vk::Rect2D scissor{
        .offset = offset,
        .extent = extent,
    };
    const float scale_x = static_cast<float>(blit.src_rect.GetWidth());
    const float scale_y = static_cast<float>(blit.src_rect.GetHeight());
    const PushConstants push_constants{
        .tex_scale = {scale_x, scale_y},
        .tex_offset = {static_cast<float>(blit.src_rect.left),
                       static_cast<float>(blit.src_rect.bottom)},
    };
    cmdbuf.setViewport(0, viewport);
    cmdbuf.setScissor(0, scissor);
    cmdbuf.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(push_constants),
                         &push_constants);
}

bool BlitHelper::BlitDepthStencil(Surface& source, Surface& dest,
                                  const VideoCore::TextureBlit& blit) {
    if (!instance.IsShaderStencilExportSupported()) {
        LOG_ERROR(Render_Vulkan, "Unable to emulate depth stencil images");
        return false;
    }

    const vk::Rect2D dst_render_area = {
        .offset = {0, 0},
        .extent = {dest.GetScaledWidth(), dest.GetScaledHeight()},
    };

    std::array<DescriptorData, 2> textures{};
    textures[0].image_info = vk::DescriptorImageInfo{
        .sampler = nearest_sampler,
        .imageView = source.DepthView(),
        .imageLayout = vk::ImageLayout::eGeneral,
    };
    textures[1].image_info = vk::DescriptorImageInfo{
        .sampler = nearest_sampler,
        .imageView = source.StencilView(),
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    const auto descriptor_set = two_textures_provider.Acquire(textures);

    const RenderPass depth_pass = {
        .framebuffer = dest.Framebuffer(),
        .render_pass =
            render_manager.GetRenderpass(PixelFormat::Invalid, dest.pixel_format, false),
        .render_area = dst_render_area,
    };
    render_manager.BeginRendering(depth_pass);

    scheduler.Record([blit, descriptor_set, this](vk::CommandBuffer cmdbuf) {
        const vk::PipelineLayout layout = two_textures_pipeline_layout;

        cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, depth_blit_pipeline);
        cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, descriptor_set, {});
        BindBlitState(cmdbuf, layout, blit);
        cmdbuf.draw(3, 1, 0, 0);
    });
    scheduler.MakeDirty(StateFlags::Pipeline);
    return true;
}

bool BlitHelper::ConvertDS24S8ToRGBA8(Surface& source, Surface& dest,
                                      const VideoCore::TextureCopy& copy) {
    std::array<DescriptorData, 3> textures{};
    textures[0].image_info = vk::DescriptorImageInfo{
        .imageView = source.DepthView(),
        .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
    };
    textures[1].image_info = vk::DescriptorImageInfo{
        .imageView = source.StencilView(),
        .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
    };
    textures[2].image_info = vk::DescriptorImageInfo{
        .imageView = dest.ImageView(),
        .imageLayout = vk::ImageLayout::eGeneral,
    };

    const auto descriptor_set = compute_provider.Acquire(textures);

    render_manager.EndRendering();
    scheduler.Record([this, descriptor_set, copy, src_image = source.Image(),
                      dst_image = dest.Image()](vk::CommandBuffer cmdbuf) {
        const std::array pre_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = src_image,
                .subresourceRange{
                    .aspectMask =
                        vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eShaderWrite,
                .oldLayout = vk::ImageLayout::eUndefined,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = dst_image,
                .subresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            },
        };
        const std::array post_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eShaderRead,
                .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite |
                                 vk::AccessFlagBits::eDepthStencilAttachmentRead,
                .oldLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = src_image,
                .subresourceRange{
                    .aspectMask =
                        vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = dst_image,
                .subresourceRange{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_ARRAY_LAYERS,
                },
            }};
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests |
                                   vk::PipelineStageFlagBits::eLateFragmentTests,
                               vk::PipelineStageFlagBits::eComputeShader,
                               vk::DependencyFlagBits::eByRegion, {}, {}, pre_barriers);

        cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_pipeline_layout, 0,
                                  descriptor_set, {});
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, d24s8_to_rgba8_pipeline);

        const ComputeInfo info = {
            .src_offset = Common::Vec2i{static_cast<int>(copy.src_offset.x),
                                        static_cast<int>(copy.src_offset.y)},
            .dst_offset = Common::Vec2i{static_cast<int>(copy.dst_offset.x),
                                        static_cast<int>(copy.dst_offset.y)},
        };
        cmdbuf.pushConstants(compute_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
                             sizeof(info), &info);

        cmdbuf.dispatch(copy.extent.width / 8, copy.extent.height / 8, 1);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eEarlyFragmentTests |
                                   vk::PipelineStageFlagBits::eLateFragmentTests |
                                   vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, post_barriers);
    });
    return true;
}

bool BlitHelper::DepthToBuffer(Surface& source, vk::Buffer buffer,
                               const VideoCore::BufferTextureCopy& copy) {
    std::array<DescriptorData, 3> textures{};
    textures[0].image_info = vk::DescriptorImageInfo{
        .sampler = nearest_sampler,
        .imageView = source.DepthView(),
        .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
    };
    textures[1].image_info = vk::DescriptorImageInfo{
        .sampler = nearest_sampler,
        .imageView = source.StencilView(),
        .imageLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
    };
    textures[2].buffer_info = vk::DescriptorBufferInfo{
        .buffer = buffer,
        .offset = copy.buffer_offset,
        .range = copy.buffer_size,
    };

    const auto descriptor_set = compute_buffer_provider.Acquire(textures);

    render_manager.EndRendering();
    scheduler.Record([this, descriptor_set, copy, src_image = source.Image(),
                      extent = source.RealExtent(false)](vk::CommandBuffer cmdbuf) {
        const vk::ImageMemoryBarrier pre_barrier = {
            .srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eShaderRead,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = src_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        const vk::ImageMemoryBarrier post_barrier = {
            .srcAccessMask = vk::AccessFlagBits::eShaderRead,
            .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite |
                             vk::AccessFlagBits::eDepthStencilAttachmentRead,
            .oldLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = src_image,
            .subresourceRange{
                .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eEarlyFragmentTests |
                                   vk::PipelineStageFlagBits::eLateFragmentTests,
                               vk::PipelineStageFlagBits::eComputeShader,
                               vk::DependencyFlagBits::eByRegion, {}, {}, pre_barrier);

        cmdbuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, compute_buffer_pipeline_layout,
                                  0, descriptor_set, {});
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eCompute, depth_to_buffer_pipeline);

        const ComputeInfo info = {
            .src_offset = Common::Vec2i{static_cast<int>(copy.texture_rect.left),
                                        static_cast<int>(copy.texture_rect.bottom)},
            .src_extent =
                Common::Vec2i{static_cast<int>(extent.width), static_cast<int>(extent.height)},
        };
        cmdbuf.pushConstants(compute_buffer_pipeline_layout, vk::ShaderStageFlagBits::eCompute, 0,
                             sizeof(ComputeInfo), &info);

        cmdbuf.dispatch(copy.texture_rect.GetWidth() / 8, copy.texture_rect.GetHeight() / 8, 1);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                               vk::PipelineStageFlagBits::eEarlyFragmentTests |
                                   vk::PipelineStageFlagBits::eLateFragmentTests |
                                   vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, post_barrier);
    });
    return true;
}

vk::Pipeline BlitHelper::MakeComputePipeline(vk::ShaderModule shader, vk::PipelineLayout layout) {
    const vk::ComputePipelineCreateInfo compute_info = {
        .stage = MakeStages(shader),
        .layout = layout,
    };

    if (const auto result = device.createComputePipeline({}, compute_info);
        result.result == vk::Result::eSuccess) {
        return result.value;
    } else {
        LOG_CRITICAL(Render_Vulkan, "Compute pipeline creation failed!");
        UNREACHABLE();
    }
}

vk::Pipeline BlitHelper::MakeDepthStencilBlitPipeline() {
    if (!instance.IsShaderStencilExportSupported()) {
        return VK_NULL_HANDLE;
    }

    const std::array stages = MakeStages(full_screen_vert, blit_depth_stencil_frag);
    const auto renderpass = render_manager.GetRenderpass(VideoCore::PixelFormat::Invalid,
                                                           VideoCore::PixelFormat::D24S8, false);
    vk::GraphicsPipelineCreateInfo depth_stencil_info = {
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pInputAssemblyState = &PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pTessellationState = nullptr,
        .pViewportState = &PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pRasterizationState = &PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pMultisampleState = &PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pDepthStencilState = &PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pColorBlendState = &PIPELINE_COLOR_BLEND_STATE_EMPTY_CREATE_INFO,
        .pDynamicState = &PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .layout = two_textures_pipeline_layout,
        .renderPass = renderpass,
    };

    if (const auto result = device.createGraphicsPipeline({}, depth_stencil_info);
        result.result == vk::Result::eSuccess) {
        return result.value;
    } else {
        LOG_CRITICAL(Render_Vulkan, "Depth stencil blit pipeline creation failed!");
        UNREACHABLE();
    }
    return VK_NULL_HANDLE;
}

} // namespace Vulkan
