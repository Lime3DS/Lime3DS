// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/container/small_vector.hpp>
#include "video_core/shader/generator/pica_fs_config.h"
#include "video_core/shader/generator/spv_fs_shader_gen.h"

namespace Pica::Shader::Generator::SPIRV {

using Pica::FramebufferRegs;
using Pica::LightingRegs;
using Pica::RasterizerRegs;
using Pica::TexturingRegs;
using TevStageConfig = TexturingRegs::TevStageConfig;
using TextureType = TexturingRegs::TextureConfig::TextureType;

constexpr u32 SPIRV_VERSION_1_3 = 0x00010300;

FragmentModule::FragmentModule(const FSConfig& config_, const Profile& profile_)
    : Sirit::Module{SPIRV_VERSION_1_3}, config{config_}, profile{profile_},
      use_fragment_shader_barycentric{profile.has_fragment_shader_barycentric &&
                                      config.lighting.enable} {
    DefineArithmeticTypes();
    DefineUniformStructs();
    DefineInterface();
    for (u32 i = 0; i < NUM_TEX_UNITS; i++) {
        DefineTexSampler(i);
    }
    DefineEntryPoint();
}

FragmentModule::~FragmentModule() = default;

void FragmentModule::Generate() {
    AddLabel(OpLabel());

    rounded_primary_color = Byteround(OpLoad(vec_ids.Get(4), primary_color_id), 4);
    primary_fragment_color = ConstF32(0.f, 0.f, 0.f, 0.f);
    secondary_fragment_color = ConstF32(0.f, 0.f, 0.f, 0.f);

    // Do not do any sort of processing if it's obvious we're not going to pass the alpha test
    if (config.framebuffer.alpha_test_func == Pica::FramebufferRegs::CompareFunc::Never) {
        OpKill();
        OpFunctionEnd();
        return;
    }

    // Append the scissor and depth tests
    WriteDepth();
    WriteScissor();

    // Write shader bytecode to emulate all enabled PICA lights
    WriteLighting();

    combiner_buffer = ConstF32(0.f, 0.f, 0.f, 0.f);
    next_combiner_buffer = GetShaderDataMember(vec_ids.Get(4), ConstS32(26));
    combiner_output = ConstF32(0.f, 0.f, 0.f, 0.f);

    // Write shader bytecode to emulate PICA TEV stages
    for (u32 index = 0; index < config.texture.tev_stages.size(); ++index) {
        WriteTevStage(index);
    }

    WriteAlphaTestCondition(config.framebuffer.alpha_test_func);

    // Emulate the fog
    switch (config.texture.fog_mode) {
    case TexturingRegs::FogMode::Fog:
        WriteFog();
        break;
    case TexturingRegs::FogMode::Gas:
        WriteGas();
        return;
    default:
        break;
    }

    Id color{Byteround(combiner_output, 4)};
    switch (config.framebuffer.logic_op) {
    case FramebufferRegs::LogicOp::Clear:
        color = ConstF32(0.f, 0.f, 0.f, 0.f);
        break;
    case FramebufferRegs::LogicOp::Set:
        color = ConstF32(1.f, 1.f, 1.f, 1.f);
        break;
    case FramebufferRegs::LogicOp::Copy:
        // Take the color output as-is
        break;
    case FramebufferRegs::LogicOp::CopyInverted:
        // out += "color = ~color;\n";
        break;
    case FramebufferRegs::LogicOp::NoOp:
        // We need to discard the color, but not necessarily the depth. This is not possible
        // with fragment shader alone, so we emulate this behavior with the color mask.
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unhandled logic_op {:x}",
                     static_cast<u32>(config.framebuffer.logic_op.Value()));
        UNIMPLEMENTED();
    }

    // Write output color
    OpStore(color_id, color);
    OpReturn();
    OpFunctionEnd();
}

void FragmentModule::WriteDepth() {
    const Id input_pointer_id{TypePointer(spv::StorageClass::Input, f32_id)};
    const Id gl_frag_coord_z{
        OpLoad(f32_id, OpAccessChain(input_pointer_id, gl_frag_coord_id, ConstU32(2u)))};
    const Id z_over_w{OpFNegate(f32_id, gl_frag_coord_z)};
    const Id depth_scale{GetShaderDataMember(f32_id, ConstS32(2))};
    const Id depth_offset{GetShaderDataMember(f32_id, ConstS32(3))};
    depth = OpFma(f32_id, z_over_w, depth_scale, depth_offset);
    if (config.framebuffer.depthmap_enable == Pica::RasterizerRegs::DepthBuffering::WBuffering) {
        const Id gl_frag_coord_w{
            OpLoad(f32_id, OpAccessChain(input_pointer_id, gl_frag_coord_id, ConstU32(3u)))};
        depth = OpFDiv(f32_id, depth, gl_frag_coord_w);
    }
    OpStore(gl_frag_depth_id, depth);
}

void FragmentModule::WriteScissor() {
    if (config.framebuffer.scissor_test_mode == RasterizerRegs::ScissorMode::Disabled) {
        return;
    }

    const Id gl_frag_coord{OpLoad(vec_ids.Get(4), gl_frag_coord_id)};
    const Id gl_frag_coord_xy{OpVectorShuffle(vec_ids.Get(2), gl_frag_coord, gl_frag_coord, 0, 1)};

    const Id scissor_x1{GetShaderDataMember(i32_id, ConstS32(6))};
    const Id scissor_y1{GetShaderDataMember(i32_id, ConstS32(7))};
    const Id scissor_1{OpCompositeConstruct(vec_ids.Get(2), OpConvertSToF(f32_id, scissor_x1),
                                            OpConvertSToF(f32_id, scissor_y1))};

    const Id scissor_x2{GetShaderDataMember(i32_id, ConstS32(8))};
    const Id scissor_y2{GetShaderDataMember(i32_id, ConstS32(9))};
    const Id scissor_2{OpCompositeConstruct(vec_ids.Get(2), OpConvertSToF(f32_id, scissor_x2),
                                            OpConvertSToF(f32_id, scissor_y2))};

    const Id cond1{OpFOrdGreaterThanEqual(bvec_ids.Get(2), gl_frag_coord_xy, scissor_1)};
    const Id cond2{OpFOrdLessThan(bvec_ids.Get(2), gl_frag_coord_xy, scissor_2)};

    Id result{OpAll(bool_id, OpCompositeConstruct(bvec_ids.Get(4), cond1, cond2))};
    if (config.framebuffer.scissor_test_mode == RasterizerRegs::ScissorMode::Include) {
        result = OpLogicalNot(bool_id, result);
    }

    const Id merge_block{OpLabel()};
    const Id kill_label{OpLabel()};
    OpSelectionMerge(merge_block, spv::SelectionControlMask::MaskNone);
    OpBranchConditional(result, kill_label, merge_block);

    AddLabel(kill_label);
    OpKill();

    AddLabel(merge_block);
}

void FragmentModule::WriteFog() {
    // Get index into fog LUT
    Id fog_index{};
    if (config.texture.fog_flip) {
        fog_index = OpFMul(f32_id, OpFSub(f32_id, ConstF32(1.f), depth), ConstF32(128.f));
    } else {
        fog_index = OpFMul(f32_id, depth, ConstF32(128.f));
    }

    // Generate clamped fog factor from LUT for given fog index
    const Id fog_i{OpFClamp(f32_id, OpFloor(f32_id, fog_index), ConstF32(0.f), ConstF32(127.f))};
    const Id fog_f{OpFSub(f32_id, fog_index, fog_i)};
    const Id fog_lut_offset{GetShaderDataMember(i32_id, ConstS32(10))};
    const Id coord{OpIAdd(i32_id, OpConvertFToS(i32_id, fog_i), fog_lut_offset)};
    if (!Sirit::ValidId(texture_buffer_lut_lf)) {
        texture_buffer_lut_lf = OpLoad(image_buffer_id, texture_buffer_lut_lf_id);
    }
    const Id fog_lut_entry_rgba{OpImageFetch(vec_ids.Get(4), texture_buffer_lut_lf, coord)};
    const Id fog_lut_r{OpCompositeExtract(f32_id, fog_lut_entry_rgba, 0)};
    const Id fog_lut_g{OpCompositeExtract(f32_id, fog_lut_entry_rgba, 1)};
    Id fog_factor{OpFma(f32_id, fog_f, fog_lut_g, fog_lut_r)};
    fog_factor = OpFClamp(f32_id, fog_factor, ConstF32(0.f), ConstF32(1.f));

    // Blend the fog
    const Id tex_env_rgb{
        OpVectorShuffle(vec_ids.Get(3), combiner_output, combiner_output, 0, 1, 2)};
    const Id fog_color{GetShaderDataMember(vec_ids.Get(3), ConstS32(19))};
    const Id fog_factor_rgb{
        OpCompositeConstruct(vec_ids.Get(3), fog_factor, fog_factor, fog_factor)};
    const Id fog_result{OpFMix(vec_ids.Get(3), fog_color, tex_env_rgb, fog_factor_rgb)};
    combiner_output = OpVectorShuffle(vec_ids.Get(4), fog_result, combiner_output, 0, 1, 2, 6);
}

void FragmentModule::WriteGas() {
    // TODO: Implement me
    LOG_CRITICAL(Render, "Unimplemented gas mode");
    OpKill();
    OpFunctionEnd();
}

