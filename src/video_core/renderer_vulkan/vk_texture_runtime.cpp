// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/container/small_vector.hpp>
#include <boost/container/static_vector.hpp>

#include "common/literals.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "video_core/custom_textures/material.h"
#include "video_core/rasterizer_cache/texture_codec.h"
#include "video_core/rasterizer_cache/utils.h"
#include "video_core/renderer_vulkan/pica_to_vk.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_render_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_format_traits.hpp>

// Ignore the -Wclass-memaccess warning on memcpy for non-trivially default constructible objects.
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

MICROPROFILE_DEFINE(Vulkan_ImageAlloc, "Vulkan", "Texture Allocation", MP_RGB(192, 52, 235));

namespace Vulkan {

namespace {

using VideoCore::MapType;
using VideoCore::PixelFormat;
using VideoCore::SurfaceType;
using VideoCore::TextureType;
using namespace Common::Literals;

struct RecordParams {
    vk::ImageAspectFlags aspect;
    vk::Filter filter;
    vk::PipelineStageFlags pipeline_flags;
    vk::AccessFlags src_access;
    vk::AccessFlags dst_access;
    vk::Image src_image;
    vk::Image dst_image;
};

vk::Filter MakeFilter(VideoCore::PixelFormat pixel_format) {
    switch (pixel_format) {
    case VideoCore::PixelFormat::D16:
    case VideoCore::PixelFormat::D24:
    case VideoCore::PixelFormat::D24S8:
        return vk::Filter::eNearest;
    default:
        return vk::Filter::eLinear;
    }
}

[[nodiscard]] vk::ClearValue MakeClearValue(VideoCore::ClearValue clear) {
    static_assert(sizeof(VideoCore::ClearValue) == sizeof(vk::ClearValue));

    vk::ClearValue value{};
    std::memcpy(&value, &clear, sizeof(vk::ClearValue));
    return value;
}

[[nodiscard]] vk::ClearColorValue MakeClearColorValue(Common::Vec4f color) {
    return vk::ClearColorValue{
        .float32 = std::array{color[0], color[1], color[2], color[3]},
    };
}

[[nodiscard]] vk::ClearDepthStencilValue MakeClearDepthStencilValue(VideoCore::ClearValue clear) {
    return vk::ClearDepthStencilValue{
        .depth = clear.depth,
        .stencil = clear.stencil,
    };
}

u32 UnpackDepthStencil(const VideoCore::StagingData& data, vk::Format dest) {
    u32 depth_offset = 0;
    u32 stencil_offset = 4 * data.size / 5;
    const auto& mapped = data.mapped;

    switch (dest) {
    case vk::Format::eD24UnormS8Uint: {
        for (; stencil_offset < data.size; depth_offset += 4) {
            u8* ptr = mapped.data() + depth_offset;
            const u32 d24s8 = VideoCore::MakeInt<u32>(ptr);
            const u32 d24 = d24s8 >> 8;
            mapped[stencil_offset] = d24s8 & 0xFF;
            std::memcpy(ptr, &d24, 4);
            stencil_offset++;
        }
        break;
    }
    case vk::Format::eD32SfloatS8Uint: {
        for (; stencil_offset < data.size; depth_offset += 4) {
            u8* ptr = mapped.data() + depth_offset;
            const u32 d24s8 = VideoCore::MakeInt<u32>(ptr);
            const float d32 = (d24s8 >> 8) / 16777215.f;
            mapped[stencil_offset] = d24s8 & 0xFF;
            std::memcpy(ptr, &d32, 4);
            stencil_offset++;
        }
        break;
    }
    default:
        LOG_ERROR(Render_Vulkan, "Unimplemented convertion for depth format {}",
                  vk::to_string(dest));
        UNREACHABLE();
    }

    ASSERT(depth_offset == 4 * data.size / 5);
    return depth_offset;
}

boost::container::small_vector<vk::ImageMemoryBarrier, 3> MakeInitBarriers(
    vk::ImageAspectFlags aspect, std::span<const vk::Image> images) {
    boost::container::small_vector<vk::ImageMemoryBarrier, 3> barriers;
    for (const vk::Image& image : images) {
        barriers.push_back(vk::ImageMemoryBarrier{
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eNone,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange{
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        });
    }
    return barriers;
}

Handle MakeHandle(const Instance* instance, u32 width, u32 height, u32 levels, TextureType type,
                  vk::Format format, vk::ImageUsageFlags usage, vk::ImageCreateFlags flags,
                  vk::ImageAspectFlags aspect, bool need_format_list,
                  std::string_view debug_name = {}) {
    const u32 layers = type == TextureType::CubeMap ? 6 : 1;

    const std::array format_list = {
        vk::Format::eR8G8B8A8Unorm,
        vk::Format::eR32Uint,
    };
    const vk::ImageFormatListCreateInfo image_format_list = {
        .viewFormatCount = static_cast<u32>(format_list.size()),
        .pViewFormats = format_list.data(),
    };

    const vk::ImageCreateInfo image_info = {
        .pNext = need_format_list ? &image_format_list : nullptr,
        .flags = flags,
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = levels,
        .arrayLayers = layers,
        .samples = vk::SampleCountFlagBits::e1,
        .usage = usage,
    };

    const VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = 0,
        .preferredFlags = 0,
        .pool = VK_NULL_HANDLE,
        .pUserData = nullptr,
    };

    VkImage unsafe_image{};
    VkImageCreateInfo unsafe_image_info = static_cast<VkImageCreateInfo>(image_info);
    VmaAllocation allocation{};

    VkResult result = vmaCreateImage(instance->GetAllocator(), &unsafe_image_info, &alloc_info,
                                     &unsafe_image, &allocation, nullptr);
    if (result != VK_SUCCESS) [[unlikely]] {
        LOG_CRITICAL(Render_Vulkan, "Failed allocating image with error {}", result);
        UNREACHABLE();
    }

    const vk::Image image{unsafe_image};
    const vk::ImageViewCreateInfo view_info = {
        .image = image,
        .viewType =
            type == TextureType::CubeMap ? vk::ImageViewType::eCube : vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange{
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = levels,
            .baseArrayLayer = 0,
            .layerCount = layers,
        },
    };
    vk::UniqueImageView image_view = instance->GetDevice().createImageViewUnique(view_info);

    if (!debug_name.empty() && instance->HasDebuggingToolAttached()) {
        SetObjectName(instance->GetDevice(), image, debug_name);
        SetObjectName(instance->GetDevice(), image_view.get(), "{} View({})", debug_name,
                      vk::to_string(aspect));
    }

    return Handle{
        .alloc = allocation,
        .image = image,
        .image_view = std::move(image_view),
    };
}

vk::UniqueFramebuffer MakeFramebuffer(vk::Device device, vk::RenderPass render_pass, u32 width,
                                      u32 height, std::span<const vk::ImageView> attachments) {
    const vk::FramebufferCreateInfo framebuffer_info = {
        .renderPass = render_pass,
        .attachmentCount = static_cast<u32>(attachments.size()),
        .pAttachments = attachments.data(),
        .width = width,
        .height = height,
        .layers = 1,
    };
    return device.createFramebufferUnique(framebuffer_info);
}

vk::ImageSubresourceRange MakeSubresourceRange(vk::ImageAspectFlags aspect, u32 level = 0,
                                               u32 levels = 1, u32 layer = 0) {
    return vk::ImageSubresourceRange{
        .aspectMask = aspect,
        .baseMipLevel = level,
        .levelCount = levels,
        .baseArrayLayer = layer,
        .layerCount = VK_REMAINING_ARRAY_LAYERS,
    };
}

constexpr u64 UPLOAD_BUFFER_SIZE = 512_MiB;
constexpr u64 DOWNLOAD_BUFFER_SIZE = 16_MiB;

} // Anonymous namespace

TextureRuntime::TextureRuntime(const Instance& instance, Scheduler& scheduler,
                               RenderManager& render_manager, DescriptorPool& pool,
                               DescriptorSetProvider& texture_provider_, u32 num_swapchain_images_)
    : instance{instance}, scheduler{scheduler}, render_manager{render_manager},
      texture_provider{texture_provider_}, blit_helper{instance, scheduler, pool, render_manager},
      upload_buffer{instance, scheduler, vk::BufferUsageFlagBits::eTransferSrc, UPLOAD_BUFFER_SIZE,
                    BufferType::Upload},
      download_buffer{instance, scheduler,
                      vk::BufferUsageFlagBits::eTransferDst |
                          vk::BufferUsageFlagBits::eStorageBuffer,
                      DOWNLOAD_BUFFER_SIZE, BufferType::Download},
      num_swapchain_images{num_swapchain_images_} {}

TextureRuntime::~TextureRuntime() = default;

VideoCore::StagingData TextureRuntime::FindStaging(u32 size, bool upload) {
    StreamBuffer& buffer = upload ? upload_buffer : download_buffer;
    const auto [data, offset, invalidate] = buffer.Map(size, 16);
    return VideoCore::StagingData{
        .size = size,
        .offset = static_cast<u32>(offset),
        .mapped = std::span{data, size},
    };
}

u32 TextureRuntime::RemoveThreshold() {
    return num_swapchain_images;
}

void TextureRuntime::Finish() {
    scheduler.Finish();
}

bool TextureRuntime::Reinterpret(Surface& source, Surface& dest,
                                 const VideoCore::TextureCopy& copy) {
    const PixelFormat src_format = source.pixel_format;
    const PixelFormat dst_format = dest.pixel_format;
    ASSERT_MSG(src_format != dst_format, "Reinterpretation with the same format is invalid");

    if (!source.traits.needs_conversion && !dest.traits.needs_conversion &&
        source.type == dest.type) {
        CopyTextures(source, dest, copy);
        return true;
    }

    if (src_format == PixelFormat::D24S8 && dst_format == PixelFormat::RGBA8) {
        blit_helper.ConvertDS24S8ToRGBA8(source, dest, copy);
    } else {
        LOG_WARNING(Render_Vulkan, "Unimplemented reinterpretation {} -> {}",
                    VideoCore::PixelFormatAsString(src_format),
                    VideoCore::PixelFormatAsString(dst_format));
        return false;
    }
    return true;
}

bool TextureRuntime::ClearTexture(Surface& surface, const VideoCore::TextureClear& clear) {
    render_manager.EndRendering();

    const RecordParams params = {
        .aspect = surface.Aspect(),
        .pipeline_flags = surface.PipelineStageFlags(),
        .src_access = surface.AccessFlags(),
        .src_image = surface.Image(),
    };

    if (clear.texture_rect == surface.GetScaledRect()) {
        scheduler.Record([params, clear](vk::CommandBuffer cmdbuf) {
            const vk::ImageSubresourceRange range = {
                .aspectMask = params.aspect,
                .baseMipLevel = clear.texture_level,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            };

            const vk::ImageMemoryBarrier pre_barrier = {
                .srcAccessMask = params.src_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = range,
            };

            const vk::ImageMemoryBarrier post_barrier = {
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = params.src_access,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = range,
            };

            cmdbuf.pipelineBarrier(params.pipeline_flags, vk::PipelineStageFlagBits::eTransfer,
                                   vk::DependencyFlagBits::eByRegion, {}, {}, pre_barrier);

            const bool is_color =
                static_cast<bool>(params.aspect & vk::ImageAspectFlagBits::eColor);
            if (is_color) {
                cmdbuf.clearColorImage(params.src_image, vk::ImageLayout::eTransferDstOptimal,
                                       MakeClearColorValue(clear.value.color), range);
            } else {
                cmdbuf.clearDepthStencilImage(params.src_image,
                                              vk::ImageLayout::eTransferDstOptimal,
                                              MakeClearDepthStencilValue(clear.value), range);
            }

            cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.pipeline_flags,
                                   vk::DependencyFlagBits::eByRegion, {}, {}, post_barrier);
        });
        return true;
    }

