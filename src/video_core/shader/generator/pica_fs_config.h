// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/hash.h"
#include "video_core/pica/regs_internal.h"
#include "video_core/shader/generator/profile.h"

namespace Pica::Shader {

struct BlendConfig {
    Pica::FramebufferRegs::BlendEquation eq;
    Pica::FramebufferRegs::BlendFactor src_factor;
    Pica::FramebufferRegs::BlendFactor dst_factor;
};

struct FramebufferConfig {
    explicit FramebufferConfig(const Pica::RegsInternal& regs, const Profile& profile);

    union {
        u32 raw{};
        BitField<0, 3, Pica::FramebufferRegs::CompareFunc> alpha_test_func;
        BitField<3, 2, Pica::RasterizerRegs::ScissorMode> scissor_test_mode;
        BitField<5, 1, Pica::RasterizerRegs::DepthBuffering> depthmap_enable;
        BitField<6, 4, Pica::FramebufferRegs::LogicOp> logic_op;
        BitField<10, 1, u32> shadow_rendering;
    };
    BlendConfig rgb_blend{};
    BlendConfig alpha_blend{};
};
static_assert(std::has_unique_object_representations_v<FramebufferConfig>);

struct TevStageConfigRaw {
    u32 sources_raw;
    u32 modifiers_raw;
    u32 ops_raw;
    u32 scales_raw;
    operator Pica::TexturingRegs::TevStageConfig() const noexcept {
        return {
            .sources_raw = sources_raw,
            .modifiers_raw = modifiers_raw,
            .ops_raw = ops_raw,
            .const_color = 0,
            .scales_raw = scales_raw,
        };
    }
};

union TextureBorder {
    BitField<0, 1, u32> enable_s;
    BitField<1, 1, u32> enable_t;
};

struct TextureConfig {
    explicit TextureConfig(const Pica::TexturingRegs& regs, const Profile& profile);

    union {
        u32 raw{};
        BitField<0, 3, Pica::TexturingRegs::TextureConfig::TextureType> texture0_type;
        BitField<3, 1, u32> texture2_use_coord1;
        BitField<4, 8, u32> combiner_buffer_input;
        BitField<12, 3, Pica::TexturingRegs::FogMode> fog_mode;
        BitField<15, 1, u32> fog_flip;
        BitField<16, 1, u32> shadow_texture_orthographic;
    };
    std::array<TextureBorder, 3> texture_border_color{};
    std::array<TevStageConfigRaw, 6> tev_stages{};
};
static_assert(std::has_unique_object_representations_v<TextureConfig>);

union Light {
    u16 raw;
    BitField<0, 3, u16> num;
    BitField<3, 1, u16> directional;
    BitField<4, 1, u16> two_sided_diffuse;
    BitField<5, 1, u16> dist_atten_enable;
    BitField<6, 1, u16> spot_atten_enable;
    BitField<7, 1, u16> geometric_factor_0;
    BitField<8, 1, u16> geometric_factor_1;
    BitField<9, 1, u16> shadow_enable;
};
static_assert(std::has_unique_object_representations_v<Light>);

struct LutConfig {
    union {
        u32 raw;
        BitField<0, 1, u32> enable;
        BitField<1, 1, u32> abs_input;
        BitField<2, 3, Pica::LightingRegs::LightingLutInput> type;
    };
    f32 scale;
};

struct LightConfig {
    explicit LightConfig(const Pica::LightingRegs& regs);