void FragmentModule::WriteLighting() {
    if (!config.lighting.enable) {
        return;
    }

    const auto& lighting = config.lighting;

    // Define lighting globals
    Id diffuse_sum{ConstF32(0.f, 0.f, 0.f, 1.f)};
    Id specular_sum{ConstF32(0.f, 0.f, 0.f, 1.f)};
    Id light_vector{ConstF32(0.f, 0.f, 0.f)};
    Id light_distance{ConstF32(0.f)};
    Id spot_dir{ConstF32(0.f, 0.f, 0.f)};
    Id half_vector{ConstF32(0.f, 0.f, 0.f)};
    Id dot_product{ConstF32(0.f)};
    Id clamp_highlights{ConstF32(1.f)};
    Id geo_factor{ConstF32(1.f)};
    Id surface_normal{};
    Id surface_tangent{};

    // Compute fragment normals and tangents
    const auto perturbation = [&]() -> Id {
        const Id texel{
            OpFunctionCall(vec_ids.Get(4), sample_tex_unit_func[lighting.bump_selector])};
        const Id texel_rgb{OpVectorShuffle(vec_ids.Get(3), texel, texel, 0, 1, 2)};
        const Id rgb_mul_two{OpVectorTimesScalar(vec_ids.Get(3), texel_rgb, ConstF32(2.f))};
        return OpFSub(vec_ids.Get(3), rgb_mul_two, ConstF32(1.f, 1.f, 1.f));
    };

    if (lighting.bump_mode == LightingRegs::LightingBumpMode::NormalMap) {
        // Bump mapping is enabled using a normal map
        surface_normal = perturbation();

        // Recompute Z-component of perturbation if 'renorm' is enabled, this provides a higher
        // precision result
        if (lighting.bump_renorm) {
            const Id normal_x{OpCompositeExtract(f32_id, surface_normal, 0)};
            const Id normal_y{OpCompositeExtract(f32_id, surface_normal, 1)};
            const Id y_mul_y{OpFMul(f32_id, normal_y, normal_y)};
            const Id val{OpFSub(f32_id, ConstF32(1.f), OpFma(f32_id, normal_x, normal_x, y_mul_y))};
            const Id normal_z{OpSqrt(f32_id, OpFMax(f32_id, val, ConstF32(0.f)))};
            surface_normal = OpCompositeConstruct(vec_ids.Get(3), normal_x, normal_y, normal_z);
        }

        // The tangent vector is not perturbed by the normal map and is just a unit vector.
        surface_tangent = ConstF32(1.f, 0.f, 0.f);
    } else if (lighting.bump_mode == LightingRegs::LightingBumpMode::TangentMap) {
        // Bump mapping is enabled using a tangent map
        surface_tangent = perturbation();

        // Mathematically, recomputing Z-component of the tangent vector won't affect the relevant
        // computation below, which is also confirmed on 3DS. So we don't bother recomputing here
        // even if 'renorm' is enabled.

        // The normal vector is not perturbed by the tangent map and is just a unit vector.
        surface_normal = ConstF32(0.f, 0.f, 1.f);
    } else {
        // No bump mapping - surface local normal and tangent are just unit vectors
        surface_normal = ConstF32(0.f, 0.f, 1.f);
        surface_tangent = ConstF32(1.f, 0.f, 0.f);
    }

    // Rotate the vector v by the quaternion q
    const auto quaternion_rotate = [this](Id q, Id v) -> Id {
        const Id q_xyz{OpVectorShuffle(vec_ids.Get(3), q, q, 0, 1, 2)};
        const Id q_xyz_cross_v{OpCross(vec_ids.Get(3), q_xyz, v)};
        const Id q_w{OpCompositeExtract(f32_id, q, 3)};
        const Id val1{
            OpFAdd(vec_ids.Get(3), q_xyz_cross_v, OpVectorTimesScalar(vec_ids.Get(3), v, q_w))};
        const Id val2{OpVectorTimesScalar(vec_ids.Get(3), OpCross(vec_ids.Get(3), q_xyz, val1),
                                          ConstF32(2.f))};
        return OpFAdd(vec_ids.Get(3), v, val2);
    };

    // Perform quaternion correction in the fragment shader if fragment_shader_barycentric is
    // supported.
    Id normquat{};
    if (use_fragment_shader_barycentric) {
        const auto are_quaternions_opposite = [&](Id qa, Id qb) {
            const Id dot_q{OpDot(f32_id, qa, qb)};
            const Id dot_le_zero{OpFOrdLessThan(bool_id, dot_q, ConstF32(0.f))};
            return OpCompositeConstruct(bvec_ids.Get(4), dot_le_zero, dot_le_zero, dot_le_zero,
                                        dot_le_zero);
        };

        const Id input_pointer_id{TypePointer(spv::StorageClass::Input, vec_ids.Get(4))};
        const Id normquat_0{
            OpLoad(vec_ids.Get(4), OpAccessChain(input_pointer_id, normquat_id, ConstS32(0)))};
        const Id normquat_1{
            OpLoad(vec_ids.Get(4), OpAccessChain(input_pointer_id, normquat_id, ConstS32(1)))};
        const Id normquat_1_correct{OpSelect(vec_ids.Get(4),
                                             are_quaternions_opposite(normquat_0, normquat_1),
                                             OpFNegate(vec_ids.Get(4), normquat_1), normquat_1)};
        const Id normquat_2{
            OpLoad(vec_ids.Get(4), OpAccessChain(input_pointer_id, normquat_id, ConstS32(2)))};
        const Id normquat_2_correct{OpSelect(vec_ids.Get(4),
                                             are_quaternions_opposite(normquat_0, normquat_2),
                                             OpFNegate(vec_ids.Get(4), normquat_2), normquat_2)};
        const Id bary_coord{OpLoad(vec_ids.Get(3), gl_bary_coord_id)};
        const Id bary_coord_x{OpCompositeExtract(f32_id, bary_coord, 0)};
        const Id bary_coord_y{OpCompositeExtract(f32_id, bary_coord, 1)};
        const Id bary_coord_z{OpCompositeExtract(f32_id, bary_coord, 2)};
        const Id normquat_0_final{OpVectorTimesScalar(vec_ids.Get(4), normquat_0, bary_coord_x)};
        const Id normquat_1_final{
            OpVectorTimesScalar(vec_ids.Get(4), normquat_1_correct, bary_coord_y)};
        const Id normquat_2_final{
            OpVectorTimesScalar(vec_ids.Get(4), normquat_2_correct, bary_coord_z)};
        normquat = OpFAdd(vec_ids.Get(4), normquat_0_final,
                          OpFAdd(vec_ids.Get(4), normquat_1_final, normquat_2_final));
    } else {
        normquat = OpLoad(vec_ids.Get(4), normquat_id);
    }

    // Rotate the surface-local normal by the interpolated normal quaternion to convert it to
    // eyespace.
    const Id normalized_normquat{OpNormalize(vec_ids.Get(4), normquat)};
    const Id normal{quaternion_rotate(normalized_normquat, surface_normal)};
    const Id tangent{quaternion_rotate(normalized_normquat, surface_tangent)};

    Id shadow{ConstF32(1.f, 1.f, 1.f, 1.f)};
    if (lighting.enable_shadow) {
        shadow = OpFunctionCall(vec_ids.Get(4), sample_tex_unit_func[lighting.shadow_selector]);
        if (lighting.shadow_invert) {
            shadow = OpFSub(vec_ids.Get(4), ConstF32(1.f, 1.f, 1.f, 1.f), shadow);
        }
    }

    const auto lookup_lighting_lut_unsigned = [this](Id lut_index, Id pos) -> Id {
        const Id pos_floor{OpFloor(f32_id, OpFMul(f32_id, pos, ConstF32(256.f)))};
        const Id index_float{OpFClamp(f32_id, pos_floor, ConstF32(0.f), ConstF32(255.f))};
        const Id index{OpConvertFToS(i32_id, index_float)};
        const Id neg_index{OpFNegate(f32_id, OpConvertSToF(f32_id, index))};
        const Id delta{OpFma(f32_id, pos, ConstF32(256.f), neg_index)};
        return LookupLightingLUT(lut_index, index, delta);
    };

    const auto lookup_lighting_lut_signed = [this](Id lut_index, Id pos) -> Id {
        const Id pos_floor{OpFloor(f32_id, OpFMul(f32_id, pos, ConstF32(128.f)))};
        const Id index_float{OpFClamp(f32_id, pos_floor, ConstF32(-128.f), ConstF32(127.f))};
        const Id index{OpConvertFToS(i32_id, index_float)};
        const Id neg_index{OpFNegate(f32_id, OpConvertSToF(f32_id, index))};
        const Id delta{OpFma(f32_id, pos, ConstF32(128.f), neg_index)};
        const Id increment{
            OpSelect(i32_id, OpSLessThan(bool_id, index, ConstS32(0)), ConstS32(256), ConstS32(0))};
        return LookupLightingLUT(lut_index, OpIAdd(i32_id, index, increment), delta);
    };

    // Samples the specified lookup table for specular lighting
    const Id view{OpLoad(vec_ids.Get(3), view_id)};
    const auto get_lut_value = [&](LightingRegs::LightingSampler sampler, u32 light_num,
                                   LightingRegs::LightingLutInput input, bool abs) -> Id {
        Id index{};
        switch (input) {
        case LightingRegs::LightingLutInput::NH:
            index = OpDot(f32_id, normal, OpNormalize(vec_ids.Get(3), half_vector));
            break;
        case LightingRegs::LightingLutInput::VH:
            index = OpDot(f32_id, OpNormalize(vec_ids.Get(3), view),
                          OpNormalize(vec_ids.Get(3), half_vector));
            break;
        case LightingRegs::LightingLutInput::NV:
            index = OpDot(f32_id, normal, OpNormalize(vec_ids.Get(3), view));
            break;
        case LightingRegs::LightingLutInput::LN:
            index = OpDot(f32_id, light_vector, normal);
            break;
        case LightingRegs::LightingLutInput::SP:
            index = OpDot(f32_id, light_vector, spot_dir);
            break;
        case LightingRegs::LightingLutInput::CP:
            // CP input is only available with configuration 7
            if (lighting.config == LightingRegs::LightingConfig::Config7) {
                // Note: even if the normal vector is modified by normal map, which is not the
                // normal of the tangent plane anymore, the half angle vector is still projected
                // using the modified normal vector.
                const Id normalized_half_vector{OpNormalize(vec_ids.Get(3), half_vector)};
                const Id normal_dot_half_vector{OpDot(f32_id, normal, normalized_half_vector)};
                const Id normal_mul_dot{
                    OpVectorTimesScalar(vec_ids.Get(3), normal, normal_dot_half_vector)};
                const Id half_angle_proj{
                    OpFSub(vec_ids.Get(3), normalized_half_vector, normal_mul_dot)};

                // Note: the half angle vector projection is confirmed not normalized before the dot
                // product. The result is in fact not cos(phi) as the name suggested.
                index = OpDot(f32_id, half_angle_proj, tangent);
            } else {
                index = ConstF32(0.f);
            }
            break;
        default:
            LOG_CRITICAL(HW_GPU, "Unknown lighting LUT input {}", (int)input);
            UNIMPLEMENTED();
            index = ConstF32(0.f);
            break;
        }

        const Id sampler_index{ConstU32(static_cast<u32>(sampler))};
        if (abs) {
            // LUT index is in the range of (0.0, 1.0)
            index = lighting.lights[light_num].two_sided_diffuse
                        ? OpFAbs(f32_id, index)
                        : OpFMax(f32_id, index, ConstF32(0.f));
            return lookup_lighting_lut_unsigned(sampler_index, index);
        } else {
            // LUT index is in the range of (-1.0, 1.0)
            return lookup_lighting_lut_signed(sampler_index, index);
        }
    };

    // Write the code to emulate each enabled light
    for (u32 light_index = 0; light_index < lighting.src_num; ++light_index) {
        const auto& light_config = lighting.lights[light_index];

        const auto GetLightMember = [&](s32 member) -> Id {
            const Id member_type = member < 6 ? vec_ids.Get(3) : f32_id;
            const Id light_num{
                ConstS32(static_cast<s32>(lighting.lights[light_index].num.Value()))};
            return GetShaderDataMember(member_type, ConstS32(24), light_num, ConstS32(member));
        };

        // Compute light vector (directional or positional)
        const Id light_position{GetLightMember(4)};
        if (light_config.directional) {
            light_vector = light_position;
        } else {
            light_vector = OpFAdd(vec_ids.Get(3), light_position, view);
        }

        light_distance = OpLength(f32_id, light_vector);
        light_vector = OpNormalize(vec_ids.Get(3), light_vector);

        spot_dir = GetLightMember(5);
        half_vector = OpFAdd(vec_ids.Get(3), OpNormalize(vec_ids.Get(3), view), light_vector);

        // Compute dot product of light_vector and normal, adjust if lighting is one-sided or
        // two-sided
        if (light_config.two_sided_diffuse) {
            dot_product = OpFAbs(f32_id, OpDot(f32_id, light_vector, normal));
        } else {
            dot_product = OpFMax(f32_id, OpDot(f32_id, light_vector, normal), ConstF32(0.f));
        }

        // If enabled, clamp specular component if lighting result is zero
        if (lighting.clamp_highlights) {
            clamp_highlights = OpFSign(f32_id, dot_product);
        }

        // If enabled, compute spot light attenuation value
        Id spot_atten{ConstF32(1.f)};
        if (light_config.spot_atten_enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::SpotlightAttenuation)) {
            const Id value{
                get_lut_value(LightingRegs::SpotlightAttenuationSampler(light_config.num),
                              light_config.num, lighting.lut_sp.type, lighting.lut_sp.abs_input)};
            spot_atten = OpFMul(f32_id, ConstF32(lighting.lut_sp.scale), value);
        }

        // If enabled, compute distance attenuation value
        Id dist_atten{ConstF32(1.f)};
        if (light_config.dist_atten_enable) {
            const Id dist_atten_scale{GetLightMember(7)};
            const Id dist_atten_bias{GetLightMember(6)};
            const Id index{OpFma(f32_id, dist_atten_scale, light_distance, dist_atten_bias)};
            const Id clamped_index{OpFClamp(f32_id, index, ConstF32(0.f), ConstF32(1.f))};
            const Id sampler{ConstS32(
                static_cast<s32>(LightingRegs::DistanceAttenuationSampler(light_config.num)))};
            dist_atten = lookup_lighting_lut_unsigned(sampler, clamped_index);
        }

        if (light_config.geometric_factor_0 || light_config.geometric_factor_1) {
            geo_factor = OpDot(f32_id, half_vector, half_vector);
            const Id dot_div_geo{
                OpFMin(f32_id, OpFDiv(f32_id, dot_product, geo_factor), ConstF32(1.f))};
            const Id is_geo_factor_zero{OpFOrdEqual(bool_id, geo_factor, ConstF32(0.f))};
            geo_factor = OpSelect(f32_id, is_geo_factor_zero, ConstF32(0.f), dot_div_geo);
        }

        // Specular 0 component
        Id d0_lut_value{ConstF32(1.f)};
        if (lighting.lut_d0.enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::Distribution0)) {
            // Lookup specular "distribution 0" LUT value
            const Id value{get_lut_value(LightingRegs::LightingSampler::Distribution0,
                                         light_config.num, lighting.lut_d0.type,
                                         lighting.lut_d0.abs_input)};
            d0_lut_value = OpFMul(f32_id, ConstF32(lighting.lut_d0.scale), value);
        }

        Id specular_0{OpVectorTimesScalar(vec_ids.Get(3), GetLightMember(0), d0_lut_value)};
        if (light_config.geometric_factor_0) {
            specular_0 = OpVectorTimesScalar(vec_ids.Get(3), specular_0, geo_factor);
        }

        // If enabled, lookup ReflectRed value, otherwise, 1.0 is used
        Id refl_value_r{ConstF32(1.f)};
        if (lighting.lut_rr.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectRed)) {
            const Id value{get_lut_value(LightingRegs::LightingSampler::ReflectRed,
                                         light_config.num, lighting.lut_rr.type,
                                         lighting.lut_rr.abs_input)};

            refl_value_r = OpFMul(f32_id, ConstF32(lighting.lut_rr.scale), value);
        }

        // If enabled, lookup ReflectGreen value, otherwise, ReflectRed value is used
        Id refl_value_g{refl_value_r};
        if (lighting.lut_rg.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectGreen)) {
            const Id value{get_lut_value(LightingRegs::LightingSampler::ReflectGreen,
                                         light_config.num, lighting.lut_rg.type,
                                         lighting.lut_rg.abs_input)};

            refl_value_g = OpFMul(f32_id, ConstF32(lighting.lut_rg.scale), value);
        }

        // If enabled, lookup ReflectBlue value, otherwise, ReflectRed value is used
        Id refl_value_b{refl_value_r};
        if (lighting.lut_rb.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectBlue)) {
            const Id value{get_lut_value(LightingRegs::LightingSampler::ReflectBlue,
                                         light_config.num, lighting.lut_rb.type,
                                         lighting.lut_rb.abs_input)};
            refl_value_b = OpFMul(f32_id, ConstF32(lighting.lut_rb.scale), value);
        }

        // Specular 1 component
        Id d1_lut_value{ConstF32(1.f)};
        if (lighting.lut_d1.enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::Distribution1)) {
            // Lookup specular "distribution 1" LUT value
            const Id value{get_lut_value(LightingRegs::LightingSampler::Distribution1,
                                         light_config.num, lighting.lut_d1.type,
                                         lighting.lut_d1.abs_input)};
            d1_lut_value = OpFMul(f32_id, ConstF32(lighting.lut_d1.scale), value);
        }

        const Id refl_value{
            OpCompositeConstruct(vec_ids.Get(3), refl_value_r, refl_value_g, refl_value_b)};
        const Id light_specular_1{GetLightMember(1)};
        Id specular_1{OpFMul(vec_ids.Get(3),
                             OpVectorTimesScalar(vec_ids.Get(3), refl_value, d1_lut_value),
                             light_specular_1)};
        if (light_config.geometric_factor_1) {
            specular_1 = OpVectorTimesScalar(vec_ids.Get(3), specular_1, geo_factor);
        }

        // Fresnel
        // Note: only the last entry in the light slots applies the Fresnel factor
        if (light_index == lighting.src_num - 1 && lighting.lut_fr.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::Fresnel)) {
            // Lookup fresnel LUT value
            Id value{get_lut_value(LightingRegs::LightingSampler::Fresnel, light_config.num,
                                   lighting.lut_fr.type, lighting.lut_fr.abs_input)};
            value = OpFMul(f32_id, ConstF32(lighting.lut_fr.scale), value);

            // Enabled for diffuse lighting alpha component
            if (lighting.enable_primary_alpha) {
                diffuse_sum = OpCompositeInsert(vec_ids.Get(4), value, diffuse_sum, 3);
            }

            // Enabled for the specular lighting alpha component
            if (lighting.enable_secondary_alpha) {
                specular_sum = OpCompositeInsert(vec_ids.Get(4), value, specular_sum, 3);
            }
        }

        const bool shadow_primary_enable = lighting.shadow_primary && light_config.shadow_enable;
        const bool shadow_secondary_enable =
            lighting.shadow_secondary && light_config.shadow_enable;
        const Id shadow_rgb{OpVectorShuffle(vec_ids.Get(3), shadow, shadow, 0, 1, 2)};

        const Id light_diffuse{GetLightMember(2)};
        const Id light_ambient{GetLightMember(3)};
        const Id diffuse_mul_dot{OpVectorTimesScalar(vec_ids.Get(3), light_diffuse, dot_product)};

        // Compute primary fragment color (diffuse lighting) function
        Id diffuse_sum_rgb{OpFAdd(vec_ids.Get(3), diffuse_mul_dot, light_ambient)};
        diffuse_sum_rgb = OpVectorTimesScalar(vec_ids.Get(3), diffuse_sum_rgb, dist_atten);
        diffuse_sum_rgb = OpVectorTimesScalar(vec_ids.Get(3), diffuse_sum_rgb, spot_atten);
        if (shadow_primary_enable) {
            diffuse_sum_rgb = OpFMul(vec_ids.Get(3), diffuse_sum_rgb, shadow_rgb);
        }

        // Compute secondary fragment color (specular lighting) function
        const Id specular_01{OpFAdd(vec_ids.Get(3), specular_0, specular_1)};
        Id specular_sum_rgb{OpVectorTimesScalar(vec_ids.Get(3), specular_01, clamp_highlights)};
        specular_sum_rgb = OpVectorTimesScalar(vec_ids.Get(3), specular_sum_rgb, dist_atten);
        specular_sum_rgb = OpVectorTimesScalar(vec_ids.Get(3), specular_sum_rgb, spot_atten);
        if (shadow_secondary_enable) {
            specular_sum_rgb = OpFMul(vec_ids.Get(3), specular_sum_rgb, shadow_rgb);
        }

        // Accumulate the fragment colors
        const Id diffuse_sum_rgba{PadVectorF32(diffuse_sum_rgb, vec_ids.Get(4), 0.f)};
        const Id specular_sum_rgba{PadVectorF32(specular_sum_rgb, vec_ids.Get(4), 0.f)};
        diffuse_sum = OpFAdd(vec_ids.Get(4), diffuse_sum, diffuse_sum_rgba);
        specular_sum = OpFAdd(vec_ids.Get(4), specular_sum, specular_sum_rgba);
    }

    // Apply shadow attenuation to alpha components if enabled
    if (lighting.shadow_alpha) {
        const Id shadow_a{OpCompositeExtract(f32_id, shadow, 3)};
        const Id shadow_a_vec{
            OpCompositeConstruct(vec_ids.Get(4), ConstF32(1.f, 1.f, 1.f), shadow_a)};
        if (lighting.enable_primary_alpha) {
            diffuse_sum = OpFMul(vec_ids.Get(4), diffuse_sum, shadow_a_vec);
        }
        if (lighting.enable_secondary_alpha) {
            specular_sum = OpFMul(vec_ids.Get(4), specular_sum, shadow_a_vec);
        }
    }

    // Sum final lighting result
    const Id lighting_global_ambient{GetShaderDataMember(vec_ids.Get(3), ConstS32(23))};
    const Id lighting_global_ambient_rgba{
        PadVectorF32(lighting_global_ambient, vec_ids.Get(4), 0.f)};
    const Id zero_vec{ConstF32(0.f, 0.f, 0.f, 0.f)};
    const Id one_vec{ConstF32(1.f, 1.f, 1.f, 1.f)};
    diffuse_sum = OpFAdd(vec_ids.Get(4), diffuse_sum, lighting_global_ambient_rgba);
    primary_fragment_color = OpFClamp(vec_ids.Get(4), diffuse_sum, zero_vec, one_vec);
    secondary_fragment_color = OpFClamp(vec_ids.Get(4), specular_sum, zero_vec, one_vec);
}

