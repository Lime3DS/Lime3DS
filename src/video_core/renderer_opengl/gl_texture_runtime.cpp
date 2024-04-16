// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/scope_exit.h"
#include "common/settings.h"
#include "video_core/custom_textures/material.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_driver.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_texture_mailbox.h"
#include "video_core/renderer_opengl/gl_texture_runtime.h"
#include "video_core/renderer_opengl/pica_to_gl.h"

namespace OpenGL {

namespace {

using VideoCore::MapType;
using VideoCore::PixelFormat;
using VideoCore::SurfaceFlagBits;
using VideoCore::SurfaceType;
using VideoCore::TextureType;

constexpr GLenum TEMP_UNIT = GL_TEXTURE15;

constexpr FormatTuple DEFAULT_TUPLE = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};

static constexpr std::array<FormatTuple, 4> DEPTH_TUPLES = {{
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT}, // D16
    {},
    {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},   // D24
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8}, // D24S8
}};

static constexpr std::array<FormatTuple, 5> COLOR_TUPLES = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},     // RGBA8
    {GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE},              // RGB8
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1}, // RGB5A1
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},     // RGB565
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},   // RGBA4
}};

static constexpr std::array<FormatTuple, 5> COLOR_TUPLES_OES = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},            // RGBA8
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},            // RGB8
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1}, // RGB5A1
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},     // RGB565
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},   // RGBA4
}};

static constexpr std::array<FormatTuple, 8> CUSTOM_TUPLES = {{
    DEFAULT_TUPLE,
    {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RG_RGTC2, GL_COMPRESSED_RG_RGTC2, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_COMPRESSED_RGBA_BPTC_UNORM_ARB, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_ASTC_4x4, GL_COMPRESSED_RGBA_ASTC_4x4, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_ASTC_6x6, GL_COMPRESSED_RGBA_ASTC_6x6, GL_UNSIGNED_BYTE},
    {GL_COMPRESSED_RGBA_ASTC_8x6, GL_COMPRESSED_RGBA_ASTC_8x6, GL_UNSIGNED_BYTE},
}};

[[nodiscard]] GLbitfield MakeBufferMask(SurfaceType type) {
    switch (type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
    case SurfaceType::Fill:
        return GL_COLOR_BUFFER_BIT;
    case SurfaceType::Depth:
        return GL_DEPTH_BUFFER_BIT;
    case SurfaceType::DepthStencil:
        return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    default:
        UNREACHABLE_MSG("Invalid surface type!");
    }
    return GL_COLOR_BUFFER_BIT;
}

[[nodiscard]] u32 FboIndex(VideoCore::SurfaceType type) {
    switch (type) {
    case VideoCore::SurfaceType::Color:
    case VideoCore::SurfaceType::Texture:
    case VideoCore::SurfaceType::Fill:
        return 0;
    case VideoCore::SurfaceType::Depth:
        return 1;
    case VideoCore::SurfaceType::DepthStencil:
        return 2;
    default:
        UNREACHABLE_MSG("Invalid surface type!");
    }
    return 0;
}

[[nodiscard]] OGLTexture MakeHandle(GLenum target, u32 width, u32 height, u32 levels,
                                    const FormatTuple& tuple, std::string_view debug_name = "") {
    OGLTexture texture{};
    texture.Create();

    glBindTexture(target, texture.handle);
    glTexStorage2D(target, levels, tuple.internal_format, width, height);

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!debug_name.empty()) {
        glObjectLabel(GL_TEXTURE, texture.handle, -1, debug_name.data());
    }

    return texture;
}

} // Anonymous namespace

TextureRuntime::TextureRuntime(const Driver& driver_, VideoCore::RendererBase& renderer)
    : driver{driver_}, blit_helper{driver} {
    for (std::size_t i = 0; i < draw_fbos.size(); ++i) {
        draw_fbos[i].Create();
        read_fbos[i].Create();
    }
}

TextureRuntime::~TextureRuntime() = default;

u32 TextureRuntime::RemoveThreshold() {
    return SWAP_CHAIN_SIZE;
}

bool TextureRuntime::NeedsConversion(VideoCore::PixelFormat pixel_format) const {
    const bool should_convert = pixel_format == PixelFormat::RGBA8 || // Needs byteswap
                                pixel_format == PixelFormat::RGB8;    // Is converted to RGBA8
    return driver.IsOpenGLES() && should_convert;
}

