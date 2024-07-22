// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef ARCHITECTURE_ARM64
#include <arm_neon.h>
#endif
#include "common/alignment.h"
#include "core/memory.h"
#include "video_core/pica/pica_core.h"
#include "video_core/rasterizer_accelerated.h"

namespace VideoCore {

using Pica::f24;

static Common::Vec4f ColorRGBA8(const u32 color) {
    const auto rgba =
        Common::Vec4u{color >> 0 & 0xFF, color >> 8 & 0xFF, color >> 16 & 0xFF, color >> 24 & 0xFF};
    return rgba / 255.0f;
}

static Common::Vec3f LightColor(const Pica::LightingRegs::LightColor& color) {
    return Common::Vec3u{color.r, color.g, color.b} / 255.0f;
}

RasterizerAccelerated::HardwareVertex::HardwareVertex(const Pica::OutputVertex& v,
                                                      bool flip_quaternion) {
    position[0] = v.pos.x.ToFloat32();
    position[1] = v.pos.y.ToFloat32();
    position[2] = v.pos.z.ToFloat32();
    position[3] = v.pos.w.ToFloat32();
    color[0] = v.color.x.ToFloat32();
    color[1] = v.color.y.ToFloat32();
    color[2] = v.color.z.ToFloat32();
    color[3] = v.color.w.ToFloat32();
    tex_coord0[0] = v.tc0.x.ToFloat32();
    tex_coord0[1] = v.tc0.y.ToFloat32();
    tex_coord1[0] = v.tc1.x.ToFloat32();
    tex_coord1[1] = v.tc1.y.ToFloat32();
    tex_coord2[0] = v.tc2.x.ToFloat32();
    tex_coord2[1] = v.tc2.y.ToFloat32();
    tex_coord0_w = v.tc0_w.ToFloat32();
    normquat[0] = v.quat.x.ToFloat32();
    normquat[1] = v.quat.y.ToFloat32();
    normquat[2] = v.quat.z.ToFloat32();
    normquat[3] = v.quat.w.ToFloat32();
    view[0] = v.view.x.ToFloat32();
    view[1] = v.view.y.ToFloat32();
    view[2] = v.view.z.ToFloat32();

    if (flip_quaternion) {
        normquat = -normquat;
    }
}

RasterizerAccelerated::RasterizerAccelerated(Memory::MemorySystem& memory_, Pica::PicaCore& pica_)
    : memory{memory_}, pica{pica_}, regs{pica.regs.internal} {
    fs_uniform_block_data.lighting_lut_dirty.fill(true);
}

/**
 * This is a helper function to resolve an issue when interpolating opposite quaternions. See below
 * for a detailed description of this issue (yuriks):
 *
 * For any rotation, there are two quaternions Q, and -Q, that represent the same rotation. If you
 * interpolate two quaternions that are opposite, instead of going from one rotation to another
 * using the shortest path, you'll go around the longest path. You can test if two quaternions are
 * opposite by checking if Dot(Q1, Q2) < 0. In that case, you can flip either of them, therefore
 * making Dot(Q1, -Q2) positive.
 *
 * This solution corrects this issue per-vertex before passing the quaternions to OpenGL. This is
 * correct for most cases but can still rotate around the long way sometimes. An implementation
 * which did `lerp(lerp(Q1, Q2), Q3)` (with proper weighting), applying the dot product check
 * between each step would work for those cases at the cost of being more complex to implement.
 *
 * Fortunately however, the 3DS hardware happens to also use this exact same logic to work around
 * these issues, making this basic implementation actually more accurate to the hardware.
 */
static bool AreQuaternionsOpposite(Common::Vec4<f24> qa, Common::Vec4<f24> qb) {
#ifdef ARCHITECTURE_ARM64
    const float32_t a[4] = {qa.x.ToFloat32(), qa.y.ToFloat32(), qa.z.ToFloat32(), qa.w.ToFloat32()};
    const float32_t b[4] = {qb.x.ToFloat32(), qb.y.ToFloat32(), qb.z.ToFloat32(), qb.w.ToFloat32()};
    const float32x4_t aa = vld1q_f32(a);
    const float32x4_t bb = vld1q_f32(b);
    const float32x4_t mm = vmulq_f32(aa, bb);
    const float32x2_t s2 = vadd_f32(vget_high_f32(mm), vget_low_f32(mm));
    return (vget_lane_f32(vpadd_f32(s2, s2), 0) < 0.f);
#else
    Common::Vec4f a{qa.x.ToFloat32(), qa.y.ToFloat32(), qa.z.ToFloat32(), qa.w.ToFloat32()};
    Common::Vec4f b{qb.x.ToFloat32(), qb.y.ToFloat32(), qb.z.ToFloat32(), qb.w.ToFloat32()};
    return (Common::Dot(a, b) < 0.f);
#endif
}

void RasterizerAccelerated::AddTriangle(const Pica::OutputVertex& v0, const Pica::OutputVertex& v1,
                                        const Pica::OutputVertex& v2) {
    vertex_batch.emplace_back(v0, false);
    vertex_batch.emplace_back(v1, AreQuaternionsOpposite(v0.quat, v1.quat));
    vertex_batch.emplace_back(v2, AreQuaternionsOpposite(v0.quat, v2.quat));
}

RasterizerAccelerated::VertexArrayInfo RasterizerAccelerated::AnalyzeVertexArray(
    bool is_indexed, u32 stride_alignment) {
    const auto& vertex_attributes = regs.pipeline.vertex_attributes;

    u32 vertex_min;
    u32 vertex_max;
    if (is_indexed) {
        const auto& index_info = regs.pipeline.index_array;
        const PAddr address = vertex_attributes.GetPhysicalBaseAddress() + index_info.offset;
        const u8* index_address_8 = memory.GetPhysicalPointer(address);
        const u16* index_address_16 = reinterpret_cast<const u16*>(index_address_8);
        const bool index_u16 = index_info.format != 0;

        vertex_min = 0xFFFF;
        vertex_max = 0;
        const u32 size = regs.pipeline.num_vertices * (index_u16 ? 2 : 1);
        FlushRegion(address, size);
        for (u32 index = 0; index < regs.pipeline.num_vertices; ++index) {
            const u32 vertex = index_u16 ? index_address_16[index] : index_address_8[index];
            vertex_min = std::min(vertex_min, vertex);
            vertex_max = std::max(vertex_max, vertex);
        }
    } else {
        vertex_min = regs.pipeline.vertex_offset;
        vertex_max = regs.pipeline.vertex_offset + regs.pipeline.num_vertices - 1;
    }

    const u32 vertex_num = vertex_max - vertex_min + 1;
    u32 vs_input_size = 0;
    for (const auto& loader : vertex_attributes.attribute_loaders) {
        if (loader.component_count != 0) {
            const u32 aligned_stride =
                Common::AlignUp(static_cast<u32>(loader.byte_count), stride_alignment);
            vs_input_size += Common::AlignUp(aligned_stride * vertex_num, 4);
        }
    }

    return {vertex_min, vertex_max, vs_input_size};
}

void RasterizerAccelerated::SyncEntireState() {
    // Sync renderer-specific fixed-function state
    SyncFixedState();

    // Sync uniforms
    SyncClipPlane();
    SyncDepthScale();
    SyncDepthOffset();
    SyncAlphaTest();
    SyncCombinerColor();
    auto& tev_stages = regs.texturing.GetTevStages();
    for (std::size_t index = 0; index < tev_stages.size(); ++index) {
        SyncTevConstColor(index, tev_stages[index]);
    }

    SyncGlobalAmbient();
    for (u32 light_index = 0; light_index < 8; light_index++) {
        SyncLightSpecular0(light_index);
        SyncLightSpecular1(light_index);
        SyncLightDiffuse(light_index);
        SyncLightAmbient(light_index);
        SyncLightPosition(light_index);
        SyncLightDistanceAttenuationBias(light_index);
        SyncLightDistanceAttenuationScale(light_index);
    }

    SyncFogColor();
    SyncProcTexNoise();
    SyncProcTexBias();
    SyncShadowBias();
    SyncShadowTextureBias();

    for (u32 tex_index = 0; tex_index < 3; tex_index++) {
        SyncTextureLodBias(tex_index);
    }
}

void RasterizerAccelerated::NotifyPicaRegisterChanged(u32 id) {
    switch (id) {
    // Depth modifiers
    case PICA_REG_INDEX(rasterizer.viewport_depth_range):
        SyncDepthScale();
        break;
    case PICA_REG_INDEX(rasterizer.viewport_depth_near_plane):
        SyncDepthOffset();
        break;

    // Depth buffering
    case PICA_REG_INDEX(rasterizer.depthmap_enable):
        shader_dirty = true;
        break;

    // Shadow texture
    case PICA_REG_INDEX(texturing.shadow):
        SyncShadowTextureBias();
        break;

    // Fog state
    case PICA_REG_INDEX(texturing.fog_color):
        SyncFogColor();
        break;
    case PICA_REG_INDEX(texturing.fog_lut_data[0]):
    case PICA_REG_INDEX(texturing.fog_lut_data[1]):
    case PICA_REG_INDEX(texturing.fog_lut_data[2]):
    case PICA_REG_INDEX(texturing.fog_lut_data[3]):
    case PICA_REG_INDEX(texturing.fog_lut_data[4]):
    case PICA_REG_INDEX(texturing.fog_lut_data[5]):
    case PICA_REG_INDEX(texturing.fog_lut_data[6]):
    case PICA_REG_INDEX(texturing.fog_lut_data[7]):
        fs_uniform_block_data.fog_lut_dirty = true;
        break;

    // ProcTex state
    case PICA_REG_INDEX(texturing.proctex):
    case PICA_REG_INDEX(texturing.proctex_lut):
    case PICA_REG_INDEX(texturing.proctex_lut_offset):
        SyncProcTexBias();
        shader_dirty = true;
        break;

    case PICA_REG_INDEX(texturing.proctex_noise_u):
    case PICA_REG_INDEX(texturing.proctex_noise_v):
    case PICA_REG_INDEX(texturing.proctex_noise_frequency):
        SyncProcTexNoise();
        break;

    case PICA_REG_INDEX(texturing.proctex_lut_data[0]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[1]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[2]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[3]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[4]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[5]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[6]):
    case PICA_REG_INDEX(texturing.proctex_lut_data[7]):
        using Pica::TexturingRegs;
        switch (regs.texturing.proctex_lut_config.ref_table.Value()) {
        case TexturingRegs::ProcTexLutTable::Noise:
            fs_uniform_block_data.proctex_noise_lut_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::ColorMap:
            fs_uniform_block_data.proctex_color_map_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::AlphaMap:
            fs_uniform_block_data.proctex_alpha_map_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::Color:
            fs_uniform_block_data.proctex_lut_dirty = true;
            break;
        case TexturingRegs::ProcTexLutTable::ColorDiff:
            fs_uniform_block_data.proctex_diff_lut_dirty = true;
            break;
        }
        break;

    // Fragment operation mode
    case PICA_REG_INDEX(framebuffer.output_merger.fragment_operation_mode):
        shader_dirty = true;
        break;

    // Alpha test
    case PICA_REG_INDEX(framebuffer.output_merger.alpha_test):
        SyncAlphaTest();
        shader_dirty = true;
        break;

    case PICA_REG_INDEX(framebuffer.shadow):
        SyncShadowBias();
        break;

    // Scissor test
    case PICA_REG_INDEX(rasterizer.scissor_test.mode):
        shader_dirty = true;
        break;

    case PICA_REG_INDEX(texturing.main_config):
        shader_dirty = true;
        break;

    // Texture 0 type
    case PICA_REG_INDEX(texturing.texture0.type):
        shader_dirty = true;
        break;

    // TEV stages
    // (This also syncs fog_mode and fog_flip which are part of tev_combiner_buffer_input)
    case PICA_REG_INDEX(texturing.tev_stage0.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage0.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage0.color_op):
    case PICA_REG_INDEX(texturing.tev_stage0.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage1.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage1.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage1.color_op):
    case PICA_REG_INDEX(texturing.tev_stage1.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage2.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage2.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage2.color_op):
    case PICA_REG_INDEX(texturing.tev_stage2.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage3.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage3.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage3.color_op):
    case PICA_REG_INDEX(texturing.tev_stage3.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage4.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage4.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage4.color_op):
    case PICA_REG_INDEX(texturing.tev_stage4.color_scale):
    case PICA_REG_INDEX(texturing.tev_stage5.color_source1):
    case PICA_REG_INDEX(texturing.tev_stage5.color_modifier1):
    case PICA_REG_INDEX(texturing.tev_stage5.color_op):
    case PICA_REG_INDEX(texturing.tev_stage5.color_scale):
    case PICA_REG_INDEX(texturing.tev_combiner_buffer_input):
        shader_dirty = true;
        break;
    case PICA_REG_INDEX(texturing.tev_stage0.const_r):
        SyncTevConstColor(0, regs.texturing.tev_stage0);
        break;
    case PICA_REG_INDEX(texturing.tev_stage1.const_r):
        SyncTevConstColor(1, regs.texturing.tev_stage1);
        break;
    case PICA_REG_INDEX(texturing.tev_stage2.const_r):
        SyncTevConstColor(2, regs.texturing.tev_stage2);
        break;
    case PICA_REG_INDEX(texturing.tev_stage3.const_r):
        SyncTevConstColor(3, regs.texturing.tev_stage3);
        break;
    case PICA_REG_INDEX(texturing.tev_stage4.const_r):
        SyncTevConstColor(4, regs.texturing.tev_stage4);
        break;
    case PICA_REG_INDEX(texturing.tev_stage5.const_r):
        SyncTevConstColor(5, regs.texturing.tev_stage5);
        break;

    // TEV combiner buffer color
    case PICA_REG_INDEX(texturing.tev_combiner_buffer_color):
        SyncCombinerColor();
        break;

    // Fragment lighting switches
    case PICA_REG_INDEX(lighting.disable):
    case PICA_REG_INDEX(lighting.max_light_index):
    case PICA_REG_INDEX(lighting.config0):
    case PICA_REG_INDEX(lighting.config1):
    case PICA_REG_INDEX(lighting.abs_lut_input):
    case PICA_REG_INDEX(lighting.lut_input):
    case PICA_REG_INDEX(lighting.lut_scale):
    case PICA_REG_INDEX(lighting.light_enable):
        break;

    // Fragment lighting specular 0 color
    case PICA_REG_INDEX(lighting.light[0].specular_0):
        SyncLightSpecular0(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].specular_0):
        SyncLightSpecular0(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].specular_0):
        SyncLightSpecular0(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].specular_0):
        SyncLightSpecular0(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].specular_0):
        SyncLightSpecular0(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].specular_0):
        SyncLightSpecular0(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].specular_0):
        SyncLightSpecular0(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].specular_0):
        SyncLightSpecular0(7);
        break;

    // Fragment lighting specular 1 color
    case PICA_REG_INDEX(lighting.light[0].specular_1):
        SyncLightSpecular1(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].specular_1):
        SyncLightSpecular1(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].specular_1):
        SyncLightSpecular1(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].specular_1):
        SyncLightSpecular1(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].specular_1):
        SyncLightSpecular1(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].specular_1):
        SyncLightSpecular1(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].specular_1):
        SyncLightSpecular1(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].specular_1):
        SyncLightSpecular1(7);
        break;

    // Fragment lighting diffuse color
    case PICA_REG_INDEX(lighting.light[0].diffuse):
        SyncLightDiffuse(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].diffuse):
        SyncLightDiffuse(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].diffuse):
        SyncLightDiffuse(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].diffuse):
        SyncLightDiffuse(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].diffuse):
        SyncLightDiffuse(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].diffuse):
        SyncLightDiffuse(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].diffuse):
        SyncLightDiffuse(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].diffuse):
        SyncLightDiffuse(7);
        break;

    // Fragment lighting ambient color
    case PICA_REG_INDEX(lighting.light[0].ambient):
        SyncLightAmbient(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].ambient):
        SyncLightAmbient(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].ambient):
        SyncLightAmbient(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].ambient):
        SyncLightAmbient(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].ambient):
        SyncLightAmbient(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].ambient):
        SyncLightAmbient(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].ambient):
        SyncLightAmbient(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].ambient):
        SyncLightAmbient(7);
        break;

    // Fragment lighting position
    case PICA_REG_INDEX(lighting.light[0].x):
    case PICA_REG_INDEX(lighting.light[0].z):
        SyncLightPosition(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].x):
    case PICA_REG_INDEX(lighting.light[1].z):
        SyncLightPosition(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].x):
    case PICA_REG_INDEX(lighting.light[2].z):
        SyncLightPosition(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].x):
    case PICA_REG_INDEX(lighting.light[3].z):
        SyncLightPosition(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].x):
    case PICA_REG_INDEX(lighting.light[4].z):
        SyncLightPosition(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].x):
    case PICA_REG_INDEX(lighting.light[5].z):
        SyncLightPosition(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].x):
    case PICA_REG_INDEX(lighting.light[6].z):
        SyncLightPosition(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].x):
    case PICA_REG_INDEX(lighting.light[7].z):
        SyncLightPosition(7);
        break;

    // Fragment spot lighting direction
    case PICA_REG_INDEX(lighting.light[0].spot_x):
    case PICA_REG_INDEX(lighting.light[0].spot_z):
        SyncLightSpotDirection(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].spot_x):
    case PICA_REG_INDEX(lighting.light[1].spot_z):
        SyncLightSpotDirection(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].spot_x):
    case PICA_REG_INDEX(lighting.light[2].spot_z):
        SyncLightSpotDirection(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].spot_x):
    case PICA_REG_INDEX(lighting.light[3].spot_z):
        SyncLightSpotDirection(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].spot_x):
    case PICA_REG_INDEX(lighting.light[4].spot_z):
        SyncLightSpotDirection(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].spot_x):
    case PICA_REG_INDEX(lighting.light[5].spot_z):
        SyncLightSpotDirection(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].spot_x):
    case PICA_REG_INDEX(lighting.light[6].spot_z):
        SyncLightSpotDirection(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].spot_x):
    case PICA_REG_INDEX(lighting.light[7].spot_z):
        SyncLightSpotDirection(7);
        break;

    // Fragment lighting light source config
    case PICA_REG_INDEX(lighting.light[0].config):
    case PICA_REG_INDEX(lighting.light[1].config):
    case PICA_REG_INDEX(lighting.light[2].config):
    case PICA_REG_INDEX(lighting.light[3].config):
    case PICA_REG_INDEX(lighting.light[4].config):
    case PICA_REG_INDEX(lighting.light[5].config):
    case PICA_REG_INDEX(lighting.light[6].config):
    case PICA_REG_INDEX(lighting.light[7].config):
        shader_dirty = true;
        break;

    // Fragment lighting distance attenuation bias
    case PICA_REG_INDEX(lighting.light[0].dist_atten_bias):
        SyncLightDistanceAttenuationBias(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].dist_atten_bias):
        SyncLightDistanceAttenuationBias(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].dist_atten_bias):
        SyncLightDistanceAttenuationBias(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].dist_atten_bias):
        SyncLightDistanceAttenuationBias(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].dist_atten_bias):
        SyncLightDistanceAttenuationBias(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].dist_atten_bias):
        SyncLightDistanceAttenuationBias(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].dist_atten_bias):
        SyncLightDistanceAttenuationBias(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].dist_atten_bias):
        SyncLightDistanceAttenuationBias(7);
        break;

    // Fragment lighting distance attenuation scale
    case PICA_REG_INDEX(lighting.light[0].dist_atten_scale):
        SyncLightDistanceAttenuationScale(0);
        break;
    case PICA_REG_INDEX(lighting.light[1].dist_atten_scale):
        SyncLightDistanceAttenuationScale(1);
        break;
    case PICA_REG_INDEX(lighting.light[2].dist_atten_scale):
        SyncLightDistanceAttenuationScale(2);
        break;
    case PICA_REG_INDEX(lighting.light[3].dist_atten_scale):
        SyncLightDistanceAttenuationScale(3);
        break;
    case PICA_REG_INDEX(lighting.light[4].dist_atten_scale):
        SyncLightDistanceAttenuationScale(4);
        break;
    case PICA_REG_INDEX(lighting.light[5].dist_atten_scale):
        SyncLightDistanceAttenuationScale(5);
        break;
    case PICA_REG_INDEX(lighting.light[6].dist_atten_scale):
        SyncLightDistanceAttenuationScale(6);
        break;
    case PICA_REG_INDEX(lighting.light[7].dist_atten_scale):
        SyncLightDistanceAttenuationScale(7);
        break;

    // Fragment lighting global ambient color (emission + ambient * ambient)
    case PICA_REG_INDEX(lighting.global_ambient):
        SyncGlobalAmbient();
        break;

    // Fragment lighting lookup tables
    case PICA_REG_INDEX(lighting.lut_data[0]):
    case PICA_REG_INDEX(lighting.lut_data[1]):
    case PICA_REG_INDEX(lighting.lut_data[2]):
    case PICA_REG_INDEX(lighting.lut_data[3]):
    case PICA_REG_INDEX(lighting.lut_data[4]):
    case PICA_REG_INDEX(lighting.lut_data[5]):
    case PICA_REG_INDEX(lighting.lut_data[6]):
    case PICA_REG_INDEX(lighting.lut_data[7]): {
        const auto& lut_config = regs.lighting.lut_config;
        fs_uniform_block_data.lighting_lut_dirty[lut_config.type] = true;
        fs_uniform_block_data.lighting_lut_dirty_any = true;
        break;
    }

    // Texture LOD biases
    case PICA_REG_INDEX(texturing.texture0.lod.bias):
        SyncTextureLodBias(0);
        break;
    case PICA_REG_INDEX(texturing.texture1.lod.bias):
        SyncTextureLodBias(1);
        break;
    case PICA_REG_INDEX(texturing.texture2.lod.bias):
        SyncTextureLodBias(2);
        break;

    // Texture borders
    case PICA_REG_INDEX(texturing.texture0.border_color):
        SyncTextureBorderColor(0);
        break;
    case PICA_REG_INDEX(texturing.texture1.border_color):
        SyncTextureBorderColor(1);
        break;
    case PICA_REG_INDEX(texturing.texture2.border_color):
        SyncTextureBorderColor(2);
        break;

    // Clipping plane
    case PICA_REG_INDEX(rasterizer.clip_enable):
    case PICA_REG_INDEX(rasterizer.clip_coef[0]):
    case PICA_REG_INDEX(rasterizer.clip_coef[1]):
    case PICA_REG_INDEX(rasterizer.clip_coef[2]):
    case PICA_REG_INDEX(rasterizer.clip_coef[3]):
        SyncClipPlane();
        break;
    }

    // Forward registers that map to fixed function API features to the video backend
    NotifyFixedFunctionPicaRegisterChanged(id);
}

