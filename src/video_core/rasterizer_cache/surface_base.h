// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/icl/interval_set.hpp>
#include "video_core/rasterizer_cache/surface_params.h"
#include "video_core/rasterizer_cache/utils.h"

namespace VideoCore {

using SurfaceRegions = boost::icl::interval_set<PAddr, std::less, SurfaceInterval>;

struct Material;

enum class SurfaceFlagBits : u32 {
    Registered = 1 << 0,   ///< Surface is registed in the rasterizer cache.
    Picked = 1 << 1,       ///< Surface has been picked when searching for a match.
    Tracked = 1 << 2,      ///< Surface is part of a texture cube and should be tracked.
    Custom = 1 << 3,       ///< Surface texture has been replaced with a custom texture.
    ShadowMap = 1 << 4,    ///< Surface is used during shadow rendering.
    RenderTarget = 1 << 5, ///< Surface was a render target.
};
DECLARE_ENUM_FLAG_OPERATORS(SurfaceFlagBits);

class SurfaceBase : public SurfaceParams {
public:
    SurfaceBase(const SurfaceParams& params);
    ~SurfaceBase();

    /// Returns true when this surface can be used to fill the fill_interval of dest_surface
    bool CanFill(const SurfaceParams& dest_surface, SurfaceInterval fill_interval) const;

    /// Returns true when surface can validate copy_interval of dest_surface
    bool CanCopy(const SurfaceParams& dest_surface, SurfaceInterval copy_interval) const;

    /// Returns the region of the biggest valid rectange within interval
    SurfaceInterval GetCopyableInterval(const SurfaceParams& params) const;

    /// Returns the clear value used to validate another surface from this fill surface
    ClearValue MakeClearValue(PAddr copy_addr, PixelFormat dst_format);

    /// Returns the internal surface extent.
    Extent RealExtent(bool scaled = true) const;

    /// Returns true if the surface contains a custom material with a normal map.
    bool HasNormalMap() const noexcept;

    bool Overlaps(PAddr overlap_addr, std::size_t overlap_size) const noexcept {
        const PAddr overlap_end = overlap_addr + static_cast<PAddr>(overlap_size);
        return addr < overlap_end && overlap_addr < end;
    }

    u64 ModificationTick() const noexcept {
        return modification_tick;
    }

    bool IsCustom() const noexcept {
        return True(flags & SurfaceFlagBits::Custom) && custom_format != CustomPixelFormat::Invalid;
    }

    bool IsRegionValid(SurfaceInterval interval) const {
        return invalid_regions.find(interval) == invalid_regions.end();
    }

    void MarkValid(SurfaceInterval interval) {
        invalid_regions.erase(interval);
        modification_tick++;
    }

    void MarkInvalid(SurfaceInterval interval) {
        invalid_regions.insert(interval);
        modification_tick++;
    }

    bool IsFullyInvalid() const {
        auto interval = GetInterval();
        return *invalid_regions.equal_range(interval).first == interval;
    }

private:
    /// Returns the fill buffer value starting from copy_addr
    std::array<u8, 4> MakeFillBuffer(PAddr copy_addr);

public:
    SurfaceFlagBits flags{};
    const Material* material = nullptr;
    SurfaceRegions invalid_regions;
    u32 fill_size = 0;
    std::array<u8, 4> fill_data;
    u64 modification_tick = 1;
};

} // namespace VideoCore
