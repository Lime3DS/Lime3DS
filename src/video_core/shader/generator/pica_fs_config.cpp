// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/shader/generator/pica_fs_config.h"

namespace Pica::Shader {

FramebufferConfig::FramebufferConfig(const Pica::RegsInternal& regs, const Profile& profile) {
    const auto& output_merger = regs.framebuffer.output_merger;
    scissor_test_mode.Assign(regs.rasterizer.scissor_test.mode);
    depthmap_enable.Assign(regs.rasterizer.depthmap_enable);
    shadow_rendering.Assign(regs.framebuffer.IsShadowRendering());
    alpha_test_func.Assign(output_merger.alpha_test.enable
                               ? output_merger.alpha_test.func.Value()
                               : Pica::FramebufferRegs::CompareFunc::Always);

    // Emulate logic op in the shader if needed and not supported.
    logic_op.Assign(Pica::FramebufferRegs::LogicOp::Copy);
    if (!profile.has_logic_op && !regs.framebuffer.output_merger.alphablend_enable) {
        logic_op.Assign(regs.framebuffer.output_merger.logic_op);
    }

    const auto alpha_eq = output_merger.alpha_blending.blend_equation_a.Value();
    const auto rgb_eq = output_merger.alpha_blending.blend_equation_rgb.Value();
    if (!profile.has_blend_minmax_factor && output_merger.alphablend_enable) {
        if (rgb_eq == Pica::FramebufferRegs::BlendEquation::Max ||
            rgb_eq == Pica::FramebufferRegs::BlendEquation::Min) {
            rgb_blend.eq = rgb_eq;
            rgb_blend.src_factor = output_merger.alpha_blending.factor_source_rgb;
            rgb_blend.dst_factor = output_merger.alpha_blending.factor_dest_rgb;
        }
        if (alpha_eq == Pica::FramebufferRegs::BlendEquation::Max ||
            alpha_eq == Pica::FramebufferRegs::BlendEquation::Min) {
            alpha_blend.eq = alpha_eq;
            alpha_blend.src_factor = output_merger.alpha_blending.factor_source_a;
            alpha_blend.dst_factor = output_merger.alpha_blending.factor_dest_a;
        }
    }
}

TextureConfig::TextureConfig(const Pica::TexturingRegs& regs, const Profile& profile) {
    texture0_type.Assign(regs.texture0.type);
    texture2_use_coord1.Assign(regs.main_config.texture2_use_coord1 != 0);
    combiner_buffer_input.Assign(regs.tev_combiner_buffer_input.update_mask_rgb.Value() |
                                 regs.tev_combiner_buffer_input.update_mask_a.Value() << 4);
    fog_mode.Assign(regs.fog_mode);
    fog_flip.Assign(regs.fog_flip != 0);
    shadow_texture_orthographic.Assign(regs.shadow.orthographic != 0);

    // Emulate custom border color if needed and not supported.
    const auto pica_textures = regs.GetTextures();
    for (u32 tex_index = 0; tex_index < 3; tex_index++) {
        const auto& config = pica_textures[tex_index].config;
        texture_border_color[tex_index].enable_s.Assign(
            !profile.has_custom_border_color &&
            config.wrap_s == Pica::TexturingRegs::TextureConfig::WrapMode::ClampToBorder);
        texture_border_color[tex_index].enable_t.Assign(
            !profile.has_custom_border_color &&
            config.wrap_t == Pica::TexturingRegs::TextureConfig::WrapMode::ClampToBorder);
    }

    const auto& stages = regs.GetTevStages();
    for (std::size_t i = 0; i < tev_stages.size(); i++) {
        const auto& tev_stage = stages[i];
        tev_stages[i].sources_raw = tev_stage.sources_raw;
        tev_stages[i].modifiers_raw = tev_stage.modifiers_raw;
        tev_stages[i].ops_raw = tev_stage.ops_raw;
        tev_stages[i].scales_raw = tev_stage.scales_raw;
        if (tev_stage.color_op == Pica::TexturingRegs::TevStageConfig::Operation::Dot3_RGBA) {
            tev_stages[i].sources_raw &= 0xFFF;
            tev_stages[i].modifiers_raw &= 0xFFF;
            tev_stages[i].ops_raw &= 0xF;
        }
    }
}

LightConfig::LightConfig(const Pica::LightingRegs& regs) {
    if (regs.disable) {
        return;
    }

    enable.Assign(1);
    src_num.Assign(regs.max_light_index + 1);
    config.Assign(regs.config0.config);
    enable_primary_alpha.Assign(regs.config0.enable_primary_alpha);
    enable_secondary_alpha.Assign(regs.config0.enable_secondary_alpha);
    bump_mode.Assign(regs.config0.bump_mode);
    bump_selector.Assign(regs.config0.bump_selector);
    bump_renorm.Assign(regs.config0.disable_bump_renorm == 0);
    clamp_highlights.Assign(regs.config0.clamp_highlights != 0);

    enable_shadow.Assign(regs.config0.enable_shadow != 0);
    if (enable_shadow) {
        shadow_primary.Assign(regs.config0.shadow_primary != 0);
        shadow_secondary.Assign(regs.config0.shadow_secondary != 0);
        shadow_invert.Assign(regs.config0.shadow_invert != 0);
        shadow_alpha.Assign(regs.config0.shadow_alpha != 0);
        shadow_selector.Assign(regs.config0.shadow_selector);
    }

    for (u32 light_index = 0; light_index <= regs.max_light_index; ++light_index) {
        const u32 num = regs.light_enable.GetNum(light_index);
        const auto& light = regs.light[num];
        lights[light_index].num.Assign(num);
        lights[light_index].directional.Assign(light.config.directional != 0);
        lights[light_index].two_sided_diffuse.Assign(light.config.two_sided_diffuse != 0);
        lights[light_index].geometric_factor_0.Assign(light.config.geometric_factor_0 != 0);
        lights[light_index].geometric_factor_1.Assign(light.config.geometric_factor_1 != 0);
        lights[light_index].dist_atten_enable.Assign(!regs.IsDistAttenDisabled(num));
        lights[light_index].spot_atten_enable.Assign(!regs.IsSpotAttenDisabled(num));
        lights[light_index].shadow_enable.Assign(!regs.IsShadowDisabled(num));
    }

    lut_d0.enable.Assign(regs.config1.disable_lut_d0 == 0);
    if (lut_d0.enable) {
        lut_d0.abs_input.Assign(regs.abs_lut_input.disable_d0 == 0);
        lut_d0.type.Assign(regs.lut_input.d0.Value());
        lut_d0.scale = regs.lut_scale.GetScale(regs.lut_scale.d0);
    }

    lut_d1.enable.Assign(regs.config1.disable_lut_d1 == 0);
    if (lut_d1.enable) {
        lut_d1.abs_input.Assign(regs.abs_lut_input.disable_d1 == 0);
        lut_d1.type.Assign(regs.lut_input.d1.Value());
        lut_d1.scale = regs.lut_scale.GetScale(regs.lut_scale.d1);
    }

    // This is a dummy field due to lack of the corresponding register
    lut_sp.enable.Assign(1);
    lut_sp.abs_input.Assign(regs.abs_lut_input.disable_sp == 0);
    lut_sp.type.Assign(regs.lut_input.sp.Value());
    lut_sp.scale = regs.lut_scale.GetScale(regs.lut_scale.sp);

    lut_fr.enable.Assign(regs.config1.disable_lut_fr == 0);
    if (lut_fr.enable) {
        lut_fr.abs_input.Assign(regs.abs_lut_input.disable_fr == 0);
        lut_fr.type.Assign(regs.lut_input.fr.Value());
        lut_fr.scale = regs.lut_scale.GetScale(regs.lut_scale.fr);
    }

    lut_rr.enable.Assign(regs.config1.disable_lut_rr == 0);
    if (lut_rr.enable) {
        lut_rr.abs_input.Assign(regs.abs_lut_input.disable_rr == 0);
        lut_rr.type.Assign(regs.lut_input.rr.Value());
        lut_rr.scale = regs.lut_scale.GetScale(regs.lut_scale.rr);
    }

    lut_rg.enable.Assign(regs.config1.disable_lut_rg == 0);
    if (lut_rg.enable) {
        lut_rg.abs_input.Assign(regs.abs_lut_input.disable_rg == 0);
        lut_rg.type.Assign(regs.lut_input.rg.Value());
        lut_rg.scale = regs.lut_scale.GetScale(regs.lut_scale.rg);
    }

    lut_rb.enable.Assign(regs.config1.disable_lut_rb == 0);
    if (lut_rb.enable) {
        lut_rb.abs_input.Assign(regs.abs_lut_input.disable_rb == 0);
        lut_rb.type.Assign(regs.lut_input.rb.Value());
        lut_rb.scale = regs.lut_scale.GetScale(regs.lut_scale.rb);
    }
}

ProcTexConfig::ProcTexConfig(const Pica::TexturingRegs& regs) {
    if (!regs.main_config.texture3_enable) {
        return;
    }

    enable.Assign(1);
    coord.Assign(regs.main_config.texture3_coordinates);
    u_clamp.Assign(regs.proctex.u_clamp);
    v_clamp.Assign(regs.proctex.v_clamp);
    color_combiner.Assign(regs.proctex.color_combiner);
    alpha_combiner.Assign(regs.proctex.alpha_combiner);
    separate_alpha.Assign(regs.proctex.separate_alpha);
    noise_enable.Assign(regs.proctex.noise_enable);
    u_shift.Assign(regs.proctex.u_shift);
    v_shift.Assign(regs.proctex.v_shift);
    lut_width = regs.proctex_lut.width;
    lut_offset0 = regs.proctex_lut_offset.level0;
    lut_offset1 = regs.proctex_lut_offset.level1;
    lut_offset2 = regs.proctex_lut_offset.level2;
    lut_offset3 = regs.proctex_lut_offset.level3;
    lod_min = regs.proctex_lut.lod_min;
    lod_max = regs.proctex_lut.lod_max;
    lut_filter.Assign(regs.proctex_lut.filter);
}

FSConfig::FSConfig(const Pica::RegsInternal& regs, const UserConfig& user_, const Profile& profile)
    : framebuffer{regs, profile}, texture{regs.texturing, profile}, lighting{regs.lighting},
      proctex{regs.texturing}, user{user_} {}

} // namespace Pica::Shader