VideoCore::StagingData TextureRuntime::FindStaging(u32 size, bool upload) {
    if (size > staging_buffer.size()) {
        staging_buffer.resize(size);
    }
    return VideoCore::StagingData{
        .size = size,
        .offset = 0,
        .mapped = std::span{staging_buffer.data(), size},
    };
}

const FormatTuple& TextureRuntime::GetFormatTuple(PixelFormat pixel_format) const {
    if (pixel_format == PixelFormat::Invalid) {
        return DEFAULT_TUPLE;
    }

    const auto type = GetFormatType(pixel_format);
    const std::size_t format_index = static_cast<std::size_t>(pixel_format);

    if (type == SurfaceType::Color) {
        ASSERT(format_index < COLOR_TUPLES.size());
        return (driver.IsOpenGLES() ? COLOR_TUPLES_OES : COLOR_TUPLES)[format_index];
    } else if (type == SurfaceType::Depth || type == SurfaceType::DepthStencil) {
        const std::size_t tuple_idx = format_index - 14;
        ASSERT(tuple_idx < DEPTH_TUPLES.size());
        return DEPTH_TUPLES[tuple_idx];
    }

    return DEFAULT_TUPLE;
}

const FormatTuple& TextureRuntime::GetFormatTuple(VideoCore::CustomPixelFormat pixel_format) {
    const std::size_t format_index = static_cast<std::size_t>(pixel_format);
    return CUSTOM_TUPLES[format_index];
}

bool TextureRuntime::Reinterpret(Surface& source, Surface& dest,
                                 const VideoCore::TextureCopy& copy) {
    const PixelFormat src_format = source.pixel_format;
    const PixelFormat dst_format = dest.pixel_format;
    ASSERT_MSG(src_format != dst_format, "Reinterpretation with the same format is invalid");
    if (src_format == PixelFormat::D24S8 && dst_format == PixelFormat::RGBA8) {
        blit_helper.ConvertDS24S8ToRGBA8(source, dest, copy);
    } else if (src_format == PixelFormat::RGBA4 && dst_format == PixelFormat::RGB5A1) {
        blit_helper.ConvertRGBA4ToRGB5A1(source, dest, copy);
    } else {
        LOG_WARNING(Render_OpenGL, "Unimplemented reinterpretation {} -> {}",
                    VideoCore::PixelFormatAsString(src_format),
                    VideoCore::PixelFormatAsString(dst_format));
        return false;
    }
    return true;
}

bool TextureRuntime::ClearTextureWithoutFbo(Surface& surface,
                                            const VideoCore::TextureClear& clear) {
    if (!driver.HasArbClearTexture() || driver.HasBug(DriverBug::BrokenClearTexture)) {
        return false;
    }
    GLenum format{};
    GLenum type{};
    switch (surface.type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
        format = GL_RGBA;
        type = GL_FLOAT;
        break;
    case SurfaceType::Depth:
        format = GL_DEPTH_COMPONENT;
        type = GL_FLOAT;
        break;
    case SurfaceType::DepthStencil:
        format = GL_DEPTH_STENCIL;
        type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        break;
    default:
        UNREACHABLE_MSG("Unknown surface type {}", surface.type);
    }
    glClearTexSubImage(surface.Handle(), clear.texture_level, clear.texture_rect.left,
                       clear.texture_rect.bottom, 0, clear.texture_rect.GetWidth(),
                       clear.texture_rect.GetHeight(), 1, format, type, &clear.value);
    return true;
}

void TextureRuntime::ClearTexture(Surface& surface, const VideoCore::TextureClear& clear) {
    if (ClearTextureWithoutFbo(surface, clear)) {
        return;
    }

    OpenGLState state = OpenGLState::GetCurState();
    state.scissor.enabled = true;
    state.scissor.x = clear.texture_rect.left;
    state.scissor.y = clear.texture_rect.bottom;
    state.scissor.width = clear.texture_rect.GetWidth();
    state.scissor.height = clear.texture_rect.GetHeight();
    state.draw.draw_framebuffer = draw_fbos[FboIndex(surface.type)].handle;
    state.Apply();

    surface.Attach(GL_DRAW_FRAMEBUFFER, clear.texture_level, 0);

    switch (surface.type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
        state.color_mask.red_enabled = true;
        state.color_mask.green_enabled = true;
        state.color_mask.blue_enabled = true;
        state.color_mask.alpha_enabled = true;
        state.Apply();
        glClearBufferfv(GL_COLOR, 0, clear.value.color.AsArray());
        break;
    case SurfaceType::Depth:
        state.depth.write_mask = GL_TRUE;
        state.Apply();
        glClearBufferfv(GL_DEPTH, 0, &clear.value.depth);
        break;
    case SurfaceType::DepthStencil:
        state.depth.write_mask = GL_TRUE;
        state.stencil.write_mask = -1;
        state.Apply();
        glClearBufferfi(GL_DEPTH_STENCIL, 0, clear.value.depth, clear.value.stencil);
        break;
    default:
        UNREACHABLE_MSG("Unknown surface type {}", surface.type);
    }
}