void FragmentModule::WriteTevStage(s32 index) {
    const TexturingRegs::TevStageConfig stage = config.texture.tev_stages[index];

    // Detects if a TEV stage is configured to be skipped (to avoid generating unnecessary code)
    const auto is_passthrough_tev_stage = [](const TevStageConfig& stage) {
        return (stage.color_op == TevStageConfig::Operation::Replace &&
                stage.alpha_op == TevStageConfig::Operation::Replace &&
                stage.color_source1 == TevStageConfig::Source::Previous &&
                stage.alpha_source1 == TevStageConfig::Source::Previous &&
                stage.color_modifier1 == TevStageConfig::ColorModifier::SourceColor &&
                stage.alpha_modifier1 == TevStageConfig::AlphaModifier::SourceAlpha &&
                stage.GetColorMultiplier() == 1 && stage.GetAlphaMultiplier() == 1);
    };

    if (!is_passthrough_tev_stage(stage)) {
        color_results_1 = AppendColorModifier(stage.color_modifier1, stage.color_source1, index);
        color_results_2 = AppendColorModifier(stage.color_modifier2, stage.color_source2, index);
        color_results_3 = AppendColorModifier(stage.color_modifier3, stage.color_source3, index);

        // Round the output of each TEV stage to maintain the PICA's 8 bits of precision
        Id color_output{Byteround(AppendColorCombiner(stage.color_op), 3)};
        Id alpha_output{};

        if (stage.color_op == TevStageConfig::Operation::Dot3_RGBA) {
            // result of Dot3_RGBA operation is also placed to the alpha component
            alpha_output = OpCompositeExtract(f32_id, color_output, 0);
        } else {
            alpha_results_1 =
                AppendAlphaModifier(stage.alpha_modifier1, stage.alpha_source1, index);
            alpha_results_2 =
                AppendAlphaModifier(stage.alpha_modifier2, stage.alpha_source2, index);
            alpha_results_3 =
                AppendAlphaModifier(stage.alpha_modifier3, stage.alpha_source3, index);

            alpha_output = Byteround(AppendAlphaCombiner(stage.alpha_op));
        }

        color_output = OpVectorTimesScalar(
            vec_ids.Get(3), color_output, ConstF32(static_cast<float>(stage.GetColorMultiplier())));
        color_output = OpFClamp(vec_ids.Get(3), color_output, ConstF32(0.f, 0.f, 0.f),
                                ConstF32(1.f, 1.f, 1.f));
        alpha_output =
            OpFMul(f32_id, alpha_output, ConstF32(static_cast<float>(stage.GetAlphaMultiplier())));
        alpha_output = OpFClamp(f32_id, alpha_output, ConstF32(0.f), ConstF32(1.f));
        combiner_output = OpCompositeConstruct(vec_ids.Get(4), color_output, alpha_output);
    }

    combiner_buffer = next_combiner_buffer;
    if (config.TevStageUpdatesCombinerBufferColor(index)) {
        next_combiner_buffer =
            OpVectorShuffle(vec_ids.Get(4), combiner_output, next_combiner_buffer, 0, 1, 2, 7);
    }

    if (config.TevStageUpdatesCombinerBufferAlpha(index)) {
        next_combiner_buffer =
            OpVectorShuffle(vec_ids.Get(4), next_combiner_buffer, combiner_output, 0, 1, 2, 7);
    }
}

