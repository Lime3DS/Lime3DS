// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include <span>
#include "video_core/rasterizer_cache/framebuffer_base.h"
#include "video_core/rasterizer_cache/rasterizer_cache_base.h"
#include "video_core/rasterizer_cache/surface_base.h"
#include "video_core/renderer_vulkan/vk_blit_helper.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"

VK_DEFINE_HANDLE(VmaAllocation)

namespace VideoCore {
struct Material;
}

namespace Vulkan {

class Instance;
class RenderManager;
class DescriptorPool;
class DescriptorSetProvider;
class Surface;

struct Handle {
    VmaAllocation alloc;
    vk::Image image;
    vk::UniqueImageView image_view;
};

/**
 * Provides texture manipulation functions to the rasterizer cache
 * Separating this into a class makes it easier to abstract graphics API code
 */
class TextureRuntime {
    friend class Surface;

public:
    explicit TextureRuntime(const Instance& instance, Scheduler& scheduler,
                            RenderManager& render_manager, DescriptorPool& pool,
                            DescriptorSetProvider& texture_provider, u32 num_swapchain_images);
    ~TextureRuntime();

    const Instance& GetInstance() const {
        return instance;
    }

    Scheduler& GetScheduler() const {
        return scheduler;
    }

    RenderManager& GetRenderpassCache() {
        return render_manager;
    }

    /// Returns the removal threshold ticks for the garbage collector
    u32 RemoveThreshold();

    /// Submits and waits for current GPU work.
    void Finish();

    /// Maps an internal staging buffer of the provided size for pixel uploads/downloads
    VideoCore::StagingData FindStaging(u32 size, bool upload);

    /// Attempts to reinterpret a rectangle of source to another rectangle of dest
    bool Reinterpret(Surface& source, Surface& dest, const VideoCore::TextureCopy& copy);

    /// Fills the rectangle of the texture with the clear value provided
    bool ClearTexture(Surface& surface, const VideoCore::TextureClear& clear);

    /// Copies a rectangle of src_tex to another rectange of dst_rect
    bool CopyTextures(Surface& source, Surface& dest, const VideoCore::TextureCopy& copy);

    /// Blits a rectangle of src_tex to another rectange of dst_rect
    bool BlitTextures(Surface& surface, Surface& dest, const VideoCore::TextureBlit& blit);

    /// Generates mipmaps for all the available levels of the texture
    void GenerateMipmaps(Surface& surface);

    /// Returns true if the provided pixel format needs convertion
    bool NeedsConversion(VideoCore::PixelFormat format) const;

    /// Removes any descriptor sets that contain the provided image view.
    void FreeDescriptorSetsWithImage(vk::ImageView image_view);

private:
    /// Clears a partial texture rect using a clear rectangle
    void ClearTextureWithRenderpass(Surface& surface, const VideoCore::TextureClear& clear);

private:
    const Instance& instance;
    Scheduler& scheduler;
    RenderManager& render_manager;
    DescriptorSetProvider& texture_provider;
    BlitHelper blit_helper;
    StreamBuffer upload_buffer;
    StreamBuffer download_buffer;
    u32 num_swapchain_images;
};

class Surface : public VideoCore::SurfaceBase {
    friend class TextureRuntime;

public:
    explicit Surface(TextureRuntime& runtime, const VideoCore::SurfaceParams& params);
    explicit Surface(TextureRuntime& runtime, const VideoCore::SurfaceBase& surface,
                     const VideoCore::Material* materal);
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    Surface(Surface&& o) noexcept = default;
    Surface& operator=(Surface&& o) noexcept = default;

    vk::ImageAspectFlags Aspect() const noexcept {
        return traits.aspect;
    }

    /// Returns the image at index, otherwise the base image
    vk::Image Image(u32 index = 1) const noexcept;

    /// Returns the image view at index, otherwise the base view
    vk::ImageView ImageView(u32 index = 1) const noexcept;

    /// Returns a copy of the upscaled image handle, used for feedback loops.
    vk::ImageView CopyImageView() noexcept;

    /// Returns the framebuffer view of the surface image
    vk::ImageView FramebufferView() noexcept;

    /// Returns the depth view of the surface image
    vk::ImageView DepthView() noexcept;

    /// Returns the stencil view of the surface image
    vk::ImageView StencilView() noexcept;

    /// Returns the R32 image view used for atomic load/store
    vk::ImageView StorageView() noexcept;

    /// Returns a framebuffer handle for rendering to this surface
    vk::Framebuffer Framebuffer() noexcept;

