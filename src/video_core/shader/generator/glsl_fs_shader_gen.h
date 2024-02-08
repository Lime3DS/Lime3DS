// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/shader/generator/pica_fs_config.h"

namespace Pica::Shader::Generator::GLSL {

class FragmentModule {
public:
    explicit FragmentModule(const FSConfig& config, const Profile& profile);
    ~FragmentModule();

    /// Emits GLSL source corresponding to the provided pica fragment configuration
    std::string Generate();

private:
    /// Undos the host perspective transformation and applies the PICA one
    void WriteDepth();

    /// Emits code to emulate the scissor rectangle
    void WriteScissor();

    /// Writes the code to emulate fragment lighting
    void WriteLighting();

    /// Writes the code to emulate fog
    void WriteFog();

    /// Writes the code to emulate gas rendering
    void WriteGas();

    /// Writes the code to emulate shadow-map rendering
    void WriteShadow();

    /// Writes the code to emulate logic ops in the fragment shader
    void WriteLogicOp();

    /// Writes the code to emulate PICA min/max blending factors
    void WriteBlending();

    /// Returns the specified TEV stage source component(s)
    std::string GetSource(Pica::TexturingRegs::TevStageConfig::Source source, u32 tev_index);

    /// Writes the color components to use for the specified TEV stage color modifier
    void AppendColorModifier(Pica::TexturingRegs::TevStageConfig::ColorModifier modifier,
                             Pica::TexturingRegs::TevStageConfig::Source source, u32 tev_index);

    /// Writes the alpha component to use for the specified TEV stage alpha modifier
    void AppendAlphaModifier(Pica::TexturingRegs::TevStageConfig::AlphaModifier modifier,
                             Pica::TexturingRegs::TevStageConfig::Source source, u32 tev_index);

    /// Writes the combiner function for the color components for the specified TEV stage operation
    void AppendColorCombiner(Pica::TexturingRegs::TevStageConfig::Operation operation);

    /// Writes the combiner function for the alpha component for the specified TEV stage operation
    void AppendAlphaCombiner(Pica::TexturingRegs::TevStageConfig::Operation operation);

    /// Writes the if-statement condition used to evaluate alpha testing
    void WriteAlphaTestCondition(Pica::FramebufferRegs::CompareFunc func);

    /// Writes the code to emulate the specified TEV stage
    void WriteTevStage(u32 index);

    void AppendProcTexShiftOffset(std::string_view v, Pica::TexturingRegs::ProcTexShift mode,
                                  Pica::TexturingRegs::ProcTexClamp clamp_mode);

    void AppendProcTexClamp(std::string_view var, Pica::TexturingRegs::ProcTexClamp mode);

    void AppendProcTexCombineAndMap(Pica::TexturingRegs::ProcTexCombiner combiner,
                                    std::string_view offset);

    void DefineExtensions();
    void DefineInterface();
    void DefineBindingsVK();
    void DefineBindingsGL();
    void DefineHelpers();
    void DefineLightingHelpers();
    void DefineShadowHelpers();
    void DefineProcTexSampler();
    void DefineTexUnitSampler(u32 i);

private:
    const FSConfig& config;
    const Profile& profile;
    std::string out;
    bool use_blend_fallback{};
    bool use_fragment_shader_interlock{};
    bool use_fragment_shader_barycentric{};
};

/**
 * Generates the GLSL fragment shader program source code for the current Pica state
 * @param config ShaderCacheKey object generated for the current Pica state, used for the shader
 *               configuration (NOTE: Use state in this struct only, not the Pica registers!)
 * @returns String of the shader source code
 */
std::string GenerateFragmentShader(const FSConfig& config, const Profile& profile);

} // namespace Pica::Shader::Generator::GLSL
