// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/hash.h"
#include "video_core/pica/regs_internal.h"
#include "video_core/shader/generator/profile.h"

#define LAYOUT_HASH static_cast<u64>(sizeof(T)), static_cast<u64>(alignof(T))
#define FIELD_HASH(x) static_cast<u64>(offsetof(T, x)), static_cast<u64>(sizeof(x))

namespace Pica::Shader {

/**
 * WARNING!
 *
 * The following structs are saved to the disk as cache entries!
 * Any modification to their members will invalidate the cache, breaking their
 * transferable properties.
 *
 * Only modify the entries if such modifications are justified.
 * If the struct is modified in a way that results in the exact same layout
 * (for example, replacing an u8 with another u8 in the same place), then bump
 * the struct's STRUCT_VERSION value.
 */

struct BlendConfig {
    Pica::FramebufferRegs::BlendEquation eq;
    Pica::FramebufferRegs::BlendFactor src_factor;
    Pica::FramebufferRegs::BlendFactor dst_factor;

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = BlendConfig;
        return Common::HashCombine(STRUCT_VERSION,

                                   // layout
                                   LAYOUT_HASH,

                                   // fields
                                   FIELD_HASH(eq), FIELD_HASH(src_factor), FIELD_HASH(dst_factor));
    }

    void SetMinMaxEmulationDisabled() {
        // If we don't need min/max emulation, set the blend equation
        // to "-1" as a clear marker that this config is disabled.
        eq = static_cast<Pica::FramebufferRegs::BlendEquation>(UINT32_MAX);
    }

    bool RequiresMinMaxEmulation() {
        return eq == Pica::FramebufferRegs::BlendEquation::Min ||
               eq == Pica::FramebufferRegs::BlendEquation::Max;
    }
};
static_assert(std::has_unique_object_representations_v<BlendConfig>);

struct FramebufferConfig {
    explicit FramebufferConfig(const Pica::RegsInternal& regs);

    union {
        u32 raw{};
        BitField<0, 3, Pica::FramebufferRegs::CompareFunc> alpha_test_func;
        BitField<3, 2, Pica::RasterizerRegs::ScissorMode> scissor_test_mode;
        BitField<5, 1, Pica::RasterizerRegs::DepthBuffering> depthmap_enable;
        BitField<6, 4, Pica::FramebufferRegs::LogicOp> logic_op;
        BitField<10, 1, u32> shadow_rendering;
        BitField<11, 1, u32> alphablend_enable;
        BitField<12, 1, u32> early_depth_test_enable;
    };
    BlendConfig requested_rgb_blend{};
    BlendConfig requested_alpha_blend{};

    Pica::FramebufferRegs::LogicOp requested_logic_op{};

    void ApplyProfile(const Profile& profile);

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 1;

        using T = FramebufferConfig;
        return Common::HashCombine(
            STRUCT_VERSION,

            // layout
            LAYOUT_HASH,

            // fields
            FIELD_HASH(alpha_test_func), FIELD_HASH(scissor_test_mode), FIELD_HASH(depthmap_enable),
            FIELD_HASH(logic_op), FIELD_HASH(shadow_rendering), FIELD_HASH(alphablend_enable),
            FIELD_HASH(early_depth_test_enable),
            FIELD_HASH(requested_rgb_blend), FIELD_HASH(requested_alpha_blend),
            FIELD_HASH(requested_logic_op),

            // nested layout
            BlendConfig::StructHash());
    }
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

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = TevStageConfigRaw;
        return Common::HashCombine(STRUCT_VERSION,

                                   // layout
                                   LAYOUT_HASH,

                                   // fields
                                   FIELD_HASH(sources_raw), FIELD_HASH(modifiers_raw),
                                   FIELD_HASH(ops_raw), FIELD_HASH(scales_raw));
    }
};
static_assert(std::has_unique_object_representations_v<TevStageConfigRaw>);