    ClearTextureWithRenderpass(surface, clear);
    return true;
}

void TextureRuntime::ClearTextureWithRenderpass(Surface& surface,
                                                const VideoCore::TextureClear& clear) {
    const bool is_color = surface.type != VideoCore::SurfaceType::Depth &&
                          surface.type != VideoCore::SurfaceType::DepthStencil;

    const auto color_format = is_color ? surface.pixel_format : PixelFormat::Invalid;
    const auto depth_format = is_color ? PixelFormat::Invalid : surface.pixel_format;
    const auto render_pass = render_manager.GetRenderpass(color_format, depth_format, true);

    const RecordParams params = {
        .aspect = surface.Aspect(),
        .pipeline_flags = surface.PipelineStageFlags(),
        .src_access = surface.AccessFlags(),
        .src_image = surface.Image(),
    };

    scheduler.Record([params, is_color, clear, render_pass,
                      framebuffer = surface.Framebuffer()](vk::CommandBuffer cmdbuf) {
        const vk::AccessFlags access_flag =
            is_color ? vk::AccessFlagBits::eColorAttachmentRead |
                           vk::AccessFlagBits::eColorAttachmentWrite
                     : vk::AccessFlagBits::eDepthStencilAttachmentRead |
                           vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        const vk::PipelineStageFlags pipeline_flags =
            is_color ? vk::PipelineStageFlagBits::eColorAttachmentOutput
                     : vk::PipelineStageFlagBits::eEarlyFragmentTests;

        const vk::ImageMemoryBarrier pre_barrier = {
            .srcAccessMask = params.src_access,
            .dstAccessMask = access_flag,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = params.src_image,
            .subresourceRange = MakeSubresourceRange(params.aspect, clear.texture_level),
        };

        const vk::ImageMemoryBarrier post_barrier = {
            .srcAccessMask = access_flag,
            .dstAccessMask = params.src_access,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = params.src_image,
            .subresourceRange = MakeSubresourceRange(params.aspect, clear.texture_level),
        };

        const vk::Rect2D render_area = {
            .offset{
                .x = static_cast<s32>(clear.texture_rect.left),
                .y = static_cast<s32>(clear.texture_rect.bottom),
            },
            .extent{
                .width = clear.texture_rect.GetWidth(),
                .height = clear.texture_rect.GetHeight(),
            },
        };

        const auto clear_value = MakeClearValue(clear.value);

        const vk::RenderPassBeginInfo renderpass_begin_info = {
            .renderPass = render_pass,
            .framebuffer = framebuffer,
            .renderArea = render_area,
            .clearValueCount = 1,
            .pClearValues = &clear_value,
        };

        cmdbuf.pipelineBarrier(params.pipeline_flags, pipeline_flags,
                               vk::DependencyFlagBits::eByRegion, {}, {}, pre_barrier);

        cmdbuf.beginRenderPass(renderpass_begin_info, vk::SubpassContents::eInline);
        cmdbuf.endRenderPass();

        cmdbuf.pipelineBarrier(pipeline_flags, params.pipeline_flags,
                               vk::DependencyFlagBits::eByRegion, {}, {}, post_barrier);
    });
}

