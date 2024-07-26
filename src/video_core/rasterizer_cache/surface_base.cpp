// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "video_core/custom_textures/material.h"
#include "video_core/rasterizer_cache/surface_base.h"
#include "video_core/texture/texture_decode.h"

namespace VideoCore {

SurfaceBase::SurfaceBase(const SurfaceParams& params) : SurfaceParams{params} {}

SurfaceBase::~SurfaceBase() = default;

bool SurfaceBase::CanFill(const SurfaceParams& dest_surface, SurfaceInterval fill_interval) const {
    if (type == SurfaceType::Fill && IsRegionValid(fill_interval) &&
        boost::icl::first(fill_interval) >= addr &&
        boost::icl::last_next(fill_interval) <= end && // dest_surface is within our fill range
        dest_surface.FromInterval(fill_interval).GetInterval() ==
            fill_interval) { // make sure interval is a rectangle in dest surface
        if (fill_size * 8 != dest_surface.GetFormatBpp()) {
            // Check if bits repeat for our fill_size
            const u32 dest_bytes_per_pixel = std::max(dest_surface.GetFormatBpp() / 8, 1u);
            std::vector<u8> fill_test(fill_size * dest_bytes_per_pixel);

            for (u32 i = 0; i < dest_bytes_per_pixel; ++i) {
                std::memcpy(&fill_test[i * fill_size], &fill_data[0], fill_size);
            }

            for (u32 i = 0; i < fill_size; ++i) {
                if (std::memcmp(&fill_test[dest_bytes_per_pixel * i], &fill_test[0],
                                dest_bytes_per_pixel) != 0) {
                    return false;
                }
            }

            if (dest_surface.GetFormatBpp() == 4 && (fill_test[0] & 0xF) != (fill_test[0] >> 4)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool SurfaceBase::CanCopy(const SurfaceParams& dest_surface, SurfaceInterval copy_interval) const {
    const SurfaceParams subrect_params = dest_surface.FromInterval(copy_interval);
    ASSERT(subrect_params.GetInterval() == copy_interval);

    if (CanSubRect(subrect_params)) {
        return true;
    }

    if (CanFill(dest_surface, copy_interval)) {
        return true;
    }

    return false;
}

SurfaceInterval SurfaceBase::GetCopyableInterval(const SurfaceParams& params) const {
    SurfaceInterval result{};
    const u32 tile_align = params.BytesInPixels(params.is_tiled ? 8 * 8 : 1);
    const auto valid_regions =
        SurfaceRegions{params.GetInterval() & GetInterval()} - invalid_regions;

    for (auto& valid_interval : valid_regions) {
        const SurfaceInterval aligned_interval{
            params.addr +
                Common::AlignUp(boost::icl::first(valid_interval) - params.addr, tile_align),
            params.addr +
                Common::AlignDown(boost::icl::last_next(valid_interval) - params.addr, tile_align)};

        if (tile_align > boost::icl::length(valid_interval) ||
            boost::icl::length(aligned_interval) == 0) {
            continue;
        }

        // Get the rectangle within aligned_interval
        const u32 stride_bytes = params.BytesInPixels(params.stride) * (params.is_tiled ? 8 : 1);
        SurfaceInterval rect_interval{
            params.addr +
                Common::AlignUp(boost::icl::first(aligned_interval) - params.addr, stride_bytes),
            params.addr + Common::AlignDown(boost::icl::last_next(aligned_interval) - params.addr,
                                            stride_bytes),
        };

        if (boost::icl::first(rect_interval) > boost::icl::last_next(rect_interval)) {
            // 1 row
            rect_interval = aligned_interval;
        } else if (boost::icl::length(rect_interval) == 0) {
            // 2 rows that do not make a rectangle, return the larger one
            const SurfaceInterval row1{boost::icl::first(aligned_interval),
                                       boost::icl::first(rect_interval)};
            const SurfaceInterval row2{boost::icl::first(rect_interval),
                                       boost::icl::last_next(aligned_interval)};
            rect_interval = (boost::icl::length(row1) > boost::icl::length(row2)) ? row1 : row2;
        }

        if (boost::icl::length(rect_interval) > boost::icl::length(result)) {
            result = rect_interval;
        }
    }
    return result;
}

Extent SurfaceBase::RealExtent(bool scaled) const {
    const bool is_custom = IsCustom();
    u32 real_width = width;
    u32 real_height = height;
    if (is_custom) {
        real_width = material->width;
        real_height = material->height;
    } else if (scaled) {
        real_width = GetScaledWidth();
        real_height = GetScaledHeight();
    }
    return Extent{
        .width = real_width,
        .height = real_height,
    };
}

bool SurfaceBase::HasNormalMap() const noexcept {
    return material && material->Map(MapType::Normal) != nullptr;
}

ClearValue SurfaceBase::MakeClearValue(PAddr copy_addr, PixelFormat dst_format) {
    const SurfaceType dst_type = GetFormatType(dst_format);
    const std::array fill_buffer = MakeFillBuffer(copy_addr);

    ClearValue result{};
    switch (dst_type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
    case SurfaceType::Fill: {
        Pica::Texture::TextureInfo tex_info{};
        tex_info.format = static_cast<Pica::TexturingRegs::TextureFormat>(dst_format);
        const auto color = Pica::Texture::LookupTexture(fill_buffer.data(), 0, 0, tex_info);
        result.color = color / 255.f;
        break;
    }
    case SurfaceType::Depth: {
        u32 depth_uint = 0;
        if (dst_format == PixelFormat::D16) {
            std::memcpy(&depth_uint, fill_buffer.data(), 2);
            result.depth = depth_uint / 65535.0f; // 2^16 - 1
        } else if (dst_format == PixelFormat::D24) {
            std::memcpy(&depth_uint, fill_buffer.data(), 3);
            result.depth = depth_uint / 16777215.0f; // 2^24 - 1
        }
        break;
    }
    case SurfaceType::DepthStencil: {
        u32 clear_value_uint;
        std::memcpy(&clear_value_uint, fill_buffer.data(), sizeof(u32));
        result.depth = (clear_value_uint & 0xFFFFFF) / 16777215.0f; // 2^24 - 1
        result.stencil = (clear_value_uint >> 24);
        break;
    }
    default:
        UNREACHABLE_MSG("Invalid surface type!");
    }

    return result;
}

std::array<u8, 4> SurfaceBase::MakeFillBuffer(PAddr copy_addr) {
    const PAddr fill_offset = (copy_addr - addr) % fill_size;
    std::array<u8, 4> fill_buffer;

    u32 fill_buff_pos = fill_offset;
    for (std::size_t i = 0; i < fill_buffer.size(); i++) {
        fill_buffer[i] = fill_data[fill_buff_pos++ % fill_size];
    }

    return fill_buffer;
}

} // namespace VideoCore