bool TextureRuntime::CopyTextures(Surface& source, Surface& dest,
                                  std::span<const VideoCore::TextureCopy> copies) {
    const GLenum src_textarget = source.texture_type == VideoCore::TextureType::CubeMap
                                     ? GL_TEXTURE_CUBE_MAP
                                     : GL_TEXTURE_2D;
    const GLenum dest_textarget =
        dest.texture_type == VideoCore::TextureType::CubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    for (const auto& copy : copies) {
        glCopyImageSubData(source.Handle(), src_textarget, copy.src_level, copy.src_offset.x,
                           copy.src_offset.y, copy.src_layer, dest.Handle(), dest_textarget,
                           copy.dst_level, copy.dst_offset.x, copy.dst_offset.y, copy.dst_layer,
                           copy.extent.width, copy.extent.height, 1);
    }
    return true;
}

bool TextureRuntime::BlitTextures(Surface& source, Surface& dest,
                                  const VideoCore::TextureBlit& blit) {
    OpenGLState state = OpenGLState::GetCurState();
    state.scissor.enabled = false;
    state.draw.read_framebuffer = read_fbos[FboIndex(source.type)].handle;
    state.draw.draw_framebuffer = draw_fbos[FboIndex(dest.type)].handle;
    state.Apply();

    source.Attach(GL_READ_FRAMEBUFFER, blit.src_level, blit.src_layer);
    dest.Attach(GL_DRAW_FRAMEBUFFER, blit.dst_level, blit.dst_layer);

    // Note: shadow map is treated as RGBA8 format in PICA, as well as in the rasterizer cache, but
    // doing linear intepolation componentwise would cause incorrect value.
    const GLbitfield buffer_mask = MakeBufferMask(source.type);
    const bool is_shadow_map = True(source.flags & SurfaceFlagBits::ShadowMap);
    const GLenum filter =
        buffer_mask == GL_COLOR_BUFFER_BIT && !is_shadow_map ? GL_LINEAR : GL_NEAREST;
    glBlitFramebuffer(blit.src_rect.left, blit.src_rect.bottom, blit.src_rect.right,
                      blit.src_rect.top, blit.dst_rect.left, blit.dst_rect.bottom,
                      blit.dst_rect.right, blit.dst_rect.top, buffer_mask, filter);

    return true;
}

void TextureRuntime::GenerateMipmaps(Surface& surface) {
    OpenGLState state = OpenGLState::GetCurState();

    const auto generate = [&](u32 index) {
        state.texture_units[0].texture_2d = surface.Handle(index);
        state.Apply();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, surface.levels - 1);
        glGenerateMipmap(GL_TEXTURE_2D);
    };

    generate(1);
    if (surface.HasNormalMap()) {
        generate(2);
    }
}

Surface::Surface(TextureRuntime& runtime_, const VideoCore::SurfaceParams& params)
    : SurfaceBase{params}, driver{&runtime_.GetDriver()}, runtime{&runtime_},
      tuple{runtime->GetFormatTuple(pixel_format)} {
    if (pixel_format == PixelFormat::Invalid) {
        return;
    }

    glActiveTexture(TEMP_UNIT);
    const GLenum target =
        texture_type == VideoCore::TextureType::CubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    textures[0] = MakeHandle(target, width, height, levels, tuple, DebugName(false));
    if (res_scale != 1) {
        textures[1] = MakeHandle(target, GetScaledWidth(), GetScaledHeight(), levels, tuple,
                                 DebugName(true, false));
    }
}