union TextureBorder {
    u32 raw{};
    BitField<0, 1, u32> enable_s;
    BitField<1, 1, u32> enable_t;

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = TextureBorder;
        return Common::HashCombine(STRUCT_VERSION,

                                   // layout
                                   LAYOUT_HASH,

                                   // fields
                                   FIELD_HASH(enable_s), FIELD_HASH(enable_t),

                                   // nested layout
                                   BlendConfig::StructHash());
    }
};
static_assert(std::has_unique_object_representations_v<TextureBorder>);

struct TextureConfig {
    explicit TextureConfig(const Pica::TexturingRegs& regs);

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

    struct TextureWrap {
        Pica::TexturingRegs::TextureConfig::WrapMode s;
        Pica::TexturingRegs::TextureConfig::WrapMode t;

        static consteval u64 StructHash() {
            constexpr u64 STRUCT_VERSION = 0;

            using T = TextureWrap;
            return Common::HashCombine(STRUCT_VERSION,

                                       // layout
                                       LAYOUT_HASH,

                                       // fields
                                       FIELD_HASH(s), FIELD_HASH(t));
        }
    };
    std::array<TextureWrap, 3> requested_wrap{};

    void ApplyProfile(const Profile& profile);

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = TextureConfig;
        return Common::HashCombine(STRUCT_VERSION,

                                   // layout
                                   LAYOUT_HASH,

                                   // fields
                                   FIELD_HASH(texture0_type), FIELD_HASH(texture2_use_coord1),
                                   FIELD_HASH(combiner_buffer_input), FIELD_HASH(fog_mode),
                                   FIELD_HASH(fog_flip), FIELD_HASH(shadow_texture_orthographic),
                                   FIELD_HASH(texture_border_color), FIELD_HASH(tev_stages),
                                   FIELD_HASH(requested_wrap),

                                   // nested layout
                                   TextureBorder::StructHash(), TevStageConfigRaw::StructHash(),
                                   TextureWrap::StructHash());
    }
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

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = Light;
        return Common::HashCombine(STRUCT_VERSION,

                                   // layout
                                   LAYOUT_HASH,

                                   // fields
                                   FIELD_HASH(num), FIELD_HASH(directional),
                                   FIELD_HASH(two_sided_diffuse), FIELD_HASH(dist_atten_enable),
                                   FIELD_HASH(spot_atten_enable), FIELD_HASH(geometric_factor_0),
                                   FIELD_HASH(geometric_factor_1), FIELD_HASH(shadow_enable));
    }
};
static_assert(std::has_unique_object_representations_v<Light>);

struct LutConfig {
    union {
        u32 raw;
        BitField<0, 1, u32> enable;
        BitField<1, 1, u32> abs_input;
        BitField<2, 3, Pica::LightingRegs::LightingLutInput> type;
    };