bool TextureRuntime::CopyTextures(Surface& source, Surface& dest,
                                  const VideoCore::TextureCopy& copy) {
    render_manager.EndRendering();

    const RecordParams params = {
        .aspect = source.Aspect(),
        .filter = MakeFilter(source.pixel_format),
        .pipeline_flags = source.PipelineStageFlags() | dest.PipelineStageFlags(),
        .src_access = source.AccessFlags(),
        .dst_access = dest.AccessFlags(),
        .src_image = source.Image(),
        .dst_image = dest.Image(),
    };

    scheduler.Record([params, copy](vk::CommandBuffer cmdbuf) {
        const vk::ImageCopy image_copy = {
            .srcSubresource{
                .aspectMask = params.aspect,
                .mipLevel = copy.src_level,
                .baseArrayLayer = copy.src_layer,
                .layerCount = 1,
            },
            .srcOffset = {static_cast<s32>(copy.src_offset.x), static_cast<s32>(copy.src_offset.y),
                          0},
            .dstSubresource{
                .aspectMask = params.aspect,
                .mipLevel = copy.dst_level,
                .baseArrayLayer = copy.dst_layer,
                .layerCount = 1,
            },
            .dstOffset = {static_cast<s32>(copy.dst_offset.x), static_cast<s32>(copy.dst_offset.y),
                          0},
            .extent = {copy.extent.width, copy.extent.height, 1},
        };

        const bool self_copy = params.src_image == params.dst_image;
        const vk::ImageLayout new_src_layout =
            self_copy ? vk::ImageLayout::eGeneral : vk::ImageLayout::eTransferSrcOptimal;
        const vk::ImageLayout new_dst_layout =
            self_copy ? vk::ImageLayout::eGeneral : vk::ImageLayout::eTransferDstOptimal;

        const std::array pre_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = params.src_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = new_src_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, copy.src_level),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = params.dst_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = new_dst_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.dst_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, copy.dst_level),
            },
        };
        const std::array post_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eNone,
                .oldLayout = new_src_layout,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, copy.src_level),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = params.dst_access,
                .oldLayout = new_dst_layout,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.dst_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, copy.dst_level),
            },
        };

        cmdbuf.pipelineBarrier(params.pipeline_flags, vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, pre_barriers);

        cmdbuf.copyImage(params.src_image, new_src_layout, params.dst_image, new_dst_layout,
                         image_copy);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.pipeline_flags,
                               vk::DependencyFlagBits::eByRegion, {}, {}, post_barriers);
    });

    return true;
}

bool TextureRuntime::BlitTextures(Surface& source, Surface& dest,
                                  const VideoCore::TextureBlit& blit) {
    const bool is_depth_stencil = source.type == VideoCore::SurfaceType::DepthStencil;
    const auto& depth_traits = instance.GetTraits(source.pixel_format);
    if (is_depth_stencil && !depth_traits.blit_support) {
        return blit_helper.BlitDepthStencil(source, dest, blit);
    }

    render_manager.EndRendering();

    const RecordParams params = {
        .aspect = source.Aspect(),
        .filter = MakeFilter(source.pixel_format),
        .pipeline_flags = source.PipelineStageFlags() | dest.PipelineStageFlags(),
        .src_access = source.AccessFlags(),
        .dst_access = dest.AccessFlags(),
        .src_image = source.Image(),
        .dst_image = dest.Image(),
    };

    scheduler.Record([params, blit](vk::CommandBuffer cmdbuf) {
        const std::array source_offsets = {
            vk::Offset3D{static_cast<s32>(blit.src_rect.left),
                         static_cast<s32>(blit.src_rect.bottom), 0},
            vk::Offset3D{static_cast<s32>(blit.src_rect.right), static_cast<s32>(blit.src_rect.top),
                         1},
        };

        const std::array dest_offsets = {
            vk::Offset3D{static_cast<s32>(blit.dst_rect.left),
                         static_cast<s32>(blit.dst_rect.bottom), 0},
            vk::Offset3D{static_cast<s32>(blit.dst_rect.right), static_cast<s32>(blit.dst_rect.top),
                         1},
        };

        const vk::ImageBlit blit_area = {
            .srcSubresource{
                .aspectMask = params.aspect,
                .mipLevel = blit.src_level,
                .baseArrayLayer = blit.src_layer,
                .layerCount = 1,
            },
            .srcOffsets = source_offsets,
            .dstSubresource{
                .aspectMask = params.aspect,
                .mipLevel = blit.dst_level,
                .baseArrayLayer = blit.dst_layer,
                .layerCount = 1,
            },
            .dstOffsets = dest_offsets,
        };

        const std::array read_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = params.src_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, blit.src_level),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = params.dst_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.dst_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, blit.dst_level),
            },
        };
        const std::array write_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferRead,
                .dstAccessMask = params.src_access,
                .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, blit.src_level),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = params.dst_access,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.dst_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, blit.dst_level),
            },
        };

        cmdbuf.pipelineBarrier(params.pipeline_flags, vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, read_barriers);

        cmdbuf.blitImage(params.src_image, vk::ImageLayout::eTransferSrcOptimal, params.dst_image,
                         vk::ImageLayout::eTransferDstOptimal, blit_area, params.filter);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.pipeline_flags,
                               vk::DependencyFlagBits::eByRegion, {}, {}, write_barriers);
    });

    return true;
}