Surface::Surface(TextureRuntime& runtime, const VideoCore::SurfaceBase& surface,
                 const VideoCore::Material* mat)
    : SurfaceBase{surface}, tuple{runtime.GetFormatTuple(mat->format)} {
    if (mat && !driver->IsCustomFormatSupported(mat->format)) {
        return;
    }

    glActiveTexture(TEMP_UNIT);
    const GLenum target =
        texture_type == VideoCore::TextureType::CubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    custom_format = mat->format;
    material = mat;

    textures[0] = MakeHandle(target, mat->width, mat->height, levels, tuple, DebugName(false));
    if (res_scale != 1) {
        textures[1] = MakeHandle(target, mat->width, mat->height, levels, DEFAULT_TUPLE,
                                 DebugName(true, true));
    }
    const bool has_normal = mat->Map(MapType::Normal);
    if (has_normal) {
        textures[2] =
            MakeHandle(target, mat->width, mat->height, levels, tuple, DebugName(true, true));
    }
}

Surface::~Surface() = default;

GLuint Surface::Handle(u32 index) const noexcept {
    if (!textures[index].handle) {
        return textures[0].handle;
    }
    return textures[index].handle;
}

GLuint Surface::CopyHandle() noexcept {
    if (!copy_texture.handle) {
        copy_texture = MakeHandle(GL_TEXTURE_2D, GetScaledWidth(), GetScaledHeight(), levels, tuple,
                                  DebugName(true));
    }

    for (u32 level = 0; level < levels; level++) {
        const u32 width = GetScaledWidth() >> level;
        const u32 height = GetScaledHeight() >> level;
        glCopyImageSubData(Handle(1), GL_TEXTURE_2D, level, 0, 0, 0, copy_texture.handle,
                           GL_TEXTURE_2D, level, 0, 0, 0, width, height, 1);
    }

    return copy_texture.handle;
}

void Surface::Upload(const VideoCore::BufferTextureCopy& upload,
                     const VideoCore::StagingData& staging) {
    ASSERT(stride * GetFormatBytesPerPixel(pixel_format) % 4 == 0);

    const u32 unscaled_width = upload.texture_rect.GetWidth();
    const u32 unscaled_height = upload.texture_rect.GetHeight();
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unscaled_width);

    glActiveTexture(TEMP_UNIT);
    glBindTexture(GL_TEXTURE_2D, Handle(0));

    glTexSubImage2D(GL_TEXTURE_2D, upload.texture_level, upload.texture_rect.left,
                    upload.texture_rect.bottom, unscaled_width, unscaled_height, tuple.format,
                    tuple.type, staging.mapped.data());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    const VideoCore::TextureBlit blit = {
        .src_level = upload.texture_level,
        .dst_level = upload.texture_level,
        .src_rect = upload.texture_rect,
        .dst_rect = upload.texture_rect * res_scale,
    };
    if (res_scale != 1 && !runtime->blit_helper.Filter(*this, blit)) {
        BlitScale(blit, true);
    }
}