using ProcTexClamp = TexturingRegs::ProcTexClamp;
using ProcTexShift = TexturingRegs::ProcTexShift;
using ProcTexCombiner = TexturingRegs::ProcTexCombiner;
using ProcTexFilter = TexturingRegs::ProcTexFilter;

void FragmentModule::WriteAlphaTestCondition(FramebufferRegs::CompareFunc func) {
    using CompareFunc = FramebufferRegs::CompareFunc;

    // The compare func is to keep the fragment so we invert it to discard it
    const auto compare = [this, func](Id alpha, Id alphatest_ref) {
        switch (func) {
        case CompareFunc::Equal:
            return OpINotEqual(bool_id, alpha, alphatest_ref);
        case CompareFunc::NotEqual:
            return OpIEqual(bool_id, alpha, alphatest_ref);
        case CompareFunc::LessThan:
            return OpSGreaterThanEqual(bool_id, alpha, alphatest_ref);
        case CompareFunc::LessThanOrEqual:
            return OpSGreaterThan(bool_id, alpha, alphatest_ref);
        case CompareFunc::GreaterThan:
            return OpSLessThanEqual(bool_id, alpha, alphatest_ref);
        case CompareFunc::GreaterThanOrEqual:
            return OpSLessThan(bool_id, alpha, alphatest_ref);
        default:
            return Id{};
        }
    };

    // Don't check for kill, this is done earlier
    switch (func) {
    case CompareFunc::Always: // Do nothing
        break;
    case CompareFunc::Equal:
    case CompareFunc::NotEqual:
    case CompareFunc::LessThan:
    case CompareFunc::LessThanOrEqual:
    case CompareFunc::GreaterThan:
    case CompareFunc::GreaterThanOrEqual: {
        const Id alpha_scaled{
            OpFMul(f32_id, OpCompositeExtract(f32_id, combiner_output, 3), ConstF32(255.f))};
        const Id alpha_int{OpConvertFToS(i32_id, alpha_scaled)};
        const Id alphatest_ref{GetShaderDataMember(i32_id, ConstS32(1))};
        const Id alpha_comp_ref{compare(alpha_int, alphatest_ref)};
        const Id kill_label{OpLabel()};
        const Id keep_label{OpLabel()};
        OpSelectionMerge(keep_label, spv::SelectionControlMask::MaskNone);
        OpBranchConditional(alpha_comp_ref, kill_label, keep_label);
        AddLabel(kill_label);
        OpKill();
        AddLabel(keep_label);
        break;
    }
    default:
        LOG_CRITICAL(Render, "Unknown alpha test condition {}", func);
        break;
    }
}

Id FragmentModule::CompareShadow(Id pixel, Id z) {
    const Id pixel_d24{OpShiftRightLogical(u32_id, pixel, ConstS32(8))};
    const Id pixel_s8{OpConvertUToF(f32_id, OpBitwiseAnd(u32_id, pixel, ConstU32(255u)))};
    const Id s8_f32{OpFMul(f32_id, pixel_s8, ConstF32(1.f / 255.f))};
    const Id d24_leq_z{OpULessThanEqual(bool_id, pixel_d24, z)};
    return OpSelect(f32_id, d24_leq_z, ConstF32(0.f), s8_f32);
}

Id FragmentModule::SampleShadow() {
    const Id texcoord0{OpLoad(vec_ids.Get(2), texcoord_id[0])};
    const Id texcoord0_w{OpLoad(f32_id, texcoord0_w_id)};
    const Id abs_min_w{OpFMul(f32_id, OpFMin(f32_id, OpFAbs(f32_id, texcoord0_w), ConstF32(1.f)),
                              ConstF32(16777215.f))};
    const Id shadow_texture_bias{GetShaderDataMember(i32_id, ConstS32(17))};
    const Id z_i32{OpSMax(i32_id, ConstS32(0),
                          OpISub(i32_id, OpConvertFToS(i32_id, abs_min_w), shadow_texture_bias))};
    const Id z{OpBitcast(u32_id, z_i32)};
    const Id shadow_texture_px{OpLoad(image_r32_id, shadow_texture_px_id)};
    const Id px_size{OpImageQuerySize(ivec_ids.Get(2), shadow_texture_px)};
    const Id coord{OpFma(vec_ids.Get(2), OpConvertSToF(vec_ids.Get(2), px_size), texcoord0,
                         ConstF32(-0.5f, -0.5f))};
    const Id coord_floor{OpFloor(vec_ids.Get(2), coord)};
    const Id f{OpFSub(vec_ids.Get(2), coord, coord_floor)};
    const Id i{OpConvertFToS(ivec_ids.Get(2), coord_floor)};

    const auto sample_shadow2D = [&](Id uv) -> Id {
        const Id true_label{OpLabel()};
        const Id false_label{OpLabel()};
        const Id end_label{OpLabel()};
        const Id uv_le_zero{OpSLessThan(bvec_ids.Get(2), uv, ConstS32(0, 0))};
        const Id uv_geq_size{OpSGreaterThanEqual(bvec_ids.Get(2), uv, px_size)};
        const Id cond{
            OpAny(bool_id, OpCompositeConstruct(bvec_ids.Get(4), uv_le_zero, uv_geq_size))};
        OpSelectionMerge(end_label, spv::SelectionControlMask::MaskNone);
        OpBranchConditional(cond, true_label, false_label);
        AddLabel(true_label);
        OpBranch(end_label);
        AddLabel(false_label);
        const Id px_texel{OpImageRead(uvec_ids.Get(4), shadow_texture_px, uv)};
        const Id px_texel_x{OpCompositeExtract(u32_id, px_texel, 0)};
        const Id result{CompareShadow(px_texel_x, z)};
        OpBranch(end_label);
        AddLabel(end_label);
        return OpPhi(f32_id, ConstF32(1.f), true_label, result, false_label);
    };

    const Id s_xy{
        OpCompositeConstruct(vec_ids.Get(2), sample_shadow2D(i),
                             sample_shadow2D(OpIAdd(ivec_ids.Get(2), i, ConstS32(1, 0))))};
    const Id s_zw{OpCompositeConstruct(
        vec_ids.Get(2), sample_shadow2D(OpIAdd(ivec_ids.Get(2), i, ConstS32(0, 1))),
        sample_shadow2D(OpIAdd(ivec_ids.Get(2), i, ConstS32(1, 1))))};
    const Id f_yy{OpVectorShuffle(vec_ids.Get(2), f, f, 1, 1)};
    const Id t{OpFMix(vec_ids.Get(2), s_xy, s_zw, f_yy)};
    const Id t_x{OpCompositeExtract(f32_id, t, 0)};
    const Id t_y{OpCompositeExtract(f32_id, t, 1)};
    const Id a_x{OpCompositeExtract(f32_id, f, 0)};
    const Id val{OpFMix(f32_id, t_x, t_y, a_x)};
    return OpCompositeConstruct(vec_ids.Get(4), val, val, val, val);
}

Id FragmentModule::AppendProcTexShiftOffset(Id v, ProcTexShift mode, ProcTexClamp clamp_mode) {
    const Id offset{clamp_mode == ProcTexClamp::MirroredRepeat ? ConstF32(1.f) : ConstF32(0.5f)};
    const Id v_i32{OpConvertFToS(i32_id, v)};

    const auto shift = [&](bool even) -> Id {
        const Id temp1{
            OpSDiv(i32_id, even ? OpIAdd(i32_id, v_i32, ConstS32(1)) : v_i32, ConstS32(2))};
        const Id temp2{OpConvertSToF(f32_id, OpSMod(i32_id, temp1, ConstS32(2)))};
        return OpFMul(f32_id, offset, temp2);
    };

    switch (mode) {
    case ProcTexShift::None:
        return ConstF32(0.f);
    case ProcTexShift::Odd:
        return shift(false);
    case ProcTexShift::Even:
        return shift(true);
    default:
        LOG_CRITICAL(Render, "Unknown shift mode {}", mode);
        return ConstF32(0.f);
    }
}