void TextureRuntime::GenerateMipmaps(Surface& surface) {
    if (VideoCore::IsCustomFormatCompressed(surface.custom_format)) {
        LOG_ERROR(Render_Vulkan, "Generating mipmaps for compressed formats unsupported!");
        return;
    }

    render_manager.EndRendering();

    auto [width, height] = surface.RealExtent();
    const u32 levels = surface.levels;
    for (u32 i = 1; i < levels; i++) {
        const Common::Rectangle<u32> src_rect{0, height, width, 0};
        width = width > 1 ? width >> 1 : 1;
        height = height > 1 ? height >> 1 : 1;
        const Common::Rectangle<u32> dst_rect{0, height, width, 0};

        const VideoCore::TextureBlit blit = {
            .src_level = i - 1,
            .dst_level = i,
            .src_rect = src_rect,
            .dst_rect = dst_rect,
        };
        BlitTextures(surface, surface, blit);
    }
}

bool TextureRuntime::NeedsConversion(VideoCore::PixelFormat format) const {
    const FormatTraits traits = instance.GetTraits(format);
    return traits.needs_conversion &&
           // DepthStencil formats are handled elsewhere due to de-interleaving.
           traits.aspect != (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
}

void TextureRuntime::FreeDescriptorSetsWithImage(vk::ImageView image_view) {
    texture_provider.FreeWithImage(image_view);
    blit_helper.compute_provider.FreeWithImage(image_view);
    blit_helper.compute_buffer_provider.FreeWithImage(image_view);
    blit_helper.two_textures_provider.FreeWithImage(image_view);
}

Surface::Surface(TextureRuntime& runtime_, const VideoCore::SurfaceParams& params)
    : SurfaceBase{params}, runtime{&runtime_}, instance{&runtime_.GetInstance()},
      scheduler{&runtime_.GetScheduler()}, traits{instance->GetTraits(pixel_format)} {

    if (pixel_format == VideoCore::PixelFormat::Invalid) {
        return;
    }

    const bool is_mutable = pixel_format == VideoCore::PixelFormat::RGBA8;
    const vk::Format format = traits.native;

    ASSERT_MSG(format != vk::Format::eUndefined && levels >= 1,
               "Image allocation parameters are invalid");

    boost::container::static_vector<vk::Image, 3> raw_images;

    vk::ImageCreateFlags flags{};
    if (texture_type == VideoCore::TextureType::CubeMap) {
        flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }
    if (is_mutable) {
        flags |= vk::ImageCreateFlagBits::eMutableFormat;
    }

    const bool need_format_list = is_mutable && instance->IsImageFormatListSupported();
    handles[0] = MakeHandle(instance, width, height, levels, texture_type, format, traits.usage,
                            flags, traits.aspect, need_format_list, DebugName(false));
    raw_images.emplace_back(handles[0].image);

    if (res_scale != 1) {
        handles[1] =
            MakeHandle(instance, GetScaledWidth(), GetScaledHeight(), levels, texture_type, format,
                       traits.usage, flags, traits.aspect, need_format_list, DebugName(true));
        raw_images.emplace_back(handles[1].image);
    }

    runtime->render_manager.EndRendering();
    scheduler->Record([raw_images, num_images, aspect = traits.aspect](vk::CommandBuffer cmdbuf) {
        const auto barriers = MakeInitBarriers(aspect, raw_images, num_images);
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::DependencyFlagBits::eByRegion, {}, {}, barriers);
    });
}

Surface::Surface(TextureRuntime& runtime_, const VideoCore::SurfaceBase& surface,
                 const VideoCore::Material* mat)
    : SurfaceBase{surface}, runtime{&runtime_}, instance{&runtime_.GetInstance()},
      scheduler{&runtime_.GetScheduler()}, traits{instance->GetTraits(mat->format)} {
    if (!traits.transfer_support) {
        return;
    }

    const bool has_normal = mat && mat->Map(MapType::Normal);
    const vk::Format format = traits.native;

    boost::container::static_vector<vk::Image, 2> raw_images;

    vk::ImageCreateFlags flags{};
    if (texture_type == VideoCore::TextureType::CubeMap) {
        flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }

    const std::string debug_name = DebugName(false, true);
    handles[0] = MakeHandle(instance, mat->width, mat->height, levels, texture_type, format,
                            traits.usage, flags, traits.aspect, false, debug_name);
    raw_images.emplace_back(handles[0].image);

    if (res_scale != 1) {
        handles[1] = MakeHandle(instance, mat->width, mat->height, levels, texture_type,
                                vk::Format::eR8G8B8A8Unorm, traits.usage, flags, traits.aspect,
                                false, debug_name);
        raw_images.emplace_back(handles[1].image);
    }
    if (has_normal) {
        handles[2] = MakeHandle(instance, mat->width, mat->height, levels, texture_type, format,
                                traits.usage, flags, traits.aspect, false, debug_name);
        raw_images.emplace_back(handles[2].image);
    }

    runtime->render_manager.EndRendering();
    scheduler->Record([raw_images, num_images, aspect = traits.aspect](vk::CommandBuffer cmdbuf) {
        const auto barriers = MakeInitBarriers(aspect, raw_images, num_images);
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::PipelineStageFlagBits::eTopOfPipe,
                               vk::DependencyFlagBits::eByRegion, {}, {}, barriers);
    });

    custom_format = mat->format;
    material = mat;
}