void Surface::UploadCustom(const VideoCore::Material* material, u32 level) {
    const u32 width = material->width;
    const u32 height = material->height;
    const auto color = material->textures[0];
    const Common::Rectangle filter_rect{0U, height, width, 0U};

    glActiveTexture(TEMP_UNIT);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, width);

    const auto upload = [&](u32 index, VideoCore::CustomTexture* texture) {
        glBindTexture(GL_TEXTURE_2D, Handle(index));
        if (VideoCore::IsCustomFormatCompressed(custom_format)) {
            const GLsizei image_size = static_cast<GLsizei>(texture->data.size());
            glCompressedTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, tuple.format,
                                      image_size, texture->data.data());
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, level, 0, 0, width, height, tuple.format, tuple.type,
                            texture->data.data());
        }
    };

    upload(0, color);

    const VideoCore::TextureBlit blit = {
        .src_rect = filter_rect,
        .dst_rect = filter_rect,
    };
    if (res_scale != 1 && !runtime->blit_helper.Filter(*this, blit)) {
        BlitScale(blit, true);
    }
    for (u32 i = 1; i < VideoCore::MAX_MAPS; i++) {
        const auto texture = material->textures[i];
        if (!texture) {
            continue;
        }
        upload(i + 1, texture);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void Surface::Download(const VideoCore::BufferTextureCopy& download,
                       const VideoCore::StagingData& staging) {
    ASSERT(stride * GetFormatBytesPerPixel(pixel_format) % 4 == 0);

    const u32 unscaled_width = download.texture_rect.GetWidth();
    const u32 unscaled_height = download.texture_rect.GetHeight();
    glPixelStorei(GL_UNPACK_ROW_LENGTH, unscaled_width);

    // Scale down upscaled data before downloading it
    if (res_scale != 1) {
        const VideoCore::TextureBlit blit = {
            .src_level = download.texture_level,
            .dst_level = download.texture_level,
            .src_rect = download.texture_rect * res_scale,
            .dst_rect = download.texture_rect,
        };
        BlitScale(blit, false);
    }

    // Try to download without using an fbo. This should succeed on recent desktop drivers
    if (DownloadWithoutFbo(download, staging)) {
        return;
    }

    OpenGLState state = OpenGLState::GetCurState();
    state.scissor.enabled = false;
    state.draw.read_framebuffer = runtime->read_fbos[FboIndex(type)].handle;
    state.Apply();

    Attach(GL_READ_FRAMEBUFFER, download.texture_level, 0, false);

    // Read the pixel data to the staging buffer
    const auto& tuple = runtime->GetFormatTuple(pixel_format);
    glReadPixels(download.texture_rect.left, download.texture_rect.bottom, unscaled_width,
                 unscaled_height, tuple.format, tuple.type, staging.mapped.data());

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

bool Surface::DownloadWithoutFbo(const VideoCore::BufferTextureCopy& download,
                                 const VideoCore::StagingData& staging) {
    if (driver->IsOpenGLES()) {
        return false;
    }

    const auto& tuple = runtime->GetFormatTuple(pixel_format);
    const u32 unscaled_width = download.texture_rect.GetWidth();

    glActiveTexture(TEMP_UNIT);
    glPixelStorei(GL_PACK_ROW_LENGTH, unscaled_width);
    SCOPE_EXIT({ glPixelStorei(GL_PACK_ROW_LENGTH, 0); });

    // Prefer glGetTextureSubImage in most cases since it's the fastest and most convenient option
    const bool is_full_download = download.texture_rect == GetRect();
    const bool has_sub_image = driver->HasArbGetTextureSubImage();
    if (has_sub_image) {
        const GLsizei buf_size = static_cast<GLsizei>(staging.mapped.size());
        glGetTextureSubImage(Handle(0), download.texture_level, download.texture_rect.left,
                             download.texture_rect.bottom, 0, download.texture_rect.GetWidth(),
                             download.texture_rect.GetHeight(), 1, tuple.format, tuple.type,
                             buf_size, staging.mapped.data());
        return true;
    } else if (is_full_download) {
        // This should only trigger for full texture downloads in oldish intel drivers
        // that only support up to 4.3
        OpenGLState state = OpenGLState::GetCurState();
        state.texture_units[0].texture_2d = Handle(0);
        state.Apply();

        glGetTexImage(GL_TEXTURE_2D, download.texture_level, tuple.format, tuple.type,
                      staging.mapped.data());

        return true;
    }
    return false;
}

void Surface::Attach(GLenum target, u32 level, u32 layer, bool scaled) {
    const GLuint handle = Handle(static_cast<u32>(scaled));
    const GLenum textarget = texture_type == TextureType::CubeMap
                                 ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer
                                 : GL_TEXTURE_2D;

    switch (type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
        glFramebufferTexture2D(target, GL_COLOR_ATTACHMENT0, textarget, handle, level);
        break;
    case SurfaceType::Depth:
        glFramebufferTexture2D(target, GL_DEPTH_ATTACHMENT, textarget, handle, level);
        break;
    case SurfaceType::DepthStencil:
        glFramebufferTexture2D(target, GL_DEPTH_STENCIL_ATTACHMENT, textarget, handle, level);
        break;
    default:
        UNREACHABLE_MSG("Invalid surface type!");
    }
}

void Surface::ScaleUp(u32 new_scale) {
    if (res_scale == new_scale || new_scale == 1) {
        return;
    }

    res_scale = new_scale;
    textures[1] = MakeHandle(GL_TEXTURE_2D, GetScaledWidth(), GetScaledHeight(), levels, tuple,
                             DebugName(true));

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
    // RGB8 is converted to RGBA8 on OpenGL ES since it doesn't support BGR8
    if (driver->IsOpenGLES() && pixel_format == VideoCore::PixelFormat::RGB8) {
        return 4;
    }
    return GetFormatBytesPerPixel(pixel_format);
}

void Surface::BlitScale(const VideoCore::TextureBlit& blit, bool up_scale) {
    const u32 fbo_index = FboIndex(type);

    OpenGLState state = OpenGLState::GetCurState();
    state.scissor.enabled = false;
    state.draw.read_framebuffer = runtime->read_fbos[fbo_index].handle;
    state.draw.draw_framebuffer = runtime->draw_fbos[fbo_index].handle;
    state.Apply();

    Attach(GL_READ_FRAMEBUFFER, blit.src_level, blit.src_layer, !up_scale);
    Attach(GL_DRAW_FRAMEBUFFER, blit.dst_level, blit.dst_layer, up_scale);

    const GLenum buffer_mask = MakeBufferMask(type);
    const GLenum filter = buffer_mask == GL_COLOR_BUFFER_BIT ? GL_LINEAR : GL_NEAREST;
    glBlitFramebuffer(blit.src_rect.left, blit.src_rect.bottom, blit.src_rect.right,
                      blit.src_rect.top, blit.dst_rect.left, blit.dst_rect.bottom,
                      blit.dst_rect.right, blit.dst_rect.top, buffer_mask, filter);
}

Framebuffer::Framebuffer(TextureRuntime& runtime, const VideoCore::FramebufferParams& params,
                         const Surface* color, const Surface* depth)
    : VideoCore::FramebufferParams{params},
      res_scale{color ? color->res_scale : (depth ? depth->res_scale : 1u)} {

    if (shadow_rendering && !color) {
        return;
    }

    if (color) {
        attachments[0] = color->Handle();
    }
    if (depth) {
        attachments[1] = depth->Handle();
    }

    framebuffer.Create();

    OpenGLState state = OpenGLState::GetCurState();
    state.draw.draw_framebuffer = framebuffer.handle;
    state.Apply();

    if (shadow_rendering) {
        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH,
                                color->width * res_scale);
        glFramebufferParameteri(GL_DRAW_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_HEIGHT,
                                color->height * res_scale);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);
    } else {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               color ? color->Handle() : 0, color_level);
        if (depth) {
            if (depth->pixel_format == PixelFormat::D24S8) {
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                       GL_TEXTURE_2D, depth->Handle(), depth_level);
            } else {
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                       depth->Handle(), depth_level);
                glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                                       0);
            }
        } else {
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   0, 0);
        }
    }
}