    union {
        u32 raw{};
        BitField<0, 1, u32> enable;
        BitField<1, 4, u32> src_num;
        BitField<5, 2, Pica::LightingRegs::LightingBumpMode> bump_mode;
        BitField<7, 2, u32> bump_selector;
        BitField<9, 1, u32> bump_renorm;
        BitField<10, 1, u32> clamp_highlights;
        BitField<11, 4, Pica::LightingRegs::LightingConfig> config;
        BitField<15, 1, u32> enable_primary_alpha;
        BitField<16, 1, u32> enable_secondary_alpha;
        BitField<17, 1, u32> enable_shadow;
        BitField<18, 1, u32> shadow_primary;
        BitField<19, 1, u32> shadow_secondary;
        BitField<20, 1, u32> shadow_invert;
        BitField<21, 1, u32> shadow_alpha;
        BitField<22, 2, u32> shadow_selector;
    };
    LutConfig lut_d0{};
    LutConfig lut_d1{};
    LutConfig lut_sp{};
    LutConfig lut_fr{};
    LutConfig lut_rr{};
    LutConfig lut_rg{};
    LutConfig lut_rb{};
    std::array<Light, 8> lights{};
};

struct ProcTexConfig {
    explicit ProcTexConfig(const Pica::TexturingRegs& regs);

    union {
        u32 raw{};
        BitField<0, 1, u32> enable;
        BitField<1, 2, u32> coord;
        BitField<3, 3, Pica::TexturingRegs::ProcTexClamp> u_clamp;
        BitField<6, 3, Pica::TexturingRegs::ProcTexClamp> v_clamp;
        BitField<9, 4, Pica::TexturingRegs::ProcTexCombiner> color_combiner;
        BitField<13, 4, Pica::TexturingRegs::ProcTexCombiner> alpha_combiner;
        BitField<17, 3, Pica::TexturingRegs::ProcTexFilter> lut_filter;
        BitField<20, 1, u32> separate_alpha;
        BitField<21, 1, u32> noise_enable;
        BitField<22, 2, Pica::TexturingRegs::ProcTexShift> u_shift;
        BitField<24, 2, Pica::TexturingRegs::ProcTexShift> v_shift;
    };
    s32 lut_width{};
    s32 lut_offset0{};
    s32 lut_offset1{};
    s32 lut_offset2{};
    s32 lut_offset3{};
    u16 lod_min{};
    u16 lod_max{};
};
static_assert(std::has_unique_object_representations_v<ProcTexConfig>);

union UserConfig {
    u32 raw{};
    BitField<0, 1, u32> use_custom_normal;
};
static_assert(std::has_unique_object_representations_v<UserConfig>);

struct FSConfig {
    explicit FSConfig(const Pica::RegsInternal& regs, const UserConfig& user,
                      const Profile& profile);

    [[nodiscard]] bool TevStageUpdatesCombinerBufferColor(u32 stage_index) const {
        return (stage_index < 4) && (texture.combiner_buffer_input & (1 << stage_index));
    }

    [[nodiscard]] bool TevStageUpdatesCombinerBufferAlpha(u32 stage_index) const {
        return (stage_index < 4) && ((texture.combiner_buffer_input >> 4) & (1 << stage_index));
    }

    [[nodiscard]] bool EmulateBlend() const {
        return framebuffer.rgb_blend.eq != Pica::FramebufferRegs::BlendEquation::Add ||
               framebuffer.alpha_blend.eq != Pica::FramebufferRegs::BlendEquation::Add;
    }

    [[nodiscard]] bool UsesShadowPipeline() const {
        const auto texture0_type = texture.texture0_type.Value();
        return texture0_type == Pica::TexturingRegs::TextureConfig::Shadow2D ||
               texture0_type == Pica::TexturingRegs::TextureConfig::ShadowCube ||
               framebuffer.shadow_rendering.Value();
    }

    bool operator==(const FSConfig& other) const noexcept {
        return std::memcmp(this, &other, sizeof(FSConfig)) == 0;
    }

    std::size_t Hash() const noexcept {
        return Common::ComputeHash64(this, sizeof(FSConfig));
    }

    FramebufferConfig framebuffer;
    TextureConfig texture;
    LightConfig lighting;
    ProcTexConfig proctex;
    UserConfig user;
};

} // namespace Pica::Shader

namespace std {
template <>
struct hash<Pica::Shader::FSConfig> {
    std::size_t operator()(const Pica::Shader::FSConfig& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std