Surface::~Surface() {
    if (!handles[0].image_view) {
        return;
    }
    for (const auto& [alloc, image, image_view] : handles) {
        if (image_view) {
            runtime->FreeDescriptorSetsWithImage(*image_view);
        }
        if (image) {
            vmaDestroyImage(instance->GetAllocator(), image, alloc);
        }
    }
    if (copy_handle.image_view) {
        vmaDestroyImage(instance->GetAllocator(), copy_handle.image, copy_handle.alloc);
    }
}

void Surface::Upload(const VideoCore::BufferTextureCopy& upload,
                     const VideoCore::StagingData& staging) {
    runtime->render_manager.EndRendering();

    const RecordParams params = {
        .aspect = Aspect(),
        .pipeline_flags = PipelineStageFlags(),
        .src_access = AccessFlags(),
        .src_image = Image(0),
    };

    scheduler->Record([buffer = runtime->upload_buffer.Handle(), format = traits.native, params,
                       staging, upload](vk::CommandBuffer cmdbuf) {
        boost::container::static_vector<vk::BufferImageCopy, 2> buffer_image_copies;

        const auto rect = upload.texture_rect;
        buffer_image_copies.emplace_back(vk::BufferImageCopy{
            .bufferOffset = upload.buffer_offset,
            .bufferRowLength = rect.GetWidth(),
            .bufferImageHeight = rect.GetHeight(),
            .imageSubresource{
                .aspectMask = params.aspect,
                .mipLevel = upload.texture_level,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {static_cast<s32>(rect.left), static_cast<s32>(rect.bottom), 0},
            .imageExtent = {rect.GetWidth(), rect.GetHeight(), 1},
        });

        if (params.aspect & vk::ImageAspectFlagBits::eStencil) {
            buffer_image_copies[0].imageSubresource.aspectMask = vk::ImageAspectFlagBits::eDepth;

            vk::BufferImageCopy& stencil_copy =
                buffer_image_copies.emplace_back(buffer_image_copies[0]);
            stencil_copy = buffer_image_copies[0];
            stencil_copy.bufferOffset += UnpackDepthStencil(staging, format);
            stencil_copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eStencil;
        }

        const vk::ImageMemoryBarrier read_barrier = {
            .srcAccessMask = params.src_access,
            .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eTransferDstOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = params.src_image,
            .subresourceRange = MakeSubresourceRange(params.aspect, upload.texture_level),
        };
        const vk::ImageMemoryBarrier write_barrier = {
            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
            .dstAccessMask = params.src_access,
            .oldLayout = vk::ImageLayout::eTransferDstOptimal,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = params.src_image,
            .subresourceRange = MakeSubresourceRange(params.aspect, upload.texture_level),
        };

        cmdbuf.pipelineBarrier(params.pipeline_flags, vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, read_barrier);

        cmdbuf.copyBufferToImage(buffer, params.src_image, vk::ImageLayout::eTransferDstOptimal,
                                 buffer_image_copies);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.pipeline_flags,
                               vk::DependencyFlagBits::eByRegion, {}, {}, write_barrier);
    });

    runtime->upload_buffer.Commit(staging.size);

    if (res_scale != 1) {
        const VideoCore::TextureBlit blit = {
            .src_level = upload.texture_level,
            .dst_level = upload.texture_level,
            .src_rect = upload.texture_rect,
            .dst_rect = upload.texture_rect * res_scale,
        };

        BlitScale(blit, true);
    }
}

void Surface::UploadCustom(const VideoCore::Material* material, u32 level) {
    const u32 width = material->width;
    const u32 height = material->height;
    const auto color = material->textures[0];
    const Common::Rectangle rect{0U, height, width, 0U};

    const auto upload = [&](u32 index, VideoCore::CustomTexture* texture) {
        const u64 custom_size = texture->data.size();
        const RecordParams params = {
            .aspect = vk::ImageAspectFlagBits::eColor,
            .pipeline_flags = PipelineStageFlags(),
            .src_access = AccessFlags(),
            .src_image = Image(index),
        };

        const auto [data, offset, invalidate] = runtime->upload_buffer.Map(custom_size, 0);
        std::memcpy(data, texture->data.data(), custom_size);
        runtime->upload_buffer.Commit(custom_size);

        scheduler->Record([buffer = runtime->upload_buffer.Handle(), level, params, rect,
                           offset = offset](vk::CommandBuffer cmdbuf) {
            const vk::BufferImageCopy buffer_image_copy = {
                .bufferOffset = offset,
                .bufferRowLength = 0,
                .bufferImageHeight = rect.GetHeight(),
                .imageSubresource{
                    .aspectMask = params.aspect,
                    .mipLevel = level,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageOffset = {static_cast<s32>(rect.left), static_cast<s32>(rect.bottom), 0},
                .imageExtent = {rect.GetWidth(), rect.GetHeight(), 1},
            };

            const vk::ImageMemoryBarrier read_barrier = {
                .srcAccessMask = params.src_access,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, level),
            };
            const vk::ImageMemoryBarrier write_barrier = {
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = params.src_access,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, level),
            };

            cmdbuf.pipelineBarrier(params.pipeline_flags, vk::PipelineStageFlagBits::eTransfer,
                                   vk::DependencyFlagBits::eByRegion, {}, {}, read_barrier);

            cmdbuf.copyBufferToImage(buffer, params.src_image, vk::ImageLayout::eTransferDstOptimal,
                                     buffer_image_copy);

            cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.pipeline_flags,
                                   vk::DependencyFlagBits::eByRegion, {}, {}, write_barrier);
        });
    };

    upload(0, color);

    for (u32 i = 1; i < VideoCore::MAX_MAPS; i++) {
        const auto texture = material->textures[i];
        if (!texture) {
            continue;
        }
        upload(i + 1, texture);
    }
}