    // Needed for std::has_unique_object_representations_v
    u32 scale_bits;
    inline f32 GetScale() const noexcept {
        return std::bit_cast<f32>(scale_bits);
    }
    inline void SetScale(f32 value) noexcept {
        scale_bits = std::bit_cast<u32>(value);
    }

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = LutConfig;
        return Common::HashCombine(STRUCT_VERSION,

                                   // layout
                                   LAYOUT_HASH,

                                   // fields
                                   FIELD_HASH(enable), FIELD_HASH(abs_input), FIELD_HASH(type),
                                   FIELD_HASH(scale_bits));
    }
};
static_assert(std::has_unique_object_representations_v<LutConfig>);

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

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = LightConfig;
        return Common::HashCombine(
            STRUCT_VERSION,

            // layout
            LAYOUT_HASH,

            // fields
            FIELD_HASH(enable), FIELD_HASH(src_num), FIELD_HASH(bump_mode),
            FIELD_HASH(bump_selector), FIELD_HASH(bump_renorm), FIELD_HASH(clamp_highlights),
            FIELD_HASH(config), FIELD_HASH(enable_primary_alpha),
            FIELD_HASH(enable_secondary_alpha), FIELD_HASH(enable_shadow),
            FIELD_HASH(shadow_primary), FIELD_HASH(shadow_secondary), FIELD_HASH(shadow_invert),
            FIELD_HASH(shadow_alpha), FIELD_HASH(shadow_selector), FIELD_HASH(lut_d0),
            FIELD_HASH(lut_d1), FIELD_HASH(lut_sp), FIELD_HASH(lut_fr), FIELD_HASH(lut_rr),
            FIELD_HASH(lut_rg), FIELD_HASH(lut_rb), FIELD_HASH(lights),

            // nested layout
            LutConfig::StructHash(), Light::StructHash());
    }
};
static_assert(std::has_unique_object_representations_v<LightConfig>);

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

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = ProcTexConfig;
        return Common::HashCombine(
            STRUCT_VERSION,

            // layout
            LAYOUT_HASH,

            // fields
            FIELD_HASH(enable), FIELD_HASH(coord), FIELD_HASH(u_clamp), FIELD_HASH(v_clamp),
            FIELD_HASH(color_combiner), FIELD_HASH(alpha_combiner), FIELD_HASH(lut_filter),
            FIELD_HASH(separate_alpha), FIELD_HASH(noise_enable), FIELD_HASH(u_shift),
            FIELD_HASH(v_shift), FIELD_HASH(lut_width), FIELD_HASH(lut_offset0),
            FIELD_HASH(lut_offset1), FIELD_HASH(lut_offset2), FIELD_HASH(lut_offset3),
            FIELD_HASH(lod_min), FIELD_HASH(lod_max));
    }
};
static_assert(std::has_unique_object_representations_v<ProcTexConfig>);

union UserConfig {
    u32 raw{};
    BitField<0, 1, u32> use_custom_normal;

    // Whether a FSConfig + UserConfig combination can be
    // cached to disk. Right now, this is true if the
    // UserConfig was default constructed
    bool IsCacheable() const {
        return raw == u32{};
    }
};
static_assert(std::has_unique_object_representations_v<UserConfig>);

struct FSConfig {
    explicit FSConfig(const Pica::RegsInternal& regs);

    [[nodiscard]] bool TevStageUpdatesCombinerBufferColor(u32 stage_index) const {
        return (stage_index < 4) && (texture.combiner_buffer_input & (1 << stage_index));
    }

    [[nodiscard]] bool TevStageUpdatesCombinerBufferAlpha(u32 stage_index) const {
        return (stage_index < 4) && ((texture.combiner_buffer_input >> 4) & (1 << stage_index));
    }

    [[nodiscard]] bool UsesSpirvIncompatibleConfig() const {
        const auto texture0_type = texture.texture0_type.Value();
        return texture0_type == Pica::TexturingRegs::TextureConfig::ShadowCube ||
               framebuffer.shadow_rendering.Value();
    }

    void ApplyProfile(const Profile& profile) {
        framebuffer.ApplyProfile(profile);
        texture.ApplyProfile(profile);
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

    static consteval u64 StructHash() {
        constexpr u64 STRUCT_VERSION = 0;

        using T = FSConfig;
        return Common::HashCombine(STRUCT_VERSION,

                                   // layout
                                   LAYOUT_HASH,

                                   // fields
                                   FIELD_HASH(framebuffer), FIELD_HASH(texture),
                                   FIELD_HASH(lighting), FIELD_HASH(proctex),

                                   // nested layout
                                   FramebufferConfig::StructHash(), TextureConfig::StructHash(),
                                   LightConfig::StructHash(), ProcTexConfig::StructHash());
    }
};
static_assert(std::has_unique_object_representations_v<FSConfig>);
static_assert(std::is_trivially_copyable_v<FSConfig>);

} // namespace Pica::Shader

namespace std {
template <>
struct hash<Pica::Shader::FSConfig> {
    std::size_t operator()(const Pica::Shader::FSConfig& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std

#undef FIELD_HASH
#undef LAYOUT_HASH
