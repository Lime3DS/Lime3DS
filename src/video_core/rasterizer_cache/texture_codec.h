// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <bit>
#include <span>
#include "common/alignment.h"
#include "common/color.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/texture/etc1.h"
#include "video_core/utils.h"

namespace VideoCore {

template <typename T>
inline T MakeInt(const u8* bytes) {
    T integer{};
    std::memcpy(&integer, bytes, sizeof(T));

    return integer;
}

template <PixelFormat format, bool converted>
constexpr void DecodePixel(const u8* source, u8* dest) {
    using namespace Common::Color;
    constexpr u32 bytes_per_pixel = GetFormatBpp(format) / 8;

    if constexpr (format == PixelFormat::RGBA8 && converted) {
        const auto abgr = DecodeRGBA8(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::RGB8 && converted) {
        const auto abgr = DecodeRGB8(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::RGB565 && converted) {
        const auto abgr = DecodeRGB565(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::RGB5A1 && converted) {
        const auto abgr = DecodeRGB5A1(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::RGBA4 && converted) {
        const auto abgr = DecodeRGBA4(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::IA8) {
        const auto abgr = DecodeIA8(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::RG8) {
        const auto abgr = DecodeRG8(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::I8) {
        const auto abgr = DecodeI8(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::A8) {
        const auto abgr = DecodeA8(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::IA4) {
        const auto abgr = DecodeIA4(source);
        std::memcpy(dest, abgr.AsArray(), 4);
    } else if constexpr (format == PixelFormat::D24 && converted) {
        const auto d32 = DecodeD24(source) / 16777215.f;
        std::memcpy(dest, &d32, sizeof(d32));
    } else if constexpr (format == PixelFormat::D24S8) {
        const u32 d24s8 = std::rotl(MakeInt<u32>(source), 8);
        std::memcpy(dest, &d24s8, sizeof(u32));
    } else {
        std::memcpy(dest, source, bytes_per_pixel);
    }
}

template <PixelFormat format>
constexpr void DecodePixel4(u32 x, u32 y, const u8* source_tile, u8* dest_pixel) {
    const u32 morton_offset = VideoCore::MortonInterleave(x, y);
    const u8 value = source_tile[morton_offset >> 1];
    const u8 pixel = Common::Color::Convert4To8((morton_offset % 2) ? (value >> 4) : (value & 0xF));

    if constexpr (format == PixelFormat::I4) {
        std::memset(dest_pixel, pixel, 3);
        dest_pixel[3] = 255;
    } else {
        std::memset(dest_pixel, 0, 3);
        dest_pixel[3] = pixel;
    }
}

template <PixelFormat format>
constexpr void DecodePixelETC1(u32 x, u32 y, const u8* source_tile, u8* dest_pixel) {
    constexpr u32 subtile_width = 4;
    constexpr u32 subtile_height = 4;
    constexpr bool has_alpha = format == PixelFormat::ETC1A4;
    constexpr std::size_t subtile_size = has_alpha ? 16 : 8;

    const u32 subtile_index = (x / subtile_width) + 2 * (y / subtile_height);
    x %= subtile_width;
    y %= subtile_height;

    const u8* subtile_ptr = source_tile + subtile_index * subtile_size;

    u8 alpha = 255;
    if constexpr (has_alpha) {
        u64_le packed_alpha;
        std::memcpy(&packed_alpha, subtile_ptr, sizeof(u64));
        subtile_ptr += sizeof(u64);

        alpha = Common::Color::Convert4To8((packed_alpha >> (4 * (x * subtile_width + y))) & 0xF);
    }

    const u64_le subtile_data = MakeInt<u64_le>(subtile_ptr);
    const auto rgb = Pica::Texture::SampleETC1Subtile(subtile_data, x, y);

    // Copy the uncompressed pixel to the destination
    std::memcpy(dest_pixel, rgb.AsArray(), 3);
    dest_pixel[3] = alpha;
}

template <PixelFormat format, bool converted>
constexpr void EncodePixel(const u8* source, u8* dest) {
    using namespace Common::Color;
    constexpr u32 bytes_per_pixel = GetFormatBpp(format) / 8;

    if constexpr (format == PixelFormat::RGBA8 && converted) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeRGBA8(rgba, dest);
    } else if constexpr (format == PixelFormat::RGB8 && converted) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeRGB8(rgba, dest);
    } else if constexpr (format == PixelFormat::RGB565 && converted) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeRGB565(rgba, dest);
    } else if constexpr (format == PixelFormat::RGB5A1 && converted) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeRGB5A1(rgba, dest);
    } else if constexpr (format == PixelFormat::RGBA4 && converted) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeRGBA4(rgba, dest);
    } else if constexpr (format == PixelFormat::IA8) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeIA8(rgba, dest);
    } else if constexpr (format == PixelFormat::RG8) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeRG8(rgba, dest);
    } else if constexpr (format == PixelFormat::I8) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeI8(rgba, dest);
    } else if constexpr (format == PixelFormat::A8) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeA8(rgba, dest);
    } else if constexpr (format == PixelFormat::IA4) {
        Common::Vec4<u8> rgba;
        std::memcpy(rgba.AsArray(), source, 4);
        EncodeIA4(rgba, dest);
    } else if constexpr (format == PixelFormat::D24 && converted) {
        float d32;
        std::memcpy(&d32, source, sizeof(d32));
        EncodeD24(static_cast<u32>(d32 * 0xFFFFFF), dest);
    } else if constexpr (format == PixelFormat::D24S8) {
        const u32 s8d24 = std::rotr(MakeInt<u32>(source), 8);
        std::memcpy(dest, &s8d24, sizeof(u32));
    } else {
        std::memcpy(dest, source, bytes_per_pixel);
    }
}

template <PixelFormat format>
constexpr void EncodePixel4(u32 x, u32 y, const u8* source_pixel, u8* dest_tile_buffer) {
    Common::Vec4<u8> rgba;
    std::memcpy(rgba.AsArray(), source_pixel, 4);

    u8 pixel;
    if constexpr (format == PixelFormat::I4) {
        pixel = Common::Color::AverageRgbComponents(rgba);
    } else {
        pixel = rgba.a();
    }

    const u32 morton_offset = VideoCore::MortonInterleave(x, y);
    const u32 byte_offset = morton_offset >> 1;

    const u8 current_values = dest_tile_buffer[byte_offset];
    const u8 new_value = Common::Color::Convert8To4(pixel);

    if (morton_offset % 2) {
        dest_tile_buffer[byte_offset] = (new_value << 4) | (current_values & 0x0F);
    } else {
        dest_tile_buffer[byte_offset] = (current_values & 0xF0) | new_value;
    }
}

template <bool morton_to_linear, PixelFormat format, bool converted>
constexpr void MortonCopyTile(u32 stride, std::span<u8> tile_buffer, std::span<u8> linear_buffer) {
    constexpr u32 bytes_per_pixel = GetFormatBpp(format) / 8;
    constexpr u32 linear_bytes_per_pixel = converted ? 4 : GetFormatBytesPerPixel(format);
    constexpr bool is_compressed = format == PixelFormat::ETC1 || format == PixelFormat::ETC1A4;
    constexpr bool is_4bit = format == PixelFormat::I4 || format == PixelFormat::A4;

    for (u32 y = 0; y < 8; y++) {
        for (u32 x = 0; x < 8; x++) {
            const auto tiled_pixel = tile_buffer.subspan(
                VideoCore::MortonInterleave(x, y) * bytes_per_pixel, bytes_per_pixel);
            const auto linear_pixel = linear_buffer.subspan(
                ((7 - y) * stride + x) * linear_bytes_per_pixel, linear_bytes_per_pixel);
            if constexpr (morton_to_linear) {
                if constexpr (is_compressed) {
                    DecodePixelETC1<format>(x, y, tile_buffer.data(), linear_pixel.data());
                } else if constexpr (is_4bit) {
                    DecodePixel4<format>(x, y, tile_buffer.data(), linear_pixel.data());
                } else {
                    DecodePixel<format, converted>(tiled_pixel.data(), linear_pixel.data());
                }
            } else {
                if constexpr (is_4bit) {
                    EncodePixel4<format>(x, y, linear_pixel.data(), tile_buffer.data());
                } else {
                    EncodePixel<format, converted>(linear_pixel.data(), tiled_pixel.data());
                }
            }
        }
    }
}

/**
 * @brief Performs morton to/from linear convertions on the provided pixel data
 * @param converted If true performs RGBA8 to/from convertion to all color formats
 * @param width, height The dimentions of the rectangular region of pixels in linear_buffer
 * @param start_offset The number of bytes from the start of the first tile to the start of
 * tiled_buffer
 * @param end_offset The number of bytes from the start of the first tile to the end of tiled_buffer
 * @param linear_buffer The linear pixel data
 * @param tiled_buffer The tiled pixel data
 *
 * The MortonCopy is at the heart of the PICA texture implementation, as it's responsible for
 * converting between linear and morton tiled layouts. The function handles both convertions but
 * there are slightly different paths and inputs for each:
 *
 * Morton to Linear:
 * During uploads, tiled_buffer is always aligned to the tile or scanline boundary depending if the
 * linear rectangle spans multiple vertical tiles. linear_buffer does not reference the entire
 * texture area, but rather the specific rectangle affected by the upload.
 *
 * Linear to Morton:
 * This is similar to the other convertion but with some differences. In this case tiled_buffer is
 * not required to be aligned to any specific boundary which requires special care.
 * start_offset/end_offset are useful here as they tell us exactly where the data should be placed
 * in the linear_buffer.
 */
template <bool morton_to_linear, PixelFormat format, bool converted = false>
static constexpr void MortonCopy(u32 width, u32 height, u32 start_offset, u32 end_offset,
                                 std::span<u8> linear_buffer, std::span<u8> tiled_buffer) {
    constexpr u32 bytes_per_pixel = GetFormatBpp(format) / 8;
    constexpr u32 aligned_bytes_per_pixel = converted ? 4 : GetFormatBytesPerPixel(format);
    constexpr u32 tile_size = GetFormatBpp(format) * 64 / 8;
    static_assert(aligned_bytes_per_pixel >= bytes_per_pixel, "");

    const u32 linear_tile_stride = (7 * width + 8) * aligned_bytes_per_pixel;
    const u32 aligned_down_start_offset = Common::AlignDown(start_offset, tile_size);
    const u32 aligned_start_offset = Common::AlignUp(start_offset, tile_size);
    const u32 aligned_end_offset = Common::AlignDown(end_offset, tile_size);
    const u32 begin_pixel_index = aligned_down_start_offset * 8 / GetFormatBpp(format);

    ASSERT(!morton_to_linear ||
           (aligned_start_offset == start_offset && aligned_end_offset == end_offset));

    // In OpenGL the texture origin is in the bottom left corner as opposed to other
    // APIs that have it at the top left. To avoid flipping texture coordinates in
    // the shader we read/write the linear buffer from the bottom up
    u32 x = (begin_pixel_index % (width * 8)) / 8;
    u32 y = (begin_pixel_index / (width * 8)) * 8;
    u32 linear_offset = ((height - 8 - y) * width + x) * aligned_bytes_per_pixel;
    u32 tiled_offset = 0;

    const auto linear_next_tile = [&] {
        x = (x + 8) % width;
        linear_offset += 8 * aligned_bytes_per_pixel;
        if (!x) {
            y = (y + 8) % height;
            if (!y) {
                return;
            }

            linear_offset -= width * 9 * aligned_bytes_per_pixel;
        }
    };

    // If during a texture download the start coordinate is not tile aligned, swizzle
    // the tile affected to a temporary buffer and copy the part we are interested in
    if (start_offset < aligned_start_offset && !morton_to_linear) {
        std::array<u8, tile_size> tmp_buf;
        auto linear_data = linear_buffer.subspan(linear_offset, linear_tile_stride);
        MortonCopyTile<morton_to_linear, format, converted>(width, tmp_buf, linear_data);

        std::memcpy(tiled_buffer.data(), tmp_buf.data() + start_offset - aligned_down_start_offset,
                    std::min(aligned_start_offset, end_offset) - start_offset);

        tiled_offset += aligned_start_offset - start_offset;
        linear_next_tile();
    }

    // If the copy spans multiple tiles, copy the fully aligned tiles in between.
    if (aligned_start_offset < aligned_end_offset) {
        const u32 tile_buffer_size = static_cast<u32>(tiled_buffer.size());
        const u32 buffer_end =
            std::min(tiled_offset + aligned_end_offset - aligned_start_offset, tile_buffer_size);
        while (tiled_offset < buffer_end) {
            auto linear_data = linear_buffer.subspan(linear_offset, linear_tile_stride);
            auto tiled_data = tiled_buffer.subspan(tiled_offset, tile_size);
            MortonCopyTile<morton_to_linear, format, converted>(width, tiled_data, linear_data);
            tiled_offset += tile_size;
            linear_next_tile();
        }
    }

    // If during a texture download the end coordinate is not tile aligned, swizzle
    // the tile affected to a temporary buffer and copy the part we are interested in
    if (end_offset > std::max(aligned_start_offset, aligned_end_offset) && !morton_to_linear) {
        std::array<u8, tile_size> tmp_buf;
        auto linear_data = linear_buffer.subspan(linear_offset, linear_tile_stride);
        MortonCopyTile<morton_to_linear, format, converted>(width, tmp_buf, linear_data);
        std::memcpy(tiled_buffer.data() + tiled_offset, tmp_buf.data(),
                    end_offset - aligned_end_offset);
    }
}

/**
 * Performs a linear copy, converting pixel formats if required.
 * @tparam decode If true, decodes the texture if needed. Otherwise, encodes if needed.
 * @tparam format Pixel format to copy.
 * @tparam converted If true, converts the texture to/from the appropriate format.
 * @param src_buffer The source pixel data
 * @param dst_buffer The destination pixel data
 * @return
 */
template <bool decode, PixelFormat format, bool converted = false>
static constexpr void LinearCopy(std::span<u8> src_buffer, std::span<u8> dst_buffer) {
    std::size_t src_size = src_buffer.size();
    std::size_t dst_size = dst_buffer.size();

    if constexpr (converted) {
        constexpr u32 encoded_bytes_per_pixel = GetFormatBpp(format) / 8;
        constexpr u32 decoded_bytes_per_pixel = 4;
        constexpr u32 src_bytes_per_pixel =
            decode ? encoded_bytes_per_pixel : decoded_bytes_per_pixel;
        constexpr u32 dst_bytes_per_pixel =
            decode ? decoded_bytes_per_pixel : encoded_bytes_per_pixel;

        src_size = Common::AlignDown(src_size, src_bytes_per_pixel);
        dst_size = Common::AlignDown(dst_size, dst_bytes_per_pixel);

        for (std::size_t src_index = 0, dst_index = 0; src_index < src_size && dst_index < dst_size;
             src_index += src_bytes_per_pixel, dst_index += dst_bytes_per_pixel) {
            const auto src_pixel = src_buffer.subspan(src_index, src_bytes_per_pixel);
            const auto dst_pixel = dst_buffer.subspan(dst_index, dst_bytes_per_pixel);
            if constexpr (decode) {
                DecodePixel<format, converted>(src_pixel.data(), dst_pixel.data());
            } else {
                EncodePixel<format, converted>(src_pixel.data(), dst_pixel.data());
            }
        }
    } else {
        std::memcpy(dst_buffer.data(), src_buffer.data(), std::min(src_size, dst_size));
    }
}

using MortonFunc = void (*)(u32, u32, u32, u32, std::span<u8>, std::span<u8>);

static constexpr std::array<MortonFunc, 18> UNSWIZZLE_TABLE = {
    MortonCopy<true, PixelFormat::RGBA8>,  // 0
    MortonCopy<true, PixelFormat::RGB8>,   // 1
    MortonCopy<true, PixelFormat::RGB5A1>, // 2
    MortonCopy<true, PixelFormat::RGB565>, // 3
    MortonCopy<true, PixelFormat::RGBA4>,  // 4
    MortonCopy<true, PixelFormat::IA8>,    // 5
    MortonCopy<true, PixelFormat::RG8>,    // 6
    MortonCopy<true, PixelFormat::I8>,     // 7
    MortonCopy<true, PixelFormat::A8>,     // 8
    MortonCopy<true, PixelFormat::IA4>,    // 9
    MortonCopy<true, PixelFormat::I4>,     // 10
    MortonCopy<true, PixelFormat::A4>,     // 11
    MortonCopy<true, PixelFormat::ETC1>,   // 12
    MortonCopy<true, PixelFormat::ETC1A4>, // 13
    MortonCopy<true, PixelFormat::D16>,    // 14
    nullptr,                               // 15
    MortonCopy<true, PixelFormat::D24>,    // 16
    MortonCopy<true, PixelFormat::D24S8>,  // 17
};

static constexpr std::array<MortonFunc, 18> UNSWIZZLE_TABLE_CONVERTED = {
    MortonCopy<true, PixelFormat::RGBA8, true>,  // 0
    MortonCopy<true, PixelFormat::RGB8, true>,   // 1
    MortonCopy<true, PixelFormat::RGB5A1, true>, // 2
    MortonCopy<true, PixelFormat::RGB565, true>, // 3
    MortonCopy<true, PixelFormat::RGBA4, true>,  // 4
    // The following formats are implicitly converted to RGBA regardless, so ignore them.
    nullptr,                                  // 5
    nullptr,                                  // 6
    nullptr,                                  // 7
    nullptr,                                  // 8
    nullptr,                                  // 9
    nullptr,                                  // 10
    nullptr,                                  // 11
    nullptr,                                  // 12
    nullptr,                                  // 13
    MortonCopy<true, PixelFormat::D16, true>, // 14
    nullptr,                                  // 15
    MortonCopy<true, PixelFormat::D24, true>, // 16
    // No conversion here as we need to do a special deinterleaving conversion elsewhere.
    nullptr, // 17
};

static constexpr std::array<MortonFunc, 18> SWIZZLE_TABLE = {
    MortonCopy<false, PixelFormat::RGBA8>,  // 0
    MortonCopy<false, PixelFormat::RGB8>,   // 1
    MortonCopy<false, PixelFormat::RGB5A1>, // 2
    MortonCopy<false, PixelFormat::RGB565>, // 3
    MortonCopy<false, PixelFormat::RGBA4>,  // 4
    MortonCopy<false, PixelFormat::IA8>,    // 5
    MortonCopy<false, PixelFormat::RG8>,    // 6
    MortonCopy<false, PixelFormat::I8>,     // 7
    MortonCopy<false, PixelFormat::A8>,     // 8
    MortonCopy<false, PixelFormat::IA4>,    // 9
    MortonCopy<false, PixelFormat::I4>,     // 10
    MortonCopy<false, PixelFormat::A4>,     // 11
    nullptr,                                // 12
    nullptr,                                // 13
    MortonCopy<false, PixelFormat::D16>,    // 14
    nullptr,                                // 15
    MortonCopy<false, PixelFormat::D24>,    // 16
    MortonCopy<false, PixelFormat::D24S8>,  // 17
};

static constexpr std::array<MortonFunc, 18> SWIZZLE_TABLE_CONVERTED = {
    MortonCopy<false, PixelFormat::RGBA8, true>,  // 0
    MortonCopy<false, PixelFormat::RGB8, true>,   // 1
    MortonCopy<false, PixelFormat::RGB5A1, true>, // 2
    MortonCopy<false, PixelFormat::RGB565, true>, // 3
    MortonCopy<false, PixelFormat::RGBA4, true>,  // 4
    // The following formats are implicitly converted from RGBA regardless, so ignore them.
    nullptr,                                   // 5
    nullptr,                                   // 6
    nullptr,                                   // 7
    nullptr,                                   // 8
    nullptr,                                   // 9
    nullptr,                                   // 10
    nullptr,                                   // 11
    nullptr,                                   // 12
    nullptr,                                   // 13
    MortonCopy<false, PixelFormat::D16, true>, // 14
    nullptr,                                   // 15
    MortonCopy<false, PixelFormat::D24, true>, // 16
    // No conversion here as we need to do a special interleaving conversion elsewhere.
    nullptr, // 17
};

using LinearFunc = void (*)(std::span<u8>, std::span<u8>);

static constexpr std::array<LinearFunc, 18> LINEAR_DECODE_TABLE = {
    LinearCopy<true, PixelFormat::RGBA8>,  // 0
    LinearCopy<true, PixelFormat::RGB8>,   // 1
    LinearCopy<true, PixelFormat::RGB5A1>, // 2
    LinearCopy<true, PixelFormat::RGB565>, // 3
    LinearCopy<true, PixelFormat::RGBA4>,  // 4
    // These formats cannot be used linearly and can be ignored.
    nullptr,                              // 5
    nullptr,                              // 6
    nullptr,                              // 7
    nullptr,                              // 8
    nullptr,                              // 9
    nullptr,                              // 10
    nullptr,                              // 11
    nullptr,                              // 12
    nullptr,                              // 13
    LinearCopy<true, PixelFormat::D16>,   // 14
    nullptr,                              // 15
    LinearCopy<true, PixelFormat::D24>,   // 16
    LinearCopy<true, PixelFormat::D24S8>, // 17
};

static constexpr std::array<LinearFunc, 18> LINEAR_DECODE_TABLE_CONVERTED = {
    LinearCopy<true, PixelFormat::RGBA8, true>,  // 0
    LinearCopy<true, PixelFormat::RGB8, true>,   // 1
    LinearCopy<true, PixelFormat::RGB5A1, true>, // 2
    LinearCopy<true, PixelFormat::RGB565, true>, // 3
    LinearCopy<true, PixelFormat::RGBA4, true>,  // 4
    // These formats cannot be used linearly and can be ignored.
    nullptr,                                  // 5
    nullptr,                                  // 6
    nullptr,                                  // 7
    nullptr,                                  // 8
    nullptr,                                  // 9
    nullptr,                                  // 10
    nullptr,                                  // 11
    nullptr,                                  // 12
    nullptr,                                  // 13
    LinearCopy<true, PixelFormat::D16, true>, // 14
    nullptr,                                  // 15
    LinearCopy<true, PixelFormat::D24, true>, // 16
    // No conversion here as we need to do a special deinterleaving conversion elsewhere.
    nullptr, // 17
};

static constexpr std::array<LinearFunc, 18> LINEAR_ENCODE_TABLE = {
    LinearCopy<false, PixelFormat::RGBA8>,  // 0
    LinearCopy<false, PixelFormat::RGB8>,   // 1
    LinearCopy<false, PixelFormat::RGB5A1>, // 2
    LinearCopy<false, PixelFormat::RGB565>, // 3
    LinearCopy<false, PixelFormat::RGBA4>,  // 4
    // These formats cannot be used linearly and can be ignored.
    nullptr,                               // 5
    nullptr,                               // 6
    nullptr,                               // 7
    nullptr,                               // 8
    nullptr,                               // 9
    nullptr,                               // 10
    nullptr,                               // 11
    nullptr,                               // 12
    nullptr,                               // 13
    LinearCopy<false, PixelFormat::D16>,   // 14
    nullptr,                               // 15
    LinearCopy<false, PixelFormat::D24>,   // 16
    LinearCopy<false, PixelFormat::D24S8>, // 17
};

static constexpr std::array<LinearFunc, 18> LINEAR_ENCODE_TABLE_CONVERTED = {
    LinearCopy<false, PixelFormat::RGBA8, true>,  // 0
    LinearCopy<false, PixelFormat::RGB8, true>,   // 1
    LinearCopy<false, PixelFormat::RGB5A1, true>, // 2
    LinearCopy<false, PixelFormat::RGB565, true>, // 3
    LinearCopy<false, PixelFormat::RGBA4, true>,  // 4
    // These formats cannot be used linearly and can be ignored.
    nullptr,                                   // 5
    nullptr,                                   // 6
    nullptr,                                   // 7
    nullptr,                                   // 8
    nullptr,                                   // 9
    nullptr,                                   // 10
    nullptr,                                   // 11
    nullptr,                                   // 12
    nullptr,                                   // 13
    LinearCopy<false, PixelFormat::D16, true>, // 14
    nullptr,                                   // 15
    LinearCopy<false, PixelFormat::D24, true>, // 16
    // No conversion here as we need to do a special interleaving conversion elsewhere.
    nullptr, // 17
};

} // namespace VideoCore