void Surface::Download(const VideoCore::BufferTextureCopy& download,
                       const VideoCore::StagingData& staging) {
    SCOPE_EXIT({
        scheduler->Finish();
        runtime->download_buffer.Commit(staging.size);
    });

    runtime->render_manager.EndRendering();

    if (pixel_format == PixelFormat::D24S8) {
        runtime->blit_helper.DepthToBuffer(*this, runtime->download_buffer.Handle(), download);
        return;
    }

    if (res_scale != 1) {
        const VideoCore::TextureBlit blit = {
            .src_level = download.texture_level,
            .dst_level = download.texture_level,
            .src_rect = download.texture_rect * res_scale,
            .dst_rect = download.texture_rect,
        };

        BlitScale(blit, false);
    }

    const RecordParams params = {
        .aspect = Aspect(),
        .pipeline_flags = PipelineStageFlags(),
        .src_access = AccessFlags(),
        .src_image = Image(0),
    };

    scheduler->Record(
        [buffer = runtime->download_buffer.Handle(), params, download](vk::CommandBuffer cmdbuf) {
            const auto rect = download.texture_rect;
            const vk::BufferImageCopy buffer_image_copy = {
                .bufferOffset = download.buffer_offset,
                .bufferRowLength = rect.GetWidth(),
                .bufferImageHeight = rect.GetHeight(),
                .imageSubresource{
                    .aspectMask = params.aspect,
                    .mipLevel = download.texture_level,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageOffset = {static_cast<s32>(rect.left), static_cast<s32>(rect.bottom), 0},
                .imageExtent = {rect.GetWidth(), rect.GetHeight(), 1},
            };

            const vk::ImageMemoryBarrier read_barrier = {
                .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, download.texture_level),
            };
            const vk::ImageMemoryBarrier image_write_barrier = {
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eMemoryWrite,
                .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, download.texture_level),
            };
            const vk::MemoryBarrier memory_write_barrier = {
                .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite,
            };

            cmdbuf.pipelineBarrier(params.pipeline_flags, vk::PipelineStageFlagBits::eTransfer,
                                   vk::DependencyFlagBits::eByRegion, {}, {}, read_barrier);

            cmdbuf.copyImageToBuffer(params.src_image, vk::ImageLayout::eTransferSrcOptimal, buffer,
                                     buffer_image_copy);

            cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.pipeline_flags,
                                   vk::DependencyFlagBits::eByRegion, memory_write_barrier, {},
                                   image_write_barrier);
        });
}

void Surface::ScaleUp(u32 new_scale) {
    if (res_scale == new_scale || new_scale == 1) {
        return;
    }

    res_scale = new_scale;

    const bool is_mutable = pixel_format == VideoCore::PixelFormat::RGBA8;

    vk::ImageCreateFlags flags{};
    if (texture_type == VideoCore::TextureType::CubeMap) {
        flags |= vk::ImageCreateFlagBits::eCubeCompatible;
    }
    if (is_mutable) {
        flags |= vk::ImageCreateFlagBits::eMutableFormat;
    }

    handles[1] =
        MakeHandle(instance, GetScaledWidth(), GetScaledHeight(), levels, texture_type,
                   traits.native, traits.usage, flags, traits.aspect, false, DebugName(true));

    runtime->render_manager.EndRendering();
    scheduler->Record(
        [raw_images = std::array{Image()}, aspect = traits.aspect](vk::CommandBuffer cmdbuf) {
            const auto barriers = MakeInitBarriers(aspect, raw_images);
            cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::DependencyFlagBits::eByRegion, {}, {}, barriers);
        });

    for (u32 level = 0; level < levels; level++) {
        const VideoCore::TextureBlit blit = {
            .src_level = level,
            .dst_level = level,
            .src_rect = GetRect(level),
            .dst_rect = GetScaledRect(level),
        };
        BlitScale(blit, true);
    }
}

u32 Surface::GetInternalBytesPerPixel() const {
    // Request 5 bytes for D24S8 as well because we can use the
    // extra space when deinterleaving the data during upload
    if (traits.native == vk::Format::eD24UnormS8Uint) {
        return 5;
    }

    return vk::blockSize(traits.native);
}

vk::AccessFlags Surface::AccessFlags() const noexcept {
    const bool is_color = static_cast<bool>(Aspect() & vk::ImageAspectFlagBits::eColor);
    const vk::AccessFlags attachment_flags =
        is_color
            ? vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
            : vk::AccessFlagBits::eDepthStencilAttachmentRead |
                  vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    return vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eTransferRead |
           vk::AccessFlagBits::eTransferWrite |
           (is_framebuffer ? attachment_flags : vk::AccessFlagBits::eNone) |
           (is_storage ? vk::AccessFlagBits::eShaderWrite : vk::AccessFlagBits::eNone);
}

vk::PipelineStageFlags Surface::PipelineStageFlags() const noexcept {
    const bool is_color = static_cast<bool>(Aspect() & vk::ImageAspectFlagBits::eColor);
    const vk::PipelineStageFlags attachment_flags =
        is_color ? vk::PipelineStageFlagBits::eColorAttachmentOutput
                 : vk::PipelineStageFlagBits::eEarlyFragmentTests |
                       vk::PipelineStageFlagBits::eLateFragmentTests;

    return vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eFragmentShader |
           (is_framebuffer ? attachment_flags : vk::PipelineStageFlagBits::eNone) |
           (is_storage ? vk::PipelineStageFlagBits::eComputeShader
                       : vk::PipelineStageFlagBits::eNone);
}

vk::Image Surface::Image(u32 index) const noexcept {
    const vk::Image image = handles[index].image;
    if (!image) {
        return handles[0].image;
    }
    return image;
}

