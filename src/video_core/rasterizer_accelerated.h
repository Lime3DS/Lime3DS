// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/vector_math.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/shader/generator/pica_fs_config.h"
#include "video_core/shader/generator/shader_uniforms.h"

namespace Memory {
class MemorySystem;
}

namespace Pica {
class PicaCore;
}

namespace VideoCore {

class RasterizerAccelerated : public RasterizerInterface {
public:
    explicit RasterizerAccelerated(Memory::MemorySystem& memory, Pica::PicaCore& pica);
    virtual ~RasterizerAccelerated() = default;

    void AddTriangle(const Pica::OutputVertex& v0, const Pica::OutputVertex& v1,
                     const Pica::OutputVertex& v2) override;

    void NotifyPicaRegisterChanged(u32 id) override;

    void SyncEntireState() override;

protected:
    /// Sync fixed-function pipeline state
    virtual void SyncFixedState() = 0;

    /// Notifies that a fixed function PICA register changed to the video backend
    virtual void NotifyFixedFunctionPicaRegisterChanged(u32 id) = 0;

    /// Syncs the depth scale to match the PICA register
    void SyncDepthScale();

    /// Syncs the depth offset to match the PICA register
    void SyncDepthOffset();

    /// Syncs the fog states to match the PICA register
    void SyncFogColor();

    /// Sync the procedural texture noise configuration to match the PICA register
    void SyncProcTexNoise();

    /// Sync the procedural texture bias configuration to match the PICA register
    void SyncProcTexBias();

    /// Syncs the alpha test states to match the PICA register
    void SyncAlphaTest();

    /// Syncs the TEV combiner color buffer to match the PICA register
    void SyncCombinerColor();

    /// Syncs the TEV constant color to match the PICA register
    void SyncTevConstColor(std::size_t tev_index,
                           const Pica::TexturingRegs::TevStageConfig& tev_stage);

    /// Syncs the lighting global ambient color to match the PICA register
    void SyncGlobalAmbient();

    /// Syncs the specified light's specular 0 color to match the PICA register
    void SyncLightSpecular0(int light_index);

    /// Syncs the specified light's specular 1 color to match the PICA register
    void SyncLightSpecular1(int light_index);

    /// Syncs the specified light's diffuse color to match the PICA register
    void SyncLightDiffuse(int light_index);

    /// Syncs the specified light's ambient color to match the PICA register
    void SyncLightAmbient(int light_index);

    /// Syncs the specified light's position to match the PICA register
    void SyncLightPosition(int light_index);

    /// Syncs the specified spot light direcition to match the PICA register
    void SyncLightSpotDirection(int light_index);

    /// Syncs the specified light's distance attenuation bias to match the PICA register
    void SyncLightDistanceAttenuationBias(int light_index);

    /// Syncs the specified light's distance attenuation scale to match the PICA register
    void SyncLightDistanceAttenuationScale(int light_index);

    /// Syncs the shadow rendering bias to match the PICA register
    void SyncShadowBias();

    /// Syncs the shadow texture bias to match the PICA register
    void SyncShadowTextureBias();

    /// Syncs the texture LOD bias to match the PICA register
    void SyncTextureLodBias(int tex_index);

    /// Syncs the texture border color to match the PICA registers
    void SyncTextureBorderColor(int tex_index);

    /// Syncs the clip plane state to match the PICA register
    void SyncClipPlane();

protected:
    /// Structure that keeps tracks of the vertex shader uniform state
    struct VSUniformBlockData {
        Pica::Shader::Generator::VSUniformData data{};
        bool dirty = true;
    };

    /// Structure that keeps tracks of the fragment shader uniform state
    struct FSUniformBlockData {
        Pica::Shader::Generator::FSUniformData data{};
        std::array<bool, Pica::LightingRegs::NumLightingSampler> lighting_lut_dirty{};
        bool lighting_lut_dirty_any = true;
        bool fog_lut_dirty = true;
        bool proctex_noise_lut_dirty = true;
        bool proctex_color_map_dirty = true;
        bool proctex_alpha_map_dirty = true;
        bool proctex_lut_dirty = true;
        bool proctex_diff_lut_dirty = true;
        bool dirty = true;
    };

    /// Structure that the hardware rendered vertices are composed of
    struct HardwareVertex {
        HardwareVertex() = default;
        HardwareVertex(const Pica::OutputVertex& v, bool flip_quaternion);

        Common::Vec4f position;
        Common::Vec4f color;
        Common::Vec2f tex_coord0;
        Common::Vec2f tex_coord1;
        Common::Vec2f tex_coord2;
        float tex_coord0_w;
        Common::Vec4f normquat;
        Common::Vec3f view;
    };

    struct VertexArrayInfo {
        u32 vs_input_index_min;
        u32 vs_input_index_max;
        u32 vs_input_size;
    };

    /// Retrieve the range and the size of the input vertex
    VertexArrayInfo AnalyzeVertexArray(bool is_indexed, u32 stride_alignment = 1);

protected:
    Memory::MemorySystem& memory;
    Pica::PicaCore& pica;
    Pica::RegsInternal& regs;

    std::vector<HardwareVertex> vertex_batch;
    Pica::Shader::UserConfig user_config{};
    bool shader_dirty = true;

    VSUniformBlockData vs_uniform_block_data{};
    FSUniformBlockData fs_uniform_block_data{};
    using LightLUT = std::array<Common::Vec2f, 256>;
    std::array<LightLUT, Pica::LightingRegs::NumLightingSampler> lighting_lut_data{};
    std::array<Common::Vec2f, 128> fog_lut_data{};
    std::array<Common::Vec2f, 128> proctex_noise_lut_data{};
    std::array<Common::Vec2f, 128> proctex_color_map_data{};
    std::array<Common::Vec2f, 128> proctex_alpha_map_data{};
    std::array<Common::Vec4f, 256> proctex_lut_data{};
    std::array<Common::Vec4f, 256> proctex_diff_lut_data{};
};

} // namespace VideoCore