void RasterizerAccelerated::SyncDepthScale() {
    const f32 depth_scale = f24::FromRaw(regs.rasterizer.viewport_depth_range).ToFloat32();

    if (depth_scale != fs_uniform_block_data.data.depth_scale) {
        fs_uniform_block_data.data.depth_scale = depth_scale;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncDepthOffset() {
    const f32 depth_offset = f24::FromRaw(regs.rasterizer.viewport_depth_near_plane).ToFloat32();

    if (depth_offset != fs_uniform_block_data.data.depth_offset) {
        fs_uniform_block_data.data.depth_offset = depth_offset;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncFogColor() {
    const auto& fog_color_regs = regs.texturing.fog_color;
    const Common::Vec3f fog_color = {
        fog_color_regs.r.Value() / 255.0f,
        fog_color_regs.g.Value() / 255.0f,
        fog_color_regs.b.Value() / 255.0f,
    };

    if (fog_color != fs_uniform_block_data.data.fog_color) {
        fs_uniform_block_data.data.fog_color = fog_color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncProcTexNoise() {
    const Common::Vec2f proctex_noise_f = {
        Pica::f16::FromRaw(regs.texturing.proctex_noise_frequency.u).ToFloat32(),
        Pica::f16::FromRaw(regs.texturing.proctex_noise_frequency.v).ToFloat32(),
    };
    const Common::Vec2f proctex_noise_a = {
        regs.texturing.proctex_noise_u.amplitude / 4095.0f,
        regs.texturing.proctex_noise_v.amplitude / 4095.0f,
    };
    const Common::Vec2f proctex_noise_p = {
        Pica::f16::FromRaw(regs.texturing.proctex_noise_u.phase).ToFloat32(),
        Pica::f16::FromRaw(regs.texturing.proctex_noise_v.phase).ToFloat32(),
    };

    if (proctex_noise_f != fs_uniform_block_data.data.proctex_noise_f ||
        proctex_noise_a != fs_uniform_block_data.data.proctex_noise_a ||
        proctex_noise_p != fs_uniform_block_data.data.proctex_noise_p) {
        fs_uniform_block_data.data.proctex_noise_f = proctex_noise_f;
        fs_uniform_block_data.data.proctex_noise_a = proctex_noise_a;
        fs_uniform_block_data.data.proctex_noise_p = proctex_noise_p;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncProcTexBias() {
    const auto proctex_bias = Pica::f16::FromRaw(regs.texturing.proctex.bias_low |
                                                 (regs.texturing.proctex_lut.bias_high << 8))
                                  .ToFloat32();
    if (proctex_bias != fs_uniform_block_data.data.proctex_bias) {
        fs_uniform_block_data.data.proctex_bias = proctex_bias;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncAlphaTest() {
    if (regs.framebuffer.output_merger.alpha_test.ref !=
        static_cast<u32>(fs_uniform_block_data.data.alphatest_ref)) {
        fs_uniform_block_data.data.alphatest_ref = regs.framebuffer.output_merger.alpha_test.ref;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncCombinerColor() {
    const auto combiner_color = ColorRGBA8(regs.texturing.tev_combiner_buffer_color.raw);
    if (combiner_color != fs_uniform_block_data.data.tev_combiner_buffer_color) {
        fs_uniform_block_data.data.tev_combiner_buffer_color = combiner_color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncTevConstColor(
    const std::size_t stage_index, const Pica::TexturingRegs::TevStageConfig& tev_stage) {
    const auto const_color = ColorRGBA8(tev_stage.const_color);

    if (const_color == fs_uniform_block_data.data.const_color[stage_index]) {
        return;
    }

    fs_uniform_block_data.data.const_color[stage_index] = const_color;
    fs_uniform_block_data.dirty = true;
}

void RasterizerAccelerated::SyncGlobalAmbient() {
    const auto color = LightColor(regs.lighting.global_ambient);
    if (color != fs_uniform_block_data.data.lighting_global_ambient) {
        fs_uniform_block_data.data.lighting_global_ambient = color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightSpecular0(int light_index) {
    const auto color = LightColor(regs.lighting.light[light_index].specular_0);
    if (color != fs_uniform_block_data.data.light_src[light_index].specular_0) {
        fs_uniform_block_data.data.light_src[light_index].specular_0 = color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightSpecular1(int light_index) {
    const auto color = LightColor(regs.lighting.light[light_index].specular_1);
    if (color != fs_uniform_block_data.data.light_src[light_index].specular_1) {
        fs_uniform_block_data.data.light_src[light_index].specular_1 = color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightDiffuse(int light_index) {
    const auto color = LightColor(regs.lighting.light[light_index].diffuse);
    if (color != fs_uniform_block_data.data.light_src[light_index].diffuse) {
        fs_uniform_block_data.data.light_src[light_index].diffuse = color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightAmbient(int light_index) {
    const auto color = LightColor(regs.lighting.light[light_index].ambient);
    if (color != fs_uniform_block_data.data.light_src[light_index].ambient) {
        fs_uniform_block_data.data.light_src[light_index].ambient = color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightPosition(int light_index) {
    const Common::Vec3f position = {
        Pica::f16::FromRaw(regs.lighting.light[light_index].x).ToFloat32(),
        Pica::f16::FromRaw(regs.lighting.light[light_index].y).ToFloat32(),
        Pica::f16::FromRaw(regs.lighting.light[light_index].z).ToFloat32(),
    };

    if (position != fs_uniform_block_data.data.light_src[light_index].position) {
        fs_uniform_block_data.data.light_src[light_index].position = position;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightSpotDirection(int light_index) {
    const auto& light = regs.lighting.light[light_index];
    const auto spot_direction =
        Common::Vec3f{light.spot_x / 2047.0f, light.spot_y / 2047.0f, light.spot_z / 2047.0f};

    if (spot_direction != fs_uniform_block_data.data.light_src[light_index].spot_direction) {
        fs_uniform_block_data.data.light_src[light_index].spot_direction = spot_direction;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightDistanceAttenuationBias(int light_index) {
    const f32 dist_atten_bias =
        Pica::f20::FromRaw(regs.lighting.light[light_index].dist_atten_bias).ToFloat32();

    if (dist_atten_bias != fs_uniform_block_data.data.light_src[light_index].dist_atten_bias) {
        fs_uniform_block_data.data.light_src[light_index].dist_atten_bias = dist_atten_bias;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncLightDistanceAttenuationScale(int light_index) {
    const f32 dist_atten_scale =
        Pica::f20::FromRaw(regs.lighting.light[light_index].dist_atten_scale).ToFloat32();

    if (dist_atten_scale != fs_uniform_block_data.data.light_src[light_index].dist_atten_scale) {
        fs_uniform_block_data.data.light_src[light_index].dist_atten_scale = dist_atten_scale;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncShadowBias() {
    const auto& shadow = regs.framebuffer.shadow;
    const f32 constant = Pica::f16::FromRaw(shadow.constant).ToFloat32();
    const f32 linear = Pica::f16::FromRaw(shadow.linear).ToFloat32();

    if (constant != fs_uniform_block_data.data.shadow_bias_constant ||
        linear != fs_uniform_block_data.data.shadow_bias_linear) {
        fs_uniform_block_data.data.shadow_bias_constant = constant;
        fs_uniform_block_data.data.shadow_bias_linear = linear;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncShadowTextureBias() {
    const s32 bias = regs.texturing.shadow.bias << 1;
    if (bias != fs_uniform_block_data.data.shadow_texture_bias) {
        fs_uniform_block_data.data.shadow_texture_bias = bias;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncTextureLodBias(int tex_index) {
    const auto pica_textures = regs.texturing.GetTextures();
    const f32 bias = pica_textures[tex_index].config.lod.bias / 256.0f;
    if (bias != fs_uniform_block_data.data.tex_lod_bias[tex_index]) {
        fs_uniform_block_data.data.tex_lod_bias[tex_index] = bias;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncTextureBorderColor(int tex_index) {
    const auto pica_textures = regs.texturing.GetTextures();
    const auto params = pica_textures[tex_index].config;
    const Common::Vec4f border_color = ColorRGBA8(params.border_color.raw);
    if (border_color != fs_uniform_block_data.data.tex_border_color[tex_index]) {
        fs_uniform_block_data.data.tex_border_color[tex_index] = border_color;
        fs_uniform_block_data.dirty = true;
    }
}

void RasterizerAccelerated::SyncClipPlane() {
    const u32 enable_clip1 = regs.rasterizer.clip_enable != 0;
    const auto raw_clip_coef = regs.rasterizer.GetClipCoef();
    const Common::Vec4f new_clip_coef = {raw_clip_coef.x.ToFloat32(), raw_clip_coef.y.ToFloat32(),
                                         raw_clip_coef.z.ToFloat32(), raw_clip_coef.w.ToFloat32()};
    if (enable_clip1 != vs_uniform_block_data.data.enable_clip1 ||
        new_clip_coef != vs_uniform_block_data.data.clip_coef) {
        vs_uniform_block_data.data.enable_clip1 = enable_clip1;
        vs_uniform_block_data.data.clip_coef = new_clip_coef;
        vs_uniform_block_data.dirty = true;
    }
}

} // namespace VideoCore
