// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "video_core/rasterizer_cache/surface_params.h"

namespace VideoCore {

bool SurfaceParams::ExactMatch(const SurfaceParams& other_surface) const {
    return std::tie(other_surface.addr, other_surface.width, other_surface.height,
                    other_surface.stride, other_surface.pixel_format, other_surface.is_tiled) ==
               std::tie(addr, width, height, stride, pixel_format, is_tiled) &&
           pixel_format != PixelFormat::Invalid && levels >= other_surface.levels;
}

bool SurfaceParams::CanSubRect(const SurfaceParams& sub_surface) const {
    const u32 level = LevelOf(sub_surface.addr);
    return sub_surface.addr >= addr && sub_surface.end <= end &&
           sub_surface.pixel_format == pixel_format && pixel_format != PixelFormat::Invalid &&
           sub_surface.is_tiled == is_tiled &&
           (sub_surface.addr - mipmap_offsets[level]) % BytesInPixels(is_tiled ? 64 : 1) == 0 &&
           (sub_surface.stride == (stride >> level) ||
            sub_surface.height <= (is_tiled ? 8u : 1u)) &&
           GetSubRect(sub_surface).right <= stride;
}

bool SurfaceParams::CanReinterpret(const SurfaceParams& other_surface) {
    return other_surface.addr >= addr && other_surface.end <= end &&
           pixel_format != PixelFormat::Invalid && pixel_format != other_surface.pixel_format &&
           GetFormatBpp() == other_surface.GetFormatBpp() && other_surface.is_tiled == is_tiled &&
           other_surface.stride == stride &&
           (other_surface.addr - addr) % BytesInPixels(is_tiled ? 64 : 1) == 0 &&
           GetSubRect(other_surface).right <= stride;
}

bool SurfaceParams::CanTexCopy(const SurfaceParams& texcopy_params) const {
    const SurfaceInterval copy_interval = texcopy_params.GetInterval();
    if (pixel_format == PixelFormat::Invalid || addr > texcopy_params.addr ||
        end < texcopy_params.end) {
        return false;
    }

    if (texcopy_params.width != texcopy_params.stride) {
        const u32 tile_stride = BytesInPixels(stride * (is_tiled ? 8 : 1));
        return (texcopy_params.addr - addr) % BytesInPixels(is_tiled ? 64 : 1) == 0 &&
               texcopy_params.width % BytesInPixels(is_tiled ? 64 : 1) == 0 &&
               (texcopy_params.height == 1 || texcopy_params.stride == tile_stride) &&
               ((texcopy_params.addr - addr) % tile_stride) + texcopy_params.width <= tile_stride;
    }

    const u32 target_level = LevelOf(texcopy_params.addr);
    if ((LevelInterval(target_level) & copy_interval) != copy_interval) {
        return false;
    }

    return FromInterval(copy_interval).GetInterval() == copy_interval;
}

void SurfaceParams::UpdateParams() {
    if (stride == 0) {
        stride = width;
    }

    type = GetFormatType(pixel_format);
    if (levels != 1) {
        ASSERT(stride == width);
        CalculateMipLevelOffsets();
        size = CalculateSurfaceSize();
    } else {
        mipmap_offsets[0] = addr;
        size = !is_tiled ? BytesInPixels(stride * (height - 1) + width)
                         : BytesInPixels(stride * 8 * (height / 8 - 1) + width * 8);
    }

    end = addr + size;
}

Common::Rectangle<u32> SurfaceParams::GetSubRect(const SurfaceParams& sub_surface) const {
    const u32 level = LevelOf(sub_surface.addr);
    const u32 begin_pixel_index = PixelsInBytes(sub_surface.addr - mipmap_offsets[level]);
    ASSERT(stride == width || level == 0);

    const u32 stride_lod = stride >> level;
    if (is_tiled) {
        const u32 x0 = (begin_pixel_index % (stride_lod * 8)) / 8;
        const u32 y0 = (begin_pixel_index / (stride_lod * 8)) * 8;
        const u32 height_lod = height >> level;
        // Top to bottom
        return {x0, height_lod - y0, x0 + sub_surface.width,
                height_lod - (y0 + sub_surface.height)};
    }

    const u32 x0 = begin_pixel_index % stride_lod;
    const u32 y0 = begin_pixel_index / stride_lod;
    // Bottom to top
    return {x0, y0 + sub_surface.height, x0 + sub_surface.width, y0};
}

Common::Rectangle<u32> SurfaceParams::GetScaledSubRect(const SurfaceParams& sub_surface) const {
    return GetSubRect(sub_surface) * res_scale;
}

SurfaceParams SurfaceParams::FromInterval(SurfaceInterval interval) const {
    SurfaceParams params = *this;
    const u32 level = LevelOf(interval.lower());
    const PAddr end_addr = interval.upper();

    // Ensure provided interval is contained in a single level
    ASSERT(level == LevelOf(end_addr) || end_addr == end || end_addr == mipmap_offsets[level + 1]);

    params.width >>= level;
    params.stride >>= level;

    const u32 tiled_size = is_tiled ? 8 : 1;
    const u32 stride_tiled_bytes = BytesInPixels(params.stride * tiled_size);
    ASSERT(stride == width || level == 0);

    const PAddr start = mipmap_offsets[level];
    PAddr aligned_start =
        start + Common::AlignDown(boost::icl::first(interval) - start, stride_tiled_bytes);
    PAddr aligned_end =
        start + Common::AlignUp(boost::icl::last_next(interval) - start, stride_tiled_bytes);

    if (aligned_end - aligned_start > stride_tiled_bytes) {
        params.addr = aligned_start;
        params.height = (aligned_end - aligned_start) / BytesInPixels(params.stride);
    } else {
        // 1 row
        ASSERT(aligned_end - aligned_start == stride_tiled_bytes);
        const u32 tiled_alignment = BytesInPixels(is_tiled ? 8 * 8 : 1);

        aligned_start =
            start + Common::AlignDown(boost::icl::first(interval) - start, tiled_alignment);
        aligned_end =
            start + Common::AlignUp(boost::icl::last_next(interval) - start, tiled_alignment);

        params.addr = aligned_start;
        params.width = PixelsInBytes(aligned_end - aligned_start) / tiled_size;
        params.stride = params.width;
        params.height = tiled_size;
    }

    params.levels = 1;
    params.UpdateParams();
    return params;
}

SurfaceInterval SurfaceParams::GetSubRectInterval(Common::Rectangle<u32> unscaled_rect,
                                                  u32 level) const {
    if (unscaled_rect.GetHeight() == 0 || unscaled_rect.GetWidth() == 0) [[unlikely]] {
        return {};
    }

    if (is_tiled) {
        unscaled_rect.left = Common::AlignDown(unscaled_rect.left, 8) * 8;
        unscaled_rect.bottom = Common::AlignDown(unscaled_rect.bottom, 8) / 8;
        unscaled_rect.right = Common::AlignUp(unscaled_rect.right, 8) * 8;
        unscaled_rect.top = Common::AlignUp(unscaled_rect.top, 8) / 8;
    }

    const u32 stride_tiled = (!is_tiled ? stride : stride * 8) >> level;
    const u32 pixels = (unscaled_rect.GetHeight() - 1) * stride_tiled + unscaled_rect.GetWidth();
    const u32 pixel_offset =
        stride_tiled * (!is_tiled ? unscaled_rect.bottom : (height / 8) - unscaled_rect.top) +
        unscaled_rect.left;

    const PAddr start = mipmap_offsets[level];
    return {start + BytesInPixels(pixel_offset), start + BytesInPixels(pixel_offset + pixels)};
}

void SurfaceParams::CalculateMipLevelOffsets() {
    ASSERT(levels <= MAX_PICA_LEVELS && stride == width);

    u32 level_width = width;
    u32 level_height = height;
    u32 offset = addr;

    for (u32 level = 0; level < levels; level++) {
        mipmap_offsets[level] = offset;
        offset += BytesInPixels(level_width * level_height);

        level_width >>= 1;
        level_height >>= 1;
    }
}

u32 SurfaceParams::CalculateSurfaceSize() const {
    ASSERT(levels <= MAX_PICA_LEVELS && stride == width);

    u32 level_width = width;
    u32 level_height = height;
    u32 size = 0;

    for (u32 level = 0; level < levels; level++) {
        size += BytesInPixels(level_width * level_height);
        level_width >>= 1;
        level_height >>= 1;
    }
    return size;
}

SurfaceInterval SurfaceParams::LevelInterval(u32 level) const {
    ASSERT(levels > level);
    const PAddr start_addr = mipmap_offsets[level];
    const PAddr end_addr = level == (levels - 1) ? end : mipmap_offsets[level + 1];
    return {start_addr, end_addr};
}

u32 SurfaceParams::LevelOf(PAddr level_addr) const {
    if (level_addr < addr || level_addr > end) {
        return 0;
    }

    u32 level = levels - 1;
    while (mipmap_offsets[level] > level_addr) {
        level--;
    }
    return level;
}

std::string SurfaceParams::DebugName(bool scaled, bool custom) const noexcept {
    const u32 scaled_width = scaled ? GetScaledWidth() : width;
    const u32 scaled_height = scaled ? GetScaledHeight() : height;
    return fmt::format("Surface: {}x{} {} {} levels from {:#x} to {:#x} ({}{})", scaled_width,
                       scaled_height, PixelFormatAsString(pixel_format), levels, addr, end,
                       custom ? "custom," : "", scaled ? "scaled" : "unscaled");
}

bool SurfaceParams::operator==(const SurfaceParams& other) const noexcept {
    return std::tie(addr, end, width, height, stride, levels, is_tiled, texture_type, pixel_format,
                    custom_format) ==
           std::tie(other.addr, other.end, other.width, other.height, other.stride, other.levels,
                    other.is_tiled, other.texture_type, other.pixel_format, other.custom_format);
}

} // namespace VideoCore