Id FragmentModule::AppendProcTexClamp(Id var, ProcTexClamp mode) {
    const Id zero{ConstF32(0.f)};
    const Id one{ConstF32(1.f)};

    const auto mirrored_repeat = [&]() -> Id {
        const Id fract{OpFract(f32_id, var)};
        const Id cond{OpIEqual(bool_id, OpSMod(i32_id, OpConvertFToS(i32_id, var), ConstS32(2)),
                               ConstS32(0))};
        return OpSelect(f32_id, cond, fract, OpFSub(f32_id, one, fract));
    };

    switch (mode) {
    case ProcTexClamp::ToZero:
        return OpSelect(f32_id, OpFOrdGreaterThan(bool_id, var, one), zero, var);
    case ProcTexClamp::ToEdge:
        return OpFMin(f32_id, var, one);
    case ProcTexClamp::SymmetricalRepeat:
        return OpFract(f32_id, var);
    case ProcTexClamp::MirroredRepeat:
        return mirrored_repeat();
    case ProcTexClamp::Pulse:
        return OpSelect(f32_id, OpFOrdGreaterThan(bool_id, var, ConstF32(0.5f)), one, zero);
    default:
        LOG_CRITICAL(Render, "Unknown clamp mode {}", mode);
        return OpFMin(f32_id, var, one);
    }
}

Id FragmentModule::AppendProcTexCombineAndMap(ProcTexCombiner combiner, Id u, Id v, Id offset) {
    const auto combined = [&]() -> Id {
        const Id u2v2{OpFma(f32_id, u, u, OpFMul(f32_id, v, v))};
        switch (combiner) {
        case ProcTexCombiner::U:
            return u;
        case ProcTexCombiner::U2:
            return OpFMul(f32_id, u, u);
        case TexturingRegs::ProcTexCombiner::V:
            return v;
        case TexturingRegs::ProcTexCombiner::V2:
            return OpFMul(f32_id, v, v);
        case TexturingRegs::ProcTexCombiner::Add:
            return OpFMul(f32_id, OpFAdd(f32_id, u, v), ConstF32(0.5f));
        case TexturingRegs::ProcTexCombiner::Add2:
            return OpFMul(f32_id, u2v2, ConstF32(0.5f));
        case TexturingRegs::ProcTexCombiner::SqrtAdd2:
            return OpFMin(f32_id, OpSqrt(f32_id, u2v2), ConstF32(1.f));
        case TexturingRegs::ProcTexCombiner::Min:
            return OpFMin(f32_id, u, v);
        case TexturingRegs::ProcTexCombiner::Max:
            return OpFMax(f32_id, u, v);
        case TexturingRegs::ProcTexCombiner::RMax: {
            const Id r{OpFma(f32_id, OpFAdd(f32_id, u, v), ConstF32(0.5f), OpSqrt(f32_id, u2v2))};
            return OpFMin(f32_id, OpFMul(f32_id, r, ConstF32(0.5f)), ConstF32(1.f));
        }
        default:
            LOG_CRITICAL(Render, "Unknown combiner {}", combiner);
            return ConstF32(0.f);
        }
    }();

    return ProcTexLookupLUT(offset, combined);
}

void FragmentModule::DefineTexSampler(u32 texture_unit) {
    const Id func_type{TypeFunction(vec_ids.Get(4))};
    sample_tex_unit_func[texture_unit] =
        OpFunction(vec_ids.Get(4), spv::FunctionControlMask::MaskNone, func_type);
    AddLabel(OpLabel());

    const Id zero_vec{ConstF32(0.f, 0.f, 0.f, 0.f)};

    if (texture_unit == 0 &&
        config.texture.texture0_type == TexturingRegs::TextureConfig::Disabled) {
        OpReturnValue(zero_vec);
        OpFunctionEnd();
        return;
    }

    if (texture_unit == 3) {
        if (config.proctex.enable) {
            OpReturnValue(ProcTexSampler());
        } else {
            OpReturnValue(zero_vec);
        }
        OpFunctionEnd();
        return;
    }

    const Id border_label{OpLabel()};
    const Id not_border_label{OpLabel()};

    u32 texcoord_num = texture_unit == 2 && config.texture.texture2_use_coord1 ? 1 : texture_unit;
    const Id texcoord{OpLoad(vec_ids.Get(2), texcoord_id[texcoord_num])};

    const auto& texture_border_color = config.texture.texture_border_color[texture_unit];
    if (texture_border_color.enable_s || texture_border_color.enable_t) {
        const Id texcoord_s{OpCompositeExtract(f32_id, texcoord, 0)};
        const Id texcoord_t{OpCompositeExtract(f32_id, texcoord, 1)};

        const Id s_lt_zero{OpFOrdLessThan(bool_id, texcoord_s, ConstF32(0.0f))};
        const Id s_gt_one{OpFOrdGreaterThan(bool_id, texcoord_s, ConstF32(1.0f))};
        const Id t_lt_zero{OpFOrdLessThan(bool_id, texcoord_t, ConstF32(0.0f))};
        const Id t_gt_one{OpFOrdGreaterThan(bool_id, texcoord_t, ConstF32(1.0f))};

        Id cond{};
        if (texture_border_color.enable_s && texture_border_color.enable_t) {
            cond = OpAny(bool_id, OpCompositeConstruct(bvec_ids.Get(4), s_lt_zero, s_gt_one,
                                                       t_lt_zero, t_gt_one));
        } else if (texture_border_color.enable_s) {
            cond = OpAny(bool_id, OpCompositeConstruct(bvec_ids.Get(2), s_lt_zero, s_gt_one));
        } else if (texture_border_color.enable_t) {
            cond = OpAny(bool_id, OpCompositeConstruct(bvec_ids.Get(2), t_lt_zero, t_gt_one));
        }

        OpSelectionMerge(not_border_label, spv::SelectionControlMask::MaskNone);
        OpBranchConditional(cond, border_label, not_border_label);

        AddLabel(border_label);
        const Id border_color{
            GetShaderDataMember(vec_ids.Get(4), ConstS32(28), ConstU32(texture_unit))};
        OpReturnValue(border_color);

        AddLabel(not_border_label);
    }

    // PICA's LOD formula for 2D textures.
    // This LOD formula is the same as the LOD lower limit defined in OpenGL.
    // f(x, y) >= max{m_u, m_v, m_w}
    // (See OpenGL 4.6 spec, 8.14.1 - Scale Factor and Level-of-Detail)
    const auto sample_lod = [&](Id tex_id) {
        const Id sampled_image{OpLoad(TypeSampledImage(image2d_id), tex_id)};
        const Id tex_image{OpImage(image2d_id, sampled_image)};
        const Id tex_size{OpImageQuerySizeLod(ivec_ids.Get(2), tex_image, ConstS32(0))};
        const Id coord{OpFMul(vec_ids.Get(2), texcoord, OpConvertSToF(vec_ids.Get(2), tex_size))};
        const Id abs_dfdx_coord{OpFAbs(vec_ids.Get(2), OpDPdx(vec_ids.Get(2), coord))};
        const Id abs_dfdy_coord{OpFAbs(vec_ids.Get(2), OpDPdy(vec_ids.Get(2), coord))};
        const Id d{OpFMax(vec_ids.Get(2), abs_dfdx_coord, abs_dfdy_coord)};
        const Id dx_dy_max{
            OpFMax(f32_id, OpCompositeExtract(f32_id, d, 0), OpCompositeExtract(f32_id, d, 1))};
        const Id lod{OpLog2(f32_id, dx_dy_max)};
        const Id lod_bias{GetShaderDataMember(f32_id, ConstS32(27), ConstU32(texture_unit))};
        const Id biased_lod{OpFAdd(f32_id, lod, lod_bias)};
        return OpImageSampleExplicitLod(vec_ids.Get(4), sampled_image, texcoord,
                                        spv::ImageOperandsMask::Lod, biased_lod);
    };

    const auto sample_3d = [&](Id tex_id, bool projection) {
        const Id image_type = !projection ? image_cube_id : image2d_id;
        const Id sampled_image{OpLoad(TypeSampledImage(image_type), tex_id)};
        const Id texcoord0_w{OpLoad(f32_id, texcoord0_w_id)};
        const Id coord{OpCompositeConstruct(vec_ids.Get(3), OpCompositeExtract(f32_id, texcoord, 0),
                                            OpCompositeExtract(f32_id, texcoord, 1), texcoord0_w)};
        if (projection) {
            return OpImageSampleProjImplicitLod(vec_ids.Get(4), sampled_image, coord);
        } else {
            return OpImageSampleImplicitLod(vec_ids.Get(4), sampled_image, coord);
        }
    };

    Id ret_val{void_id};
    switch (texture_unit) {
    case 0:
        // Only unit 0 respects the texturing type
        switch (config.texture.texture0_type) {
        case Pica::TexturingRegs::TextureConfig::Texture2D:
            ret_val = sample_lod(tex0_id);
            break;
        case Pica::TexturingRegs::TextureConfig::Projection2D:
            ret_val = sample_3d(tex0_id, true);
            break;
        case Pica::TexturingRegs::TextureConfig::TextureCube:
            ret_val = sample_3d(tex0_id, false);
            break;
        case Pica::TexturingRegs::TextureConfig::Shadow2D:
            ret_val = SampleShadow();
            // case Pica::TexturingRegs::TextureConfig::ShadowCube:
            // return "shadowTextureCube(texcoord0, texcoord0_w)";
            break;
        default:
            LOG_CRITICAL(Render, "Unhandled texture type {:x}",
                         config.texture.texture0_type.Value());
            UNIMPLEMENTED();
            ret_val = zero_vec;
            break;
        }
        break;
    case 1:
        ret_val = sample_lod(tex1_id);
        break;
    case 2:
        ret_val = sample_lod(tex2_id);
        break;
    default:
        UNREACHABLE();
        break;
    }

    OpReturnValue(ret_val);
    OpFunctionEnd();
}