vk::ImageView Surface::CopyImageView() noexcept {
    vk::ImageLayout copy_layout = vk::ImageLayout::eGeneral;
    if (!copy_handle.image) {
        vk::ImageCreateFlags flags{};
        if (texture_type == VideoCore::TextureType::CubeMap) {
            flags |= vk::ImageCreateFlagBits::eCubeCompatible;
        }
        copy_handle =
            MakeHandle(instance, GetScaledWidth(), GetScaledHeight(), levels, texture_type,
                       traits.native, traits.usage, flags, traits.aspect, false);
        copy_layout = vk::ImageLayout::eUndefined;
    }

    runtime->render_manager.EndRendering();

    const RecordParams params = {
        .aspect = Aspect(),
        .pipeline_flags = PipelineStageFlags(),
        .src_access = AccessFlags(),
        .src_image = Image(),
        .dst_image = copy_handle.image,
    };

    scheduler->Record([params, copy_layout, levels = this->levels, width = GetScaledWidth(),
                       height = GetScaledHeight()](vk::CommandBuffer cmdbuf) {
        std::array pre_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, 0, levels),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eShaderRead,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = copy_layout,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.dst_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, 0, levels),
            },
        };
        std::array post_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferRead,
                .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
                .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.src_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, 0, levels),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eShaderRead,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = params.dst_image,
                .subresourceRange = MakeSubresourceRange(params.aspect, 0, levels),
            },
        };

        boost::container::small_vector<vk::ImageCopy, 3> image_copies;
        for (u32 level = 0; level < levels; level++) {
            image_copies.push_back(vk::ImageCopy{
                .srcSubresource{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = level,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .srcOffset = {0, 0, 0},
                .dstSubresource{
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .mipLevel = level,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .dstOffset = {0, 0, 0},
                .extent = {width >> level, height >> level, 1},
            });
        }

        cmdbuf.pipelineBarrier(params.pipeline_flags, vk::PipelineStageFlagBits::eTransfer,
                               vk::DependencyFlagBits::eByRegion, {}, {}, pre_barriers);

        cmdbuf.copyImage(params.src_image, vk::ImageLayout::eTransferSrcOptimal, params.dst_image,
                         vk::ImageLayout::eTransferDstOptimal, image_copies);

        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, params.pipeline_flags,
                               vk::DependencyFlagBits::eByRegion, {}, {}, post_barriers);
    });

    return copy_handle.image_view.get();
}

vk::ImageView Surface::ImageView(u32 index) const noexcept {
    const auto& image_view = handles[index].image_view.get();
    if (!image_view) {
        return handles[0].image_view.get();
    }
    return image_view;
}

vk::ImageView Surface::FramebufferView() noexcept {
    is_framebuffer = true;
    return ImageView();
}

vk::ImageView Surface::DepthView() noexcept {
    if (depth_view) {
        return depth_view.get();
    }

    const vk::ImageViewCreateInfo view_info = {
        .image = Image(),
        .viewType = vk::ImageViewType::e2D,
        .format = instance->GetTraits(pixel_format).native,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eDepth,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    depth_view = instance->GetDevice().createImageViewUnique(view_info);
    return depth_view.get();
}

vk::ImageView Surface::StencilView() noexcept {
    if (stencil_view) {
        return stencil_view.get();
    }

    const vk::ImageViewCreateInfo view_info = {
        .image = Image(),
        .viewType = vk::ImageViewType::e2D,
        .format = instance->GetTraits(pixel_format).native,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eStencil,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };

    stencil_view = instance->GetDevice().createImageViewUnique(view_info);
    return stencil_view.get();
}

vk::ImageView Surface::StorageView() noexcept {
    if (storage_view) {
        return storage_view.get();
    }

    if (pixel_format != VideoCore::PixelFormat::RGBA8) {
        LOG_WARNING(Render_Vulkan,
                    "Attempted to retrieve storage view from unsupported surface with format {}",
                    VideoCore::PixelFormatAsString(pixel_format));
        return ImageView();
    }

    is_storage = true;

    const vk::ImageViewCreateInfo storage_view_info = {
        .image = Image(),
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eR32Uint,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        },
    };
    storage_view = instance->GetDevice().createImageViewUnique(storage_view_info);
    return storage_view.get();
}

vk::Framebuffer Surface::Framebuffer() noexcept {
    const u32 index = res_scale == 1 ? 0u : 1u;
    if (framebuffers[index]) {
        return framebuffers[index].get();
    }

    const bool is_depth = type == SurfaceType::Depth || type == SurfaceType::DepthStencil;
    const auto color_format = is_depth ? PixelFormat::Invalid : pixel_format;
    const auto depth_format = is_depth ? pixel_format : PixelFormat::Invalid;
    const auto render_pass =
        runtime->render_manager.GetRenderpass(color_format, depth_format, false);
    const auto attachments = std::array{ImageView()};
    framebuffers[index] = MakeFramebuffer(instance->GetDevice(), render_pass, GetScaledWidth(),
                                          GetScaledHeight(), attachments);
    return framebuffers[index].get();
}

void Surface::BlitScale(const VideoCore::TextureBlit& blit, bool up_scale) {
    const FormatTraits& depth_traits = instance->GetTraits(pixel_format);
    const bool is_depth_stencil = pixel_format == PixelFormat::D24S8;
    if (is_depth_stencil && !depth_traits.blit_support) {
        LOG_WARNING(Render_Vulkan, "Depth scale unsupported by hardware");
        return;
    }

    scheduler->Record([src_image = Image(!up_scale), aspect = Aspect(),
                       filter = MakeFilter(pixel_format), dst_image = Image(up_scale),
                       blit](vk::CommandBuffer render_cmdbuf) {
        const std::array source_offsets = {
            vk::Offset3D{static_cast<s32>(blit.src_rect.left),
                         static_cast<s32>(blit.src_rect.bottom), 0},
            vk::Offset3D{static_cast<s32>(blit.src_rect.right), static_cast<s32>(blit.src_rect.top),
                         1},
        };

        const std::array dest_offsets = {
            vk::Offset3D{static_cast<s32>(blit.dst_rect.left),
                         static_cast<s32>(blit.dst_rect.bottom), 0},
            vk::Offset3D{static_cast<s32>(blit.dst_rect.right), static_cast<s32>(blit.dst_rect.top),
                         1},
        };

        const vk::ImageBlit blit_area = {
            .srcSubresource{
                .aspectMask = aspect,
                .mipLevel = blit.src_level,
                .baseArrayLayer = blit.src_layer,
                .layerCount = 1,
            },
            .srcOffsets = source_offsets,
            .dstSubresource{
                .aspectMask = aspect,
                .mipLevel = blit.dst_level,
                .baseArrayLayer = blit.dst_layer,
                .layerCount = 1,
            },
            .dstOffsets = dest_offsets,
        };

        const std::array read_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
                .dstAccessMask = vk::AccessFlagBits::eTransferRead,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferSrcOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = src_image,
                .subresourceRange = MakeSubresourceRange(aspect, blit.src_level),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eShaderRead |
                                 vk::AccessFlagBits::eDepthStencilAttachmentRead |
                                 vk::AccessFlagBits::eColorAttachmentRead |
                                 vk::AccessFlagBits::eTransferRead,
                .dstAccessMask = vk::AccessFlagBits::eTransferWrite,
                .oldLayout = vk::ImageLayout::eGeneral,
                .newLayout = vk::ImageLayout::eTransferDstOptimal,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = dst_image,
                .subresourceRange = MakeSubresourceRange(aspect, blit.dst_level),
            },
        };
        const std::array write_barriers = {
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eNone,
                .dstAccessMask = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead,
                .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = src_image,
                .subresourceRange = MakeSubresourceRange(aspect, blit.src_level),
            },
            vk::ImageMemoryBarrier{
                .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                .dstAccessMask = vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eMemoryRead,
                .oldLayout = vk::ImageLayout::eTransferDstOptimal,
                .newLayout = vk::ImageLayout::eGeneral,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = dst_image,
                .subresourceRange = MakeSubresourceRange(aspect, blit.dst_level),
            },
        };

        render_cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      vk::DependencyFlagBits::eByRegion, {}, {}, read_barriers);

        render_cmdbuf.blitImage(src_image, vk::ImageLayout::eTransferSrcOptimal, dst_image,
                                vk::ImageLayout::eTransferDstOptimal, blit_area, filter);

        render_cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
                                      vk::PipelineStageFlagBits::eAllCommands,
                                      vk::DependencyFlagBits::eByRegion, {}, {}, write_barriers);
    });
}