Framebuffer::~Framebuffer() = default;

Sampler::Sampler(TextureRuntime&, VideoCore::SamplerParams params) {
    const GLenum mag_filter = PicaToGL::TextureMagFilterMode(params.mag_filter);
    const GLenum min_filter = PicaToGL::TextureMinFilterMode(params.min_filter, params.mip_filter);
    const GLenum wrap_s = PicaToGL::WrapMode(params.wrap_s);
    const GLenum wrap_t = PicaToGL::WrapMode(params.wrap_t);
    const Common::Vec4f gl_color = PicaToGL::ColorRGBA8(params.border_color);
    const auto lod_min = static_cast<float>(params.lod_min);
    const auto lod_max = static_cast<float>(params.lod_max);

    sampler.Create();

    const GLuint handle = sampler.handle;
    glSamplerParameteri(handle, GL_TEXTURE_MAG_FILTER, mag_filter);
    glSamplerParameteri(handle, GL_TEXTURE_MIN_FILTER, min_filter);

    glSamplerParameteri(handle, GL_TEXTURE_WRAP_S, wrap_s);
    glSamplerParameteri(handle, GL_TEXTURE_WRAP_T, wrap_t);

    glSamplerParameterfv(handle, GL_TEXTURE_BORDER_COLOR, gl_color.AsArray());

    glSamplerParameterf(handle, GL_TEXTURE_MIN_LOD, lod_min);
    glSamplerParameterf(handle, GL_TEXTURE_MAX_LOD, lod_max);
}

Sampler::~Sampler() = default;

DebugScope::DebugScope(TextureRuntime& runtime, Common::Vec4f, std::string_view label)
    : local_scope_depth{global_scope_depth++} {
    if (!Settings::values.renderer_debug) {
        return;
    }
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, local_scope_depth,
                     static_cast<GLsizei>(label.size()), label.data());
}

DebugScope::~DebugScope() {
    if (!Settings::values.renderer_debug) {
        return;
    }
    glPopDebugGroup();
    global_scope_depth--;
}

} // namespace OpenGL