Id FragmentModule::ProcTexSampler() {
    // Define noise tables at the beginning of the function
    if (config.proctex.noise_enable) {
        noise1d_table =
            DefineVar<false>(TypeArray(i32_id, ConstU32(16u)), spv::StorageClass::Function);
        noise2d_table =
            DefineVar<false>(TypeArray(i32_id, ConstU32(16u)), spv::StorageClass::Function);
    }
    lut_offsets = DefineVar<false>(TypeArray(i32_id, ConstU32(8u)), spv::StorageClass::Function);

    Id uv{};
    if (config.proctex.coord < 3) {
        const Id texcoord{OpLoad(vec_ids.Get(2), texcoord_id[config.proctex.coord.Value()])};
        uv = OpFAbs(vec_ids.Get(2), texcoord);
    } else {
        LOG_CRITICAL(Render, "Unexpected proctex.coord >= 3");
        uv = OpFAbs(vec_ids.Get(2), OpLoad(vec_ids.Get(2), texcoord_id[0]));
    }

    // This LOD formula is the same as the LOD upper limit defined in OpenGL.
    // f(x, y) <= m_u + m_v + m_w
    // (See OpenGL 4.6 spec, 8.14.1 - Scale Factor and Level-of-Detail)
    // Note: this is different from the one normal 2D textures use.
    const Id uv_1{OpFAbs(vec_ids.Get(2), OpDPdx(vec_ids.Get(2), uv))};
    const Id uv_2{OpFAbs(vec_ids.Get(2), OpDPdy(vec_ids.Get(2), uv))};
    const Id duv{OpFMax(vec_ids.Get(2), uv_1, uv_2)};

    // unlike normal texture, the bias is inside the log2
    const Id proctex_bias{GetShaderDataMember(f32_id, ConstS32(16))};
    const Id bias{
        OpFMul(f32_id, ConstF32(static_cast<f32>(config.proctex.lut_width)), proctex_bias)};
    const Id duv_xy{
        OpFAdd(f32_id, OpCompositeExtract(f32_id, duv, 0), OpCompositeExtract(f32_id, duv, 1))};

    Id lod{OpLog2(f32_id, OpFMul(f32_id, OpFAbs(f32_id, bias), duv_xy))};
    lod = OpSelect(f32_id, OpFOrdEqual(bool_id, proctex_bias, ConstF32(0.f)), ConstF32(0.f), lod);
    lod =
        OpFClamp(f32_id, lod, ConstF32(std::max(0.0f, static_cast<float>(config.proctex.lod_min))),
                 ConstF32(std::min(7.0f, static_cast<float>(config.proctex.lod_max))));

    // Get shift offset before noise generation
    const Id u_shift{AppendProcTexShiftOffset(OpCompositeExtract(f32_id, uv, 1),
                                              config.proctex.u_shift, config.proctex.u_clamp)};
    const Id v_shift{AppendProcTexShiftOffset(OpCompositeExtract(f32_id, uv, 0),
                                              config.proctex.v_shift, config.proctex.v_clamp)};

    // Generate noise
    if (config.proctex.noise_enable) {
        const Id proctex_noise_a{GetShaderDataMember(vec_ids.Get(2), ConstS32(21))};
        const Id noise_coef{ProcTexNoiseCoef(uv)};
        uv = OpFAdd(vec_ids.Get(2), uv,
                    OpVectorTimesScalar(vec_ids.Get(2), proctex_noise_a, noise_coef));
        uv = OpFAbs(vec_ids.Get(2), uv);
    }

    // Shift
    Id u{OpFAdd(f32_id, OpCompositeExtract(f32_id, uv, 0), u_shift)};
    Id v{OpFAdd(f32_id, OpCompositeExtract(f32_id, uv, 1), v_shift)};

    // Clamp
    u = AppendProcTexClamp(u, config.proctex.u_clamp);
    v = AppendProcTexClamp(v, config.proctex.v_clamp);

    // Combine and map
    const Id proctex_color_map_offset{GetShaderDataMember(i32_id, ConstS32(12))};
    const Id lut_coord{
        AppendProcTexCombineAndMap(config.proctex.color_combiner, u, v, proctex_color_map_offset)};

    Id final_color{};
    switch (config.proctex.lut_filter) {
    case ProcTexFilter::Linear:
    case ProcTexFilter::Nearest: {
        final_color = SampleProcTexColor(lut_coord, ConstS32(0));
        break;
    }
    case ProcTexFilter::NearestMipmapNearest:
    case ProcTexFilter::LinearMipmapNearest: {
        final_color = SampleProcTexColor(lut_coord, OpConvertFToS(i32_id, OpRound(f32_id, lod)));
        break;
    }
    case ProcTexFilter::NearestMipmapLinear:
    case ProcTexFilter::LinearMipmapLinear: {
        const Id lod_i{OpConvertFToS(i32_id, lod)};
        const Id lod_f{OpFract(f32_id, lod)};
        const Id color1{SampleProcTexColor(lut_coord, lod_i)};
        const Id color2{SampleProcTexColor(lut_coord, OpIAdd(i32_id, lod_i, ConstS32(1)))};
        final_color = OpFMix(f32_id, color1, color2, lod_f);
        break;
    }
    }

    if (config.proctex.separate_alpha) {
        const Id proctex_alpha_map_offset{GetShaderDataMember(i32_id, ConstS32(13))};
        const Id final_alpha{AppendProcTexCombineAndMap(config.proctex.alpha_combiner, u, v,
                                                        proctex_alpha_map_offset)};
        final_color = OpCompositeInsert(vec_ids.Get(4), final_alpha, final_color, 3);
    }

    return final_color;
}

Id FragmentModule::Byteround(Id variable_id, u32 size) {
    if (size > 1) {
        const Id scaled_vec_id{
            OpVectorTimesScalar(vec_ids.Get(size), variable_id, ConstF32(255.f))};
        const Id rounded_id{OpRound(vec_ids.Get(size), scaled_vec_id)};
        return OpVectorTimesScalar(vec_ids.Get(size), rounded_id, ConstF32(1.f / 255.f));
    } else {
        const Id rounded_id{OpRound(f32_id, OpFMul(f32_id, variable_id, ConstF32(255.f)))};
        return OpFMul(f32_id, rounded_id, ConstF32(1.f / 255.f));
    }
}

Id FragmentModule::ProcTexLookupLUT(Id offset, Id coord) {
    coord = OpFMul(f32_id, coord, ConstF32(128.f));
    const Id index_i{OpFClamp(f32_id, OpFloor(f32_id, coord), ConstF32(0.f), ConstF32(127.0f))};
    const Id index_f{OpFSub(f32_id, coord, index_i)};
    const Id p{OpIAdd(i32_id, OpConvertFToS(i32_id, index_i), offset)};
    if (!Sirit::ValidId(texture_buffer_lut_rg)) {
        texture_buffer_lut_rg = OpLoad(image_buffer_id, texture_buffer_lut_rg_id);
    }
    const Id entry{OpImageFetch(vec_ids.Get(4), texture_buffer_lut_rg, p)};
    const Id entry_r{OpCompositeExtract(f32_id, entry, 0)};
    const Id entry_g{OpCompositeExtract(f32_id, entry, 1)};
    return OpFClamp(f32_id, OpFma(f32_id, entry_g, index_f, entry_r), ConstF32(0.f), ConstF32(1.f));
};

Id FragmentModule::ProcTexNoiseCoef(Id x) {
    // Noise utility
    const auto proctex_noise_rand1D = [&](Id v) -> Id {
        InitTableS32(noise1d_table, 0, 4, 10, 8, 4, 9, 7, 12, 5, 15, 13, 14, 11, 15, 2, 11);
        const Id table_ptr{TypePointer(spv::StorageClass::Function, i32_id)};
        const Id left_tmp{OpIAdd(i32_id, OpSMod(i32_id, v, ConstS32(9)), ConstS32(2))};
        const Id left{OpBitwiseAnd(i32_id, OpIMul(i32_id, left_tmp, ConstS32(3)), ConstS32(0xF))};
        const Id table_index{OpBitwiseAnd(i32_id, OpSDiv(i32_id, v, ConstS32(9)), ConstS32(0xF))};
        const Id table_value{OpLoad(i32_id, OpAccessChain(table_ptr, noise1d_table, table_index))};
        return OpBitwiseXor(i32_id, left, table_value);
    };

    const auto proctex_noise_rand2D = [&](Id point) -> Id {
        InitTableS32(noise2d_table, 10, 2, 15, 8, 0, 7, 4, 5, 5, 13, 2, 6, 13, 9, 3, 14);
        const Id table_ptr{TypePointer(spv::StorageClass::Function, i32_id)};
        const Id point_x{OpConvertFToS(i32_id, OpCompositeExtract(f32_id, point, 0))};
        const Id point_y{OpConvertFToS(i32_id, OpCompositeExtract(f32_id, point, 1))};
        const Id u2{proctex_noise_rand1D(point_x)};
        const Id cond{OpIEqual(bool_id, OpBitwiseAnd(i32_id, u2, ConstS32(3)), ConstS32(1))};
        const Id table_value{OpLoad(i32_id, OpAccessChain(table_ptr, noise2d_table, u2))};
        Id v2{proctex_noise_rand1D(point_y)};
        v2 = OpIAdd(i32_id, v2, OpSelect(i32_id, cond, ConstS32(4), ConstS32(0)));
        v2 = OpBitwiseXor(i32_id, v2,
                          OpIMul(i32_id, OpBitwiseAnd(i32_id, u2, ConstS32(1)), ConstS32(6)));
        v2 = OpIAdd(i32_id, v2, OpIAdd(i32_id, u2, ConstS32(10)));
        v2 = OpBitwiseAnd(i32_id, v2, ConstS32(0xF));
        v2 = OpBitwiseXor(i32_id, v2, table_value);
        return OpFma(f32_id, OpConvertSToF(f32_id, v2), ConstF32(2.f / 15.f), ConstF32(-1.f));
    };

    const Id proctex_noise_f{GetShaderDataMember(vec_ids.Get(2), ConstS32(20))};
    const Id proctex_noise_p{GetShaderDataMember(vec_ids.Get(2), ConstS32(22))};
    const Id grid{OpFMul(vec_ids.Get(2),
                         OpVectorTimesScalar(vec_ids.Get(2), proctex_noise_f, ConstF32(9.f)),
                         OpFAbs(vec_ids.Get(2), OpFAdd(vec_ids.Get(2), x, proctex_noise_p)))};
    const Id point{OpFloor(vec_ids.Get(2), grid)};
    const Id frac{OpFSub(vec_ids.Get(2), grid, point)};
    const Id frac_x{OpCompositeExtract(f32_id, frac, 0)};
    const Id frac_y{OpCompositeExtract(f32_id, frac, 1)};
    const Id frac_x_y{OpFAdd(f32_id, frac_x, frac_y)};
    const Id g0{OpFMul(f32_id, proctex_noise_rand2D(point), frac_x_y)};
    const Id frac_x_y_min_one{OpFSub(f32_id, frac_x_y, ConstF32(1.f))};
    const Id g1{OpFMul(f32_id,
                       proctex_noise_rand2D(OpFAdd(vec_ids.Get(2), point, ConstF32(1.f, 0.f))),
                       frac_x_y_min_one)};
    const Id g2{OpFMul(f32_id,
                       proctex_noise_rand2D(OpFAdd(vec_ids.Get(2), point, ConstF32(0.f, 1.f))),
                       frac_x_y_min_one)};
    const Id frac_x_y_min_two{OpFSub(f32_id, frac_x_y, ConstF32(2.f))};
    const Id g3{OpFMul(f32_id,
                       proctex_noise_rand2D(OpFAdd(vec_ids.Get(2), point, ConstF32(1.f, 1.f))),
                       frac_x_y_min_two)};
    const Id proctex_noise_lut_offset{GetShaderDataMember(i32_id, ConstS32(11))};
    const Id x_noise{ProcTexLookupLUT(proctex_noise_lut_offset, frac_x)};
    const Id y_noise{ProcTexLookupLUT(proctex_noise_lut_offset, frac_y)};
    const Id x0{OpFMix(f32_id, g0, g1, x_noise)};
    const Id x1{OpFMix(f32_id, g2, g3, x_noise)};
    return OpFMix(f32_id, x0, x1, y_noise);
}

