// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/rasterizer_cache/framebuffer_base.h"
#include "video_core/rasterizer_cache/rasterizer_cache_base.h"
#include "video_core/rasterizer_cache/surface_base.h"
#include "video_core/renderer_opengl/gl_blit_helper.h"

namespace VideoCore {
struct Material;
class RendererBase;
} // namespace VideoCore

namespace OpenGL {

struct FormatTuple {
    GLint internal_format;
    GLenum format;
    GLenum type;

    bool operator==(const FormatTuple& other) const noexcept {
        return std::tie(internal_format, format, type) ==
               std::tie(other.internal_format, other.format, other.type);
    }
};

class Surface;
class Driver;

/**
 * Provides texture manipulation functions to the rasterizer cache
 * Separating this into a class makes it easier to abstract graphics API code
 */
class TextureRuntime {
    friend class Surface;
    friend class Framebuffer;

public:
    explicit TextureRuntime(const Driver& driver, VideoCore::RendererBase& renderer);
    ~TextureRuntime();

    /// Returns the removal threshold ticks for the garbage collector
    u32 RemoveThreshold();

    /// Submits and waits for current GPU work.
    void Finish() {}

    /// Returns true if the provided pixel format cannot be used natively by the runtime.
    bool NeedsConversion(VideoCore::PixelFormat pixel_format) const;

    /// Maps an internal staging buffer of the provided size of pixel uploads/downloads
    VideoCore::StagingData FindStaging(u32 size, bool upload);

    /// Returns the OpenGL format tuple associated with the provided pixel format
    const FormatTuple& GetFormatTuple(VideoCore::PixelFormat pixel_format) const;
    const FormatTuple& GetFormatTuple(VideoCore::CustomPixelFormat pixel_format);

    /// Attempts to reinterpret a rectangle of source to another rectangle of dest
    bool Reinterpret(Surface& source, Surface& dest, const VideoCore::TextureCopy& copy);

    /// Fills the rectangle of the texture with the clear value provided
    void ClearTexture(Surface& surface, const VideoCore::TextureClear& clear);

    /// Copies a rectangle of source to another rectange of dest
    bool CopyTextures(Surface& source, Surface& dest,
                      std::span<const VideoCore::TextureCopy> copies);

    bool CopyTextures(Surface& source, Surface& dest, const VideoCore::TextureCopy& copy) {
        return CopyTextures(source, dest, std::array{copy});
    }

    /// Blits a rectangle of source to another rectange of dest
    bool BlitTextures(Surface& source, Surface& dest, const VideoCore::TextureBlit& blit);

    /// Generates mipmaps for all the available levels of the texture
    void GenerateMipmaps(Surface& surface);

private:
    /// Returns the OpenGL driver class
    const Driver& GetDriver() const {
        return driver;
    }

    /// Fills the rectangle of the surface with the value provided, without an fbo.
    bool ClearTextureWithoutFbo(Surface& surface, const VideoCore::TextureClear& clear);

private:
    const Driver& driver;
    BlitHelper blit_helper;
    std::vector<u8> staging_buffer;
    std::array<OGLFramebuffer, 3> draw_fbos;
    std::array<OGLFramebuffer, 3> read_fbos;
};

class Surface : public VideoCore::SurfaceBase {
public:
    explicit Surface(TextureRuntime& runtime, const VideoCore::SurfaceParams& params);
    explicit Surface(TextureRuntime& runtime, const VideoCore::SurfaceBase& surface,
                     const VideoCore::Material* material);
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    Surface(Surface&& o) noexcept = default;
    Surface& operator=(Surface&& o) noexcept = default;

    [[nodiscard]] const FormatTuple& Tuple() const noexcept {
        return tuple;
    }

    /// Returns the texture handle at index, otherwise the first one if not valid.
    GLuint Handle(u32 index = 1) const noexcept;

    /// Returns a copy of the upscaled texture handle, used for feedback loops.
    GLuint CopyHandle() noexcept;

    /// Uploads pixel data in staging to a rectangle region of the surface texture
    void Upload(const VideoCore::BufferTextureCopy& upload, const VideoCore::StagingData& staging);

    /// Uploads the custom material to the surface allocation.
    void UploadCustom(const VideoCore::Material* material, u32 level);

    /// Downloads pixel data to staging from a rectangle region of the surface texture
    void Download(const VideoCore::BufferTextureCopy& download,
                  const VideoCore::StagingData& staging);

    /// Attaches a handle of surface to the specified framebuffer target
    void Attach(GLenum target, u32 level, u32 layer, bool scaled = true);

    /// Scales up the surface to match the new resolution scale.
    void ScaleUp(u32 new_scale);

    /// Returns the bpp of the internal surface format
    u32 GetInternalBytesPerPixel() const;

private:
    /// Performs blit between the scaled/unscaled images
    void BlitScale(const VideoCore::TextureBlit& blit, bool up_scale);

    /// Attempts to download without using an fbo
    bool DownloadWithoutFbo(const VideoCore::BufferTextureCopy& download,
                            const VideoCore::StagingData& staging);

private:
    const Driver* driver;
    TextureRuntime* runtime;
    std::array<OGLTexture, 3> textures;
    OGLTexture copy_texture;
    FormatTuple tuple;
};

class Framebuffer : public VideoCore::FramebufferParams {
public:
    explicit Framebuffer(TextureRuntime& runtime, const VideoCore::FramebufferParams& params,
                         const Surface* color, const Surface* depth_stencil);
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    Framebuffer(Framebuffer&& o) noexcept = default;
    Framebuffer& operator=(Framebuffer&& o) noexcept = default;

    [[nodiscard]] u32 Scale() const noexcept {
        return res_scale;
    }

    [[nodiscard]] GLuint Handle() const noexcept {
        return framebuffer.handle;
    }

    [[nodiscard]] GLuint Attachment(VideoCore::SurfaceType type) const noexcept {
        return attachments[Index(type)];
    }

    [[nodiscard]] bool HasAttachment(VideoCore::SurfaceType type) const noexcept {
        return static_cast<bool>(attachments[Index(type)]);
    }

private:
    u32 res_scale{1};
    std::array<GLuint, 2> attachments{};
    OGLFramebuffer framebuffer;
};

class Sampler {
public:
    explicit Sampler(TextureRuntime&, VideoCore::SamplerParams params);
    ~Sampler();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    Sampler(Sampler&&) = default;
    Sampler& operator=(Sampler&&) = default;

    [[nodiscard]] GLuint Handle() const noexcept {
        return sampler.handle;
    }

private:
    OGLSampler sampler;
};

class DebugScope {
public:
    template <typename... T>
    explicit DebugScope(TextureRuntime& runtime, Common::Vec4f color,
                        fmt::format_string<T...> format, T... args)
        : DebugScope{runtime, color, fmt::format(format, std::forward<T>(args)...)} {}
    explicit DebugScope(TextureRuntime& runtime, Common::Vec4f, std::string_view label);
    ~DebugScope();

private:
    inline static GLuint global_scope_depth = 0;
    const GLuint local_scope_depth{};
};

struct Traits {
    using Runtime = OpenGL::TextureRuntime;
    using Sampler = OpenGL::Sampler;
    using Surface = OpenGL::Surface;
    using Framebuffer = OpenGL::Framebuffer;
    using DebugScope = OpenGL::DebugScope;
};

using RasterizerCache = VideoCore::RasterizerCache<Traits>;

} // namespace OpenGL