    /// Uploads pixel data in staging to a rectangle region of the surface texture
    void Upload(const VideoCore::BufferTextureCopy& upload, const VideoCore::StagingData& staging);

    /// Uploads the custom material to the surface allocation.
    void UploadCustom(const VideoCore::Material* material, u32 level);

    /// Downloads pixel data to staging from a rectangle region of the surface texture
    void Download(const VideoCore::BufferTextureCopy& download,
                  const VideoCore::StagingData& staging);

    /// Scales up the surface to match the new resolution scale.
    void ScaleUp(u32 new_scale);

    /// Returns the bpp of the internal surface format
    u32 GetInternalBytesPerPixel() const;

    /// Returns the access flags indicative of the surface
    vk::AccessFlags AccessFlags() const noexcept;

    /// Returns the pipeline stage flags indicative of the surface
    vk::PipelineStageFlags PipelineStageFlags() const noexcept;

private:
    /// Performs blit between the scaled/unscaled images
    void BlitScale(const VideoCore::TextureBlit& blit, bool up_scale);

    /// Downloads scaled depth stencil data
    void DepthStencilDownload(const VideoCore::BufferTextureCopy& download,
                              const VideoCore::StagingData& staging);

public:
    TextureRuntime* runtime;
    const Instance* instance;
    Scheduler* scheduler;
    FormatTraits traits;
    std::array<Handle, 3> handles{};
    std::array<vk::UniqueFramebuffer, 2> framebuffers{};
    Handle copy_handle;
    vk::UniqueImageView depth_view;
    vk::UniqueImageView stencil_view;
    vk::UniqueImageView storage_view;
    bool is_framebuffer{};
    bool is_storage{};
};

class Framebuffer : public VideoCore::FramebufferParams {
public:
    explicit Framebuffer(TextureRuntime& runtime, const VideoCore::FramebufferParams& params,
                         Surface* color, Surface* depth_stencil);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&& o) noexcept = default;
    Framebuffer& operator=(Framebuffer&& o) noexcept = default;

    VideoCore::PixelFormat Format(VideoCore::SurfaceType type) const noexcept {
        return formats[Index(type)];
    }

    [[nodiscard]] vk::ImageView ImageView(VideoCore::SurfaceType type) const noexcept {
        return image_views[Index(type)];
    }

    [[nodiscard]] vk::Framebuffer Handle() const noexcept {
        return framebuffer.get();
    }

    [[nodiscard]] std::array<vk::Image, 2> Images() const noexcept {
        return images;
    }

    [[nodiscard]] std::array<vk::ImageAspectFlags, 2> Aspects() const noexcept {
        return aspects;
    }

    [[nodiscard]] vk::RenderPass RenderPass() const noexcept {
        return render_pass;
    }

    u32 Scale() const noexcept {
        return res_scale;
    }

    u32 Width() const noexcept {
        return width;
    }

    u32 Height() const noexcept {
        return height;
    }

private:
    std::array<vk::Image, 2> images{};
    std::array<vk::ImageView, 2> image_views{};
    vk::UniqueFramebuffer framebuffer;
    vk::RenderPass render_pass;
    std::array<vk::ImageAspectFlags, 2> aspects{};
    std::array<VideoCore::PixelFormat, 2> formats{VideoCore::PixelFormat::Invalid,
                                                  VideoCore::PixelFormat::Invalid};
    u32 width{};
    u32 height{};
    u32 res_scale{1};
};

class Sampler {
public:
    Sampler(TextureRuntime& runtime, const VideoCore::SamplerParams& params);
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    Sampler(Sampler&& o) noexcept = default;
    Sampler& operator=(Sampler&& o) noexcept = default;

    [[nodiscard]] vk::Sampler Handle() const noexcept {
        return sampler.get();
    }

private:
    vk::UniqueSampler sampler;
};

class DebugScope {
public:
    template <typename... T>
    explicit DebugScope(TextureRuntime& runtime, Common::Vec4f color,
                        fmt::format_string<T...> format, T... args)
        : DebugScope{runtime, color, fmt::format(format, std::forward<T>(args)...)} {}
    explicit DebugScope(TextureRuntime& runtime, Common::Vec4f color, std::string_view label);
    ~DebugScope();

private:
    Scheduler& scheduler;
    bool has_debug_tool;
};

struct Traits {
    using Runtime = Vulkan::TextureRuntime;
    using Surface = Vulkan::Surface;
    using Sampler = Vulkan::Sampler;
    using Framebuffer = Vulkan::Framebuffer;
    using DebugScope = Vulkan::DebugScope;
};

using RasterizerCache = VideoCore::RasterizerCache<Traits>;

} // namespace Vulkan
