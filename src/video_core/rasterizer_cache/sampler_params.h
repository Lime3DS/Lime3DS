// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <compare>
#include "common/hash.h"
#include "video_core/pica/regs_texturing.h"

namespace VideoCore {

struct SamplerParams {
    using TextureConfig = Pica::TexturingRegs::TextureConfig;
    TextureConfig::TextureFilter mag_filter;
    TextureConfig::TextureFilter min_filter;
    TextureConfig::TextureFilter mip_filter;
    TextureConfig::WrapMode wrap_s;
    TextureConfig::WrapMode wrap_t;
    u32 border_color = 0;
    u32 lod_min = 0;
    u32 lod_max = 0;
    s32 lod_bias = 0;

    auto operator<=>(const SamplerParams&) const noexcept = default;

    const u64 Hash() const {
        return Common::ComputeHash64(this, sizeof(SamplerParams));
    }
};
static_assert(std::has_unique_object_representations_v<SamplerParams>,
              "SamplerParams is not suitable for hashing");

} // namespace VideoCore

namespace std {
template <>
struct hash<VideoCore::SamplerParams> {
    std::size_t operator()(const VideoCore::SamplerParams& params) const noexcept {
        return params.Hash();
    }
};
} // namespace std