Id FragmentModule::SampleProcTexColor(Id lut_coord, Id level) {
    const Id lut_width{OpShiftRightArithmetic(i32_id, ConstS32(config.proctex.lut_width), level)};
    const Id lut_ptr{TypePointer(spv::StorageClass::Function, i32_id)};
    // Offsets for level 4-7 seem to be hardcoded
    InitTableS32(lut_offsets, config.proctex.lut_offset0, config.proctex.lut_offset1,
                 config.proctex.lut_offset2, config.proctex.lut_offset3, 0xF0, 0xF8, 0xFC, 0xFE);
    const Id lut_offset{OpLoad(i32_id, OpAccessChain(lut_ptr, lut_offsets, level))};
    // For the color lut, coord=0.0 is lut[offset] and coord=1.0 is lut[offset+width-1]
    lut_coord =
        OpFMul(f32_id, lut_coord, OpConvertSToF(f32_id, OpISub(i32_id, lut_width, ConstS32(1))));

    if (!Sirit::ValidId(texture_buffer_lut_rgba)) {
        texture_buffer_lut_rgba = OpLoad(image_buffer_id, texture_buffer_lut_rgba_id);
    }

    const Id proctex_lut_offset{GetShaderDataMember(i32_id, ConstS32(14))};

    switch (config.proctex.lut_filter) {
    case ProcTexFilter::Linear:
    case ProcTexFilter::LinearMipmapLinear:
    case ProcTexFilter::LinearMipmapNearest: {
        const Id lut_index_i{OpIAdd(i32_id, OpConvertFToS(i32_id, lut_coord), lut_offset)};
        const Id lut_index_f{OpFract(f32_id, lut_coord)};
        const Id proctex_diff_lut_offset{GetShaderDataMember(i32_id, ConstS32(15))};
        const Id p1{OpIAdd(i32_id, lut_index_i, proctex_lut_offset)};
        const Id p2{OpIAdd(i32_id, lut_index_i, proctex_diff_lut_offset)};
        const Id texel1{OpImageFetch(vec_ids.Get(4), texture_buffer_lut_rgba, p1)};
        const Id texel2{OpImageFetch(vec_ids.Get(4), texture_buffer_lut_rgba, p2)};
        return OpFAdd(vec_ids.Get(4), texel1,
                      OpVectorTimesScalar(vec_ids.Get(4), texel2, lut_index_f));
    }
    case ProcTexFilter::Nearest:
    case ProcTexFilter::NearestMipmapLinear:
    case ProcTexFilter::NearestMipmapNearest: {
        lut_coord = OpFAdd(f32_id, lut_coord, OpConvertSToF(f32_id, lut_offset));
        const Id lut_coord_rounded{OpConvertFToS(i32_id, OpRound(f32_id, lut_coord))};
        const Id p{OpIAdd(i32_id, lut_coord_rounded, proctex_lut_offset)};
        return OpImageFetch(vec_ids.Get(4), texture_buffer_lut_rgba, p);
    }
    }

    return Id{};
}

Id FragmentModule::LookupLightingLUT(Id lut_index, Id index, Id delta) {
    // Only load the texture buffer lut once
    if (!Sirit::ValidId(texture_buffer_lut_lf)) {
        texture_buffer_lut_lf = OpLoad(image_buffer_id, texture_buffer_lut_lf_id);
    }

    const Id lut_index_x{OpShiftRightArithmetic(i32_id, lut_index, ConstS32(2))};
    const Id lut_index_y{OpBitwiseAnd(i32_id, lut_index, ConstS32(3))};
    const Id lut_offset{GetShaderDataMember(i32_id, ConstS32(18), lut_index_x, lut_index_y)};
    const Id coord{OpIAdd(i32_id, lut_offset, index)};
    const Id entry{OpImageFetch(vec_ids.Get(4), texture_buffer_lut_lf, coord)};
    const Id entry_r{OpCompositeExtract(f32_id, entry, 0)};
    const Id entry_g{OpCompositeExtract(f32_id, entry, 1)};
    return OpFma(f32_id, entry_g, delta, entry_r);
}

Id FragmentModule::GetSource(TevStageConfig::Source source, s32 index) {
    using Source = TevStageConfig::Source;
    switch (source) {
    case Source::PrimaryColor:
        return rounded_primary_color;
    case Source::PrimaryFragmentColor:
        return primary_fragment_color;
    case Source::SecondaryFragmentColor:
        return secondary_fragment_color;
    case Source::Texture0:
        return OpFunctionCall(vec_ids.Get(4), sample_tex_unit_func[0]);
    case Source::Texture1:
        return OpFunctionCall(vec_ids.Get(4), sample_tex_unit_func[1]);
    case Source::Texture2:
        return OpFunctionCall(vec_ids.Get(4), sample_tex_unit_func[2]);
    case Source::Texture3:
        return OpFunctionCall(vec_ids.Get(4), sample_tex_unit_func[3]);
    case Source::PreviousBuffer:
        return combiner_buffer;
    case Source::Constant:
        return GetShaderDataMember(vec_ids.Get(4), ConstS32(25), ConstS32(index));
    case Source::Previous:
        return combiner_output;
    default:
        LOG_CRITICAL(Render, "Unknown source op {}", source);
        return ConstF32(0.f, 0.f, 0.f, 0.f);
    }
}

Id FragmentModule::AppendColorModifier(TevStageConfig::ColorModifier modifier,
                                       TevStageConfig::Source source, s32 index) {
    using Source = TevStageConfig::Source;
    using ColorModifier = TevStageConfig::ColorModifier;
    const TexturingRegs::TevStageConfig stage = config.texture.tev_stages[index];
    const bool force_source3 = index == 0 && source == Source::Previous;
    const Id source_color{GetSource(force_source3 ? stage.color_source3.Value() : source, index)};
    const Id one_vec{ConstF32(1.f, 1.f, 1.f)};

    const auto shuffle = [&](s32 r, s32 g, s32 b) -> Id {
        return OpVectorShuffle(vec_ids.Get(3), source_color, source_color, r, g, b);
    };

    switch (modifier) {
    case ColorModifier::SourceColor:
        return shuffle(0, 1, 2);
    case ColorModifier::OneMinusSourceColor:
        return OpFSub(vec_ids.Get(3), one_vec, shuffle(0, 1, 2));
    case ColorModifier::SourceRed:
        return shuffle(0, 0, 0);
    case ColorModifier::OneMinusSourceRed:
        return OpFSub(vec_ids.Get(3), one_vec, shuffle(0, 0, 0));
    case ColorModifier::SourceGreen:
        return shuffle(1, 1, 1);
    case ColorModifier::OneMinusSourceGreen:
        return OpFSub(vec_ids.Get(3), one_vec, shuffle(1, 1, 1));
    case ColorModifier::SourceBlue:
        return shuffle(2, 2, 2);
    case ColorModifier::OneMinusSourceBlue:
        return OpFSub(vec_ids.Get(3), one_vec, shuffle(2, 2, 2));
    case ColorModifier::SourceAlpha:
        return shuffle(3, 3, 3);
    case ColorModifier::OneMinusSourceAlpha:
        return OpFSub(vec_ids.Get(3), one_vec, shuffle(3, 3, 3));
    default:
        LOG_CRITICAL(Render, "Unknown color modifier op {}", modifier);
        return one_vec;
    }
}

Id FragmentModule::AppendAlphaModifier(TevStageConfig::AlphaModifier modifier,
                                       TevStageConfig::Source source, s32 index) {
    using Source = TevStageConfig::Source;
    using AlphaModifier = TevStageConfig::AlphaModifier;
    const TexturingRegs::TevStageConfig stage = config.texture.tev_stages[index];
    const bool force_source3 = index == 0 && source == Source::Previous;
    const Id source_alpha{GetSource(force_source3 ? stage.alpha_source3.Value() : source, index)};
    const Id one_f32{ConstF32(1.f)};

    const auto component = [&](s32 c) -> Id { return OpCompositeExtract(f32_id, source_alpha, c); };

    switch (modifier) {
    case AlphaModifier::SourceAlpha:
        return component(3);
    case AlphaModifier::OneMinusSourceAlpha:
        return OpFSub(f32_id, one_f32, component(3));
    case AlphaModifier::SourceRed:
        return component(0);
    case AlphaModifier::OneMinusSourceRed:
        return OpFSub(f32_id, one_f32, component(0));
    case AlphaModifier::SourceGreen:
        return component(1);
    case AlphaModifier::OneMinusSourceGreen:
        return OpFSub(f32_id, one_f32, component(1));
    case AlphaModifier::SourceBlue:
        return component(2);
    case AlphaModifier::OneMinusSourceBlue:
        return OpFSub(f32_id, one_f32, component(2));
    default:
        LOG_CRITICAL(Render, "Unknown alpha modifier op {}", modifier);
        return one_f32;
    }
}

Id FragmentModule::AppendColorCombiner(Pica::TexturingRegs::TevStageConfig::Operation operation) {
    using Operation = TevStageConfig::Operation;
    const Id half_vec{ConstF32(0.5f, 0.5f, 0.5f)};
    const Id one_vec{ConstF32(1.f, 1.f, 1.f)};
    const Id zero_vec{ConstF32(0.f, 0.f, 0.f)};
    Id color{};

    switch (operation) {
    case Operation::Replace:
        color = color_results_1;
        break;
    case Operation::Modulate:
        color = OpFMul(vec_ids.Get(3), color_results_1, color_results_2);
        break;
    case Operation::Add:
        color = OpFAdd(vec_ids.Get(3), color_results_1, color_results_2);
        break;
    case Operation::AddSigned:
        color = OpFSub(vec_ids.Get(3), OpFAdd(vec_ids.Get(3), color_results_1, color_results_2),
                       half_vec);
        break;
    case Operation::Lerp:
        color = OpFMix(vec_ids.Get(3), color_results_2, color_results_1, color_results_3);
        break;
    case Operation::Subtract:
        color = OpFSub(vec_ids.Get(3), color_results_1, color_results_2);
        break;
    case Operation::MultiplyThenAdd:
        color = OpFma(vec_ids.Get(3), color_results_1, color_results_2, color_results_3);
        break;
    case Operation::AddThenMultiply:
        color = OpFMin(vec_ids.Get(3), OpFAdd(vec_ids.Get(3), color_results_1, color_results_2),
                       one_vec);
        color = OpFMul(vec_ids.Get(3), color, color_results_3);
        break;
    case Operation::Dot3_RGB:
    case Operation::Dot3_RGBA:
        color = OpDot(f32_id, OpFSub(vec_ids.Get(3), color_results_1, half_vec),
                      OpFSub(vec_ids.Get(3), color_results_2, half_vec));
        color = OpFMul(f32_id, color, ConstF32(4.f));
        color = OpCompositeConstruct(vec_ids.Get(3), color, color, color);
        break;
    default:
        color = zero_vec;
        LOG_CRITICAL(Render, "Unknown color combiner operation: {}", operation);
        break;
    }

    // Clamp result to 0.0, 1.0
    return OpFClamp(vec_ids.Get(3), color, zero_vec, one_vec);
}

