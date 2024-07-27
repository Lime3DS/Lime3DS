// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <limits>
#include <string_view>
#include "video_core/pica/regs_framebuffer.h"
#include "video_core/pica/regs_texturing.h"

namespace Pica {
enum class PixelFormat : u32;
}

namespace VideoCore {

enum class PixelFormat : u32 {
    RGBA8 = 0,
    RGB8 = 1,
    RGB5A1 = 2,
    RGB565 = 3,
    RGBA4 = 4,
    IA8 = 5,
    RG8 = 6,
    I8 = 7,
    A8 = 8,
    IA4 = 9,
    I4 = 10,
    A4 = 11,
    ETC1 = 12,
    ETC1A4 = 13,
    D16 = 14,
    D24 = 16,
    D24S8 = 17,
    MaxPixelFormat = 18,
    Invalid = std::numeric_limits<u32>::max(),
};
constexpr std::size_t PIXEL_FORMAT_COUNT = static_cast<std::size_t>(PixelFormat::MaxPixelFormat);

enum class SurfaceType : u32 {
    Color = 0,
    Texture = 1,
    Depth = 2,
    DepthStencil = 3,
    Fill = 4,
    Invalid = 5,
};

enum class TextureType : u16 {
    Texture2D = 0,
    CubeMap = 1,
};

struct PixelFormatInfo {
    SurfaceType type;
    u32 bits_per_block;
    u32 bytes_per_pixel;
};

/**
 * Lookup table for querying pixel format properties (type, name, etc)
 * @note Modern GPUs require 4 byte alignment for D24
 * @note Texture formats are automatically converted to RGBA8
 **/
constexpr std::array<PixelFormatInfo, PIXEL_FORMAT_COUNT> FORMAT_MAP = {{
    {SurfaceType::Color, 32, 4},
    {SurfaceType::Color, 24, 3},
    {SurfaceType::Color, 16, 2},
    {SurfaceType::Color, 16, 2},
    {SurfaceType::Color, 16, 2},
    {SurfaceType::Texture, 16, 4},
    {SurfaceType::Texture, 16, 4},
    {SurfaceType::Texture, 8, 4},
    {SurfaceType::Texture, 8, 4},
    {SurfaceType::Texture, 8, 4},
    {SurfaceType::Texture, 4, 4},
    {SurfaceType::Texture, 4, 4},
    {SurfaceType::Texture, 4, 4},
    {SurfaceType::Texture, 8, 4},
    {SurfaceType::Depth, 16, 2},
    {SurfaceType::Invalid, 0, 0},
    {SurfaceType::Depth, 24, 4},
    {SurfaceType::DepthStencil, 32, 4},
}};

constexpr u32 GetFormatBpp(PixelFormat format) {
    const std::size_t index = static_cast<std::size_t>(format);
    ASSERT(index < FORMAT_MAP.size());
    return FORMAT_MAP[index].bits_per_block;
}

constexpr u32 GetFormatBytesPerPixel(PixelFormat format) {
    const std::size_t index = static_cast<std::size_t>(format);
    ASSERT(index < FORMAT_MAP.size());
    return FORMAT_MAP[index].bytes_per_pixel;
}

constexpr SurfaceType GetFormatType(PixelFormat format) {
    const std::size_t index = static_cast<std::size_t>(format);
    ASSERT(index < FORMAT_MAP.size());
    return FORMAT_MAP[index].type;
}

bool CheckFormatsBlittable(PixelFormat source_format, PixelFormat dest_format);

std::string_view PixelFormatAsString(PixelFormat format);

PixelFormat PixelFormatFromTextureFormat(Pica::TexturingRegs::TextureFormat format);

PixelFormat PixelFormatFromColorFormat(Pica::FramebufferRegs::ColorFormat format);

PixelFormat PixelFormatFromDepthFormat(Pica::FramebufferRegs::DepthFormat format);

PixelFormat PixelFormatFromGPUPixelFormat(Pica::PixelFormat format);

} // namespace VideoCore
