// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/hash.h"
#include "video_core/pica/regs_texturing.h"
#include "video_core/rasterizer_cache/slot_id.h"

namespace VideoCore {

struct TextureCube {
    SurfaceId surface_id;
    std::array<SurfaceId, 6> face_ids;
    std::array<u64, 6> ticks;
};

struct TextureCubeConfig {
    PAddr px;
    PAddr nx;
    PAddr py;
    PAddr ny;
    PAddr pz;
    PAddr nz;
    u32 width;
    u32 levels;
    Pica::TexturingRegs::TextureFormat format;

    bool operator==(const TextureCubeConfig& rhs) const {
        return std::memcmp(this, &rhs, sizeof(TextureCubeConfig)) == 0;
    }

    bool operator!=(const TextureCubeConfig& rhs) const {
        return std::memcmp(this, &rhs, sizeof(TextureCubeConfig)) != 0;
    }

    const u64 Hash() const {
        return Common::ComputeHash64(this, sizeof(TextureCubeConfig));
    }
};

} // namespace VideoCore

namespace std {
template <>
struct hash<VideoCore::TextureCubeConfig> {
    std::size_t operator()(const VideoCore::TextureCubeConfig& config) const noexcept {
        return config.Hash();
    }
};
} // namespace std