Framebuffer::Framebuffer(TextureRuntime& runtime, const VideoCore::FramebufferParams& params,
                         Surface* color, Surface* depth)
    : VideoCore::FramebufferParams{params}, res_scale{color ? color->res_scale
                                                            : (depth ? depth->res_scale : 1u)} {
    auto& render_manager = runtime.GetRenderpassCache();
    if (shadow_rendering && !color) {
        return;
    }

    width = height = std::numeric_limits<u32>::max();

    const auto prepare = [&](u32 index, Surface* surface) {
        const VideoCore::Extent extent = surface->RealExtent();
        width = std::min(width, extent.width);
        height = std::min(height, extent.height);
        if (!shadow_rendering) {
            formats[index] = surface->pixel_format;
        }
        images[index] = surface->Image();
        aspects[index] = surface->Aspect();
        image_views[index] = shadow_rendering ? surface->StorageView() : surface->FramebufferView();
    };

    boost::container::static_vector<vk::ImageView, 2> attachments;

    if (color) {
        prepare(0, color);
        attachments.emplace_back(image_views[0]);
    }

    if (depth) {
        prepare(1, depth);
        attachments.emplace_back(image_views[1]);
    }

    const vk::Device device = runtime.GetInstance().GetDevice();
    if (shadow_rendering) {
        render_pass =
            render_manager.GetRenderpass(PixelFormat::Invalid, PixelFormat::Invalid, false);
        framebuffer = MakeFramebuffer(device, render_pass, color->GetScaledWidth(),
                                      color->GetScaledHeight(), {});
    } else {
        render_pass = render_manager.GetRenderpass(formats[0], formats[1], false);
        framebuffer =
            MakeFramebuffer(device, render_pass, width, height, attachments, num_attachments);
    }
}

Framebuffer::~Framebuffer() = default;

Sampler::Sampler(TextureRuntime& runtime, const VideoCore::SamplerParams& params) {
    using TextureConfig = VideoCore::SamplerParams::TextureConfig;

    const Instance& instance = runtime.GetInstance();
    const vk::PhysicalDeviceProperties properties = instance.GetPhysicalDevice().getProperties();
    const bool use_border_color =
        instance.IsCustomBorderColorSupported() && (params.wrap_s == TextureConfig::ClampToBorder ||
                                                    params.wrap_t == TextureConfig::ClampToBorder);

    const Common::Vec4f color = PicaToVK::ColorRGBA8(params.border_color);
    const vk::SamplerCustomBorderColorCreateInfoEXT border_color_info = {
        .customBorderColor = MakeClearColorValue(color),
        .format = vk::Format::eUndefined,
    };

    const vk::Filter mag_filter = PicaToVK::TextureFilterMode(params.mag_filter);
    const vk::Filter min_filter = PicaToVK::TextureFilterMode(params.min_filter);
    const vk::SamplerMipmapMode mipmap_mode = PicaToVK::TextureMipFilterMode(params.mip_filter);
    const vk::SamplerAddressMode wrap_u = PicaToVK::WrapMode(params.wrap_s);
    const vk::SamplerAddressMode wrap_v = PicaToVK::WrapMode(params.wrap_t);
    const float lod_min = static_cast<float>(params.lod_min);
    const float lod_max = static_cast<float>(params.lod_max);

    const vk::SamplerCreateInfo sampler_info = {
        .pNext = use_border_color ? &border_color_info : nullptr,
        .magFilter = mag_filter,
        .minFilter = min_filter,
        .mipmapMode = mipmap_mode,
        .addressModeU = wrap_u,
        .addressModeV = wrap_v,
        .mipLodBias = 0,
        .anisotropyEnable = instance.IsAnisotropicFilteringSupported(),
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = false,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = lod_min,
        .maxLod = lod_max,
        .borderColor =
            use_border_color ? vk::BorderColor::eFloatCustomEXT : vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = false,
    };
    sampler = instance.GetDevice().createSamplerUnique(sampler_info);
}

Sampler::~Sampler() = default;

DebugScope::DebugScope(TextureRuntime& runtime, Common::Vec4f color, std::string_view label)
    : scheduler{runtime.GetScheduler()}, has_debug_tool{
                                             runtime.GetInstance().HasDebuggingToolAttached()} {
    if (!has_debug_tool) {
        return;
    }
    scheduler.Record([color, label = std::string(label)](vk::CommandBuffer cmdbuf) {
        const vk::DebugUtilsLabelEXT debug_label = {
            .pLabelName = label.data(),
            .color = std::array{color[0], color[1], color[2], color[3]},
        };
        cmdbuf.beginDebugUtilsLabelEXT(debug_label);
    });
}

DebugScope::~DebugScope() {
    if (!has_debug_tool) {
        return;
    }
    scheduler.Record([](vk::CommandBuffer cmdbuf) { cmdbuf.endDebugUtilsLabelEXT(); });
}

} // namespace Vulkan