Id FragmentModule::AppendAlphaCombiner(TevStageConfig::Operation operation) {
    using Operation = TevStageConfig::Operation;
    Id color{};

    switch (operation) {
    case Operation::Replace:
        color = alpha_results_1;
        break;
    case Operation::Modulate:
        color = OpFMul(f32_id, alpha_results_1, alpha_results_2);
        break;
    case Operation::Add:
        color = OpFAdd(f32_id, alpha_results_1, alpha_results_2);
        break;
    case Operation::AddSigned:
        color = OpFSub(f32_id, OpFAdd(f32_id, alpha_results_1, alpha_results_2), ConstF32(0.5f));
        break;
    case Operation::Lerp:
        color = OpFMix(f32_id, alpha_results_2, alpha_results_1, alpha_results_3);
        break;
    case Operation::Subtract:
        color = OpFSub(f32_id, alpha_results_1, alpha_results_2);
        break;
    case Operation::MultiplyThenAdd:
        color = OpFma(f32_id, alpha_results_1, alpha_results_2, alpha_results_3);
        break;
    case Operation::AddThenMultiply:
        color = OpFMin(f32_id, OpFAdd(f32_id, alpha_results_1, alpha_results_2), ConstF32(1.f));
        color = OpFMul(f32_id, color, alpha_results_3);
        break;
    default:
        color = ConstF32(0.f);
        LOG_CRITICAL(Render, "Unknown alpha combiner operation: {}", operation);
        break;
    }

    return OpFClamp(f32_id, color, ConstF32(0.f), ConstF32(1.f));
}

void FragmentModule::DefineArithmeticTypes() {
    void_id = Name(TypeVoid(), "void_id");
    bool_id = Name(TypeBool(), "bool_id");
    f32_id = Name(TypeFloat(32), "f32_id");
    i32_id = Name(TypeSInt(32), "i32_id");
    u32_id = Name(TypeUInt(32), "u32_id");

    for (u32 size = 2; size <= 4; size++) {
        const u32 i = size - 2;
        vec_ids.ids[i] = Name(TypeVector(f32_id, size), fmt::format("vec{}_id", size));
        ivec_ids.ids[i] = Name(TypeVector(i32_id, size), fmt::format("ivec{}_id", size));
        uvec_ids.ids[i] = Name(TypeVector(u32_id, size), fmt::format("uvec{}_id", size));
        bvec_ids.ids[i] = Name(TypeVector(bool_id, size), fmt::format("bvec{}_id", size));
    }
}

void FragmentModule::DefineEntryPoint() {
    AddCapability(spv::Capability::Shader);
    AddCapability(spv::Capability::SampledBuffer);
    AddCapability(spv::Capability::ImageQuery);
    if (use_fragment_shader_barycentric) {
        AddCapability(spv::Capability::FragmentBarycentricKHR);
        AddExtension("SPV_KHR_fragment_shader_barycentric");
    }
    SetMemoryModel(spv::AddressingModel::Logical, spv::MemoryModel::GLSL450);

    const Id main_type{TypeFunction(TypeVoid())};
    const Id main_func{OpFunction(TypeVoid(), spv::FunctionControlMask::MaskNone, main_type)};

    boost::container::small_vector<Id, 11> interface_ids{
        primary_color_id, texcoord_id[0], texcoord_id[1], texcoord_id[2],   texcoord0_w_id,
        normquat_id,      view_id,        color_id,       gl_frag_coord_id, gl_frag_depth_id,
    };
    if (use_fragment_shader_barycentric) {
        interface_ids.push_back(gl_bary_coord_id);
    }

    AddEntryPoint(spv::ExecutionModel::Fragment, main_func, "main", interface_ids);
    AddExecutionMode(main_func, spv::ExecutionMode::OriginUpperLeft);
    AddExecutionMode(main_func, spv::ExecutionMode::DepthReplacing);
}

void FragmentModule::DefineUniformStructs() {
    const Id light_src_struct_id{TypeStruct(vec_ids.Get(3), vec_ids.Get(3), vec_ids.Get(3),
                                            vec_ids.Get(3), vec_ids.Get(3), vec_ids.Get(3), f32_id,
                                            f32_id)};

    const Id light_src_array_id{TypeArray(light_src_struct_id, ConstU32(NUM_LIGHTS))};
    const Id lighting_lut_array_id{TypeArray(ivec_ids.Get(4), ConstU32(NUM_LIGHTING_SAMPLERS / 4))};
    const Id const_color_array_id{TypeArray(vec_ids.Get(4), ConstU32(NUM_TEV_STAGES))};
    const Id border_color_array_id{TypeArray(vec_ids.Get(4), ConstU32(NUM_NON_PROC_TEX_UNITS))};

    const Id shader_data_struct_id{
        TypeStruct(i32_id, i32_id, f32_id, f32_id, f32_id, f32_id, i32_id, i32_id, i32_id, i32_id,
                   i32_id, i32_id, i32_id, i32_id, i32_id, i32_id, f32_id, i32_id,
                   lighting_lut_array_id, vec_ids.Get(3), vec_ids.Get(2), vec_ids.Get(2),
                   vec_ids.Get(2), vec_ids.Get(3), light_src_array_id, const_color_array_id,
                   vec_ids.Get(4), vec_ids.Get(3), border_color_array_id, vec_ids.Get(4))};

    constexpr std::array light_src_offsets{0u, 16u, 32u, 48u, 64u, 80u, 92u, 96u};
    constexpr std::array shader_data_offsets{
        0u,  4u,  8u,  12u, 16u,  20u,  24u,  28u,  32u,  36u,  40u,   44u,   48u,   52u,   56u,
        60u, 64u, 68u, 80u, 176u, 192u, 200u, 208u, 224u, 240u, 1136u, 1232u, 1248u, 1264u, 1312u};

    Decorate(lighting_lut_array_id, spv::Decoration::ArrayStride, 16u);
    Decorate(light_src_array_id, spv::Decoration::ArrayStride, 112u);
    Decorate(const_color_array_id, spv::Decoration::ArrayStride, 16u);
    Decorate(border_color_array_id, spv::Decoration::ArrayStride, 16u);
    for (u32 i = 0; i < static_cast<u32>(light_src_offsets.size()); i++) {
        MemberDecorate(light_src_struct_id, i, spv::Decoration::Offset, light_src_offsets[i]);
    }
    for (u32 i = 0; i < static_cast<u32>(shader_data_offsets.size()); i++) {
        MemberDecorate(shader_data_struct_id, i, spv::Decoration::Offset, shader_data_offsets[i]);
    }
    Decorate(shader_data_struct_id, spv::Decoration::Block);

    shader_data_id = AddGlobalVariable(
        TypePointer(spv::StorageClass::Uniform, shader_data_struct_id), spv::StorageClass::Uniform);
    Decorate(shader_data_id, spv::Decoration::DescriptorSet, 0);
    Decorate(shader_data_id, spv::Decoration::Binding, 2);
}

void FragmentModule::DefineInterface() {
    // Define interface block
    primary_color_id = DefineInput(vec_ids.Get(4), 1);
    texcoord_id[0] = DefineInput(vec_ids.Get(2), 2);
    texcoord_id[1] = DefineInput(vec_ids.Get(2), 3);
    texcoord_id[2] = DefineInput(vec_ids.Get(2), 4);
    texcoord0_w_id = DefineInput(f32_id, 5);
    if (use_fragment_shader_barycentric) {
        normquat_id = DefineInput(TypeArray(vec_ids.Get(4), ConstU32(3U)), 6);
        Decorate(normquat_id, spv::Decoration::PerVertexKHR);
    } else {
        normquat_id = DefineInput(vec_ids.Get(4), 6);
    }
    view_id = DefineInput(vec_ids.Get(3), 7);
    color_id = DefineOutput(vec_ids.Get(4), 0);

    // Define the texture unit samplers types
    image_buffer_id = TypeImage(f32_id, spv::Dim::Buffer, 0, 0, 0, 1, spv::ImageFormat::Unknown);
    image2d_id = TypeImage(f32_id, spv::Dim::Dim2D, 0, 0, 0, 1, spv::ImageFormat::Unknown);
    image_cube_id = TypeImage(f32_id, spv::Dim::Cube, 0, 0, 0, 1, spv::ImageFormat::Unknown);
    image_r32_id = TypeImage(u32_id, spv::Dim::Dim2D, 0, 0, 0, 2, spv::ImageFormat::R32ui);
    sampler_id = TypeSampler();

    // Define lighting texture buffers
    texture_buffer_lut_lf_id = DefineUniformConst(image_buffer_id, 0, 3);
    texture_buffer_lut_rg_id = DefineUniformConst(image_buffer_id, 0, 4);
    texture_buffer_lut_rgba_id = DefineUniformConst(image_buffer_id, 0, 5);

    // Define texture unit samplers
    const auto texture_type = config.texture.texture0_type.Value();
    const auto tex0_type = texture_type == TextureType::TextureCube ? image_cube_id : image2d_id;
    tex0_id = DefineUniformConst(TypeSampledImage(tex0_type), 1, 0);
    tex1_id = DefineUniformConst(TypeSampledImage(image2d_id), 1, 1);
    tex2_id = DefineUniformConst(TypeSampledImage(image2d_id), 1, 2);

    // Define shadow textures
    shadow_texture_px_id = DefineUniformConst(image_r32_id, 2, 0, true);

    // Define built-ins
    gl_frag_coord_id = DefineVar(vec_ids.Get(4), spv::StorageClass::Input);
    gl_frag_depth_id = DefineVar(f32_id, spv::StorageClass::Output);
    Decorate(gl_frag_coord_id, spv::Decoration::BuiltIn, spv::BuiltIn::FragCoord);
    Decorate(gl_frag_depth_id, spv::Decoration::BuiltIn, spv::BuiltIn::FragDepth);
    if (use_fragment_shader_barycentric) {
        gl_bary_coord_id = DefineVar(vec_ids.Get(3), spv::StorageClass::Input);
        Decorate(gl_bary_coord_id, spv::Decoration::BuiltIn, spv::BuiltIn::BaryCoordKHR);
    }
}

std::vector<u32> GenerateFragmentShader(const FSConfig& config, const Profile& profile) {
    FragmentModule module{config, profile};
    module.Generate();
    return module.Assemble();
}

} // namespace Pica::Shader::Generator::SPIRV
