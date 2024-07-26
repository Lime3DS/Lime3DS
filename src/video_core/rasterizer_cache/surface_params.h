// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/icl/right_open_interval.hpp>
#include "common/math_util.h"
#include "video_core/custom_textures/custom_format.h"
#include "video_core/rasterizer_cache/pixel_format.h"

namespace VideoCore {

using SurfaceInterval = boost::icl::right_open_interval<PAddr>;

constexpr std::size_t MAX_PICA_LEVELS = 8;

class SurfaceParams {
public:
    /// Returns true if other_surface matches exactly params
    bool ExactMatch(const SurfaceParams& other_surface) const;

    /// Returns true if sub_surface is a subrect of params
    bool CanSubRect(const SurfaceParams& sub_surface) const;

    /// Returns true if other_surface can be used for reinterpretion.
    bool CanReinterpret(const SurfaceParams& other_surface);

    /// Returns true if params can be used for texcopy
    bool CanTexCopy(const SurfaceParams& texcopy_params) const;

    /// Updates remaining members from the already set addr, width, height and pixel_format
    void UpdateParams();

    /// Returns the unscaled rectangle referenced by sub_surface
    Common::Rectangle<u32> GetSubRect(const SurfaceParams& sub_surface) const;

    /// Returns the scaled rectangle referenced by sub_surface
    Common::Rectangle<u32> GetScaledSubRect(const SurfaceParams& sub_surface) const;

    /// Returns the outer rectangle containing interval
    SurfaceParams FromInterval(SurfaceInterval interval) const;

    /// Returns the address interval referenced by unscaled_rect
    SurfaceInterval GetSubRectInterval(Common::Rectangle<u32> unscaled_rect, u32 level = 0) const;

    /// Return the address interval of the provided level
    SurfaceInterval LevelInterval(u32 level) const;

    /// Returns the level of the provided address
    u32 LevelOf(PAddr addr) const;

    /// Returns a string identifier of the params object
    std::string DebugName(bool scaled, bool custom = false) const noexcept;

    bool operator==(const SurfaceParams& other) const noexcept;

    [[nodiscard]] SurfaceInterval GetInterval() const noexcept {
        return SurfaceInterval{addr, end};
    }

    [[nodiscard]] u32 GetFormatBpp() const noexcept {
        return VideoCore::GetFormatBpp(pixel_format);
    }

    [[nodiscard]] u32 GetScaledWidth() const noexcept {
        return width * res_scale;
    }

    [[nodiscard]] u32 GetScaledHeight() const noexcept {
        return height * res_scale;
    }

    [[nodiscard]] Common::Rectangle<u32> GetRect(u32 level = 0) const noexcept {
        return {0, height >> level, width >> level, 0};
    }

    [[nodiscard]] Common::Rectangle<u32> GetScaledRect(u32 level = 0) const noexcept {
        return {0, GetScaledHeight() >> level, GetScaledWidth() >> level, 0};
    }

    [[nodiscard]] u32 PixelsInBytes(u32 size) const noexcept {
        return size * 8 / GetFormatBpp();
    }

    [[nodiscard]] u32 BytesInPixels(u32 pixels) const noexcept {
        return pixels * GetFormatBpp() / 8;
    }

private:
    /// Computes the offset of each mipmap level
    void CalculateMipLevelOffsets();

    /// Calculates total surface size taking mipmaps into account
    u32 CalculateSurfaceSize() const;

public:
    PAddr addr = 0;
    PAddr end = 0;
    u32 size = 0;

    u32 width = 0;
    u32 height = 0;
    u32 stride = 0;
    u32 levels = 1;
    u32 res_scale = 1;

    bool is_tiled = false;
    TextureType texture_type = TextureType::Texture2D;
    PixelFormat pixel_format = PixelFormat::Invalid;
    CustomPixelFormat custom_format = CustomPixelFormat::Invalid;
    SurfaceType type = SurfaceType::Invalid;

    std::array<u32, MAX_PICA_LEVELS> mipmap_offsets{};
};

} // namespace VideoCore
