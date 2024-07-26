// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <sirit/sirit.h>

#include "video_core/pica/regs_framebuffer.h"
#include "video_core/pica/regs_texturing.h"

namespace Pica::Shader {
struct FSConfig;
struct Profile;
} // namespace Pica::Shader

namespace Pica::Shader::Generator::SPIRV {

using Sirit::Id;

struct VectorIds {
    /// Returns the type id of the vector with the provided size
    [[nodiscard]] constexpr Id Get(u32 size) const {
        return ids[size - 2];
    }

    std::array<Id, 3> ids;
};

class FragmentModule : public Sirit::Module {
    static constexpr u32 NUM_TEV_STAGES = 6;
    static constexpr u32 NUM_LIGHTS = 8;
    static constexpr u32 NUM_LIGHTING_SAMPLERS = 24;
    static constexpr u32 NUM_TEX_UNITS = 4;
    static constexpr u32 NUM_NON_PROC_TEX_UNITS = 3;

public:
    explicit FragmentModule(const FSConfig& config, const Profile& profile);
    ~FragmentModule();

    /// Emits SPIR-V bytecode corresponding to the provided pica fragment configuration
    void Generate();

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

    /// Writes the code to emulate the specified TEV stage
    void WriteTevStage(s32 index);

    /// Defines the basic texture sampling functions for a unit
    void DefineTexSampler(u32 texture_unit);

    /// Function for sampling the procedurally generated texture unit.
    Id ProcTexSampler();

    /// Writes the if-statement condition used to evaluate alpha testing.
    void WriteAlphaTestCondition(Pica::FramebufferRegs::CompareFunc func);

    /// Samples the current fragment texel from shadow plane
    [[nodiscard]] Id SampleShadow();

    [[nodiscard]] Id AppendProcTexShiftOffset(Id v, Pica::TexturingRegs::ProcTexShift mode,
                                              Pica::TexturingRegs::ProcTexClamp clamp_mode);

    [[nodiscard]] Id AppendProcTexClamp(Id var, Pica::TexturingRegs::ProcTexClamp mode);

    [[nodiscard]] Id AppendProcTexCombineAndMap(Pica::TexturingRegs::ProcTexCombiner combiner, Id u,
                                                Id v, Id offset);

    /// Rounds the provided variable to the nearest 1/255th
    [[nodiscard]] Id Byteround(Id variable_id, u32 size = 1);

    /// LUT sampling uitlity
    /// For NoiseLUT/ColorMap/AlphaMap, coord=0.0 is lut[0], coord=127.0/128.0 is lut[127] and
    /// coord=1.0 is lut[127]+lut_diff[127]. For other indices, the result is interpolated using
    /// value entries and difference entries.
    [[nodiscard]] Id ProcTexLookupLUT(Id offset, Id coord);

    /// Generates random noise with proctex
    [[nodiscard]] Id ProcTexNoiseCoef(Id x);

    /// Samples a color value from the rgba texture lut
    [[nodiscard]] Id SampleProcTexColor(Id lut_coord, Id level);

    /// Lookups the lighting LUT at the provided lut_index
    [[nodiscard]] Id LookupLightingLUT(Id lut_index, Id index, Id delta);

    /// Returns the specified TEV stage source component(s)
    [[nodiscard]] Id GetSource(Pica::TexturingRegs::TevStageConfig::Source source, s32 index);

    /// Writes the color components to use for the specified TEV stage color modifier
    [[nodiscard]] Id AppendColorModifier(
        Pica::TexturingRegs::TevStageConfig::ColorModifier modifier,
        Pica::TexturingRegs::TevStageConfig::Source source, s32 tev_index);

    /// Writes the alpha component to use for the specified TEV stage alpha modifier
    [[nodiscard]] Id AppendAlphaModifier(
        Pica::TexturingRegs::TevStageConfig::AlphaModifier modifier,
        Pica::TexturingRegs::TevStageConfig::Source source, s32 index);

    /// Writes the combiner function for the color components for the specified TEV stage operation
    [[nodiscard]] Id AppendColorCombiner(Pica::TexturingRegs::TevStageConfig::Operation operation);

    /// Writes the combiner function for the alpha component for the specified TEV stage operation
    [[nodiscard]] Id AppendAlphaCombiner(Pica::TexturingRegs::TevStageConfig::Operation operation);

private:
    /// Creates a constant array of integers
    template <typename... T>
    void InitTableS32(Id table, T... elems) {
        const Id table_const{ConstS32(elems...)};
        OpStore(table, table_const);
    };

    /// Loads the member specified from the shader_data uniform struct
    template <typename... Ids>
    [[nodiscard]] Id GetShaderDataMember(Id type, Ids... ids) {
        const Id uniform_ptr{TypePointer(spv::StorageClass::Uniform, type)};
        return OpLoad(type, OpAccessChain(uniform_ptr, shader_data_id, ids...));
    }

    /// Pads the provided vector by inserting args at the end
    template <typename... Args>
    [[nodiscard]] Id PadVectorF32(Id vector, Id pad_type_id, Args&&... args) {
        return OpCompositeConstruct(pad_type_id, vector, ConstF32(args...));
    }

    /// Defines a input variable
    [[nodiscard]] Id DefineInput(Id type, u32 location) {
        const Id input_id{DefineVar(type, spv::StorageClass::Input)};
        Decorate(input_id, spv::Decoration::Location, location);
        return input_id;
    }

    /// Defines a input variable
    [[nodiscard]] Id DefineOutput(Id type, u32 location) {
        const Id output_id{DefineVar(type, spv::StorageClass::Output)};
        Decorate(output_id, spv::Decoration::Location, location);
        return output_id;
    }

    /// Defines a uniform constant variable
    [[nodiscard]] Id DefineUniformConst(Id type, u32 set, u32 binding, bool readonly = false) {
        const Id uniform_id{DefineVar(type, spv::StorageClass::UniformConstant)};
        Decorate(uniform_id, spv::Decoration::DescriptorSet, set);
        Decorate(uniform_id, spv::Decoration::Binding, binding);
        if (readonly) {
            Decorate(uniform_id, spv::Decoration::NonWritable);
        }
        return uniform_id;
    }

    template <bool global = true>
    [[nodiscard]] Id DefineVar(Id type, spv::StorageClass storage_class) {
        const Id pointer_type_id{TypePointer(storage_class, type)};
        return global ? AddGlobalVariable(pointer_type_id, storage_class)
                      : AddLocalVariable(pointer_type_id, storage_class);
    }

    /// Returns the id of a signed integer constant of value
    [[nodiscard]] Id ConstU32(u32 value) {
        return Constant(u32_id, value);
    }

    template <typename... Args>
    [[nodiscard]] Id ConstU32(Args&&... values) {
        constexpr u32 size = static_cast<u32>(sizeof...(values));
        static_assert(size >= 2);
        const std::array constituents{Constant(u32_id, values)...};
        const Id type = size <= 4 ? uvec_ids.Get(size) : TypeArray(u32_id, ConstU32(size));
        return ConstantComposite(type, constituents);
    }

    /// Returns the id of a signed integer constant of value
    [[nodiscard]] Id ConstS32(s32 value) {
        return Constant(i32_id, value);
    }

    template <typename... Args>
    [[nodiscard]] Id ConstS32(Args&&... values) {
        constexpr u32 size = static_cast<u32>(sizeof...(values));
        static_assert(size >= 2);
        const std::array constituents{Constant(i32_id, values)...};
        const Id type = size <= 4 ? ivec_ids.Get(size) : TypeArray(i32_id, ConstU32(size));
        return ConstantComposite(type, constituents);
    }

    /// Returns the id of a float constant of value
    [[nodiscard]] Id ConstF32(f32 value) {
        return Constant(f32_id, value);
    }

    template <typename... Args>
    [[nodiscard]] Id ConstF32(Args... values) {
        constexpr u32 size = static_cast<u32>(sizeof...(values));
        static_assert(size >= 2);
        const std::array constituents{Constant(f32_id, values)...};
        const Id type = size <= 4 ? vec_ids.Get(size) : TypeArray(f32_id, ConstU32(size));
        return ConstantComposite(type, constituents);
    }

    void DefineArithmeticTypes();
    void DefineEntryPoint();
    void DefineUniformStructs();
    void DefineInterface();
    Id CompareShadow(Id pixel, Id z);

private:
    const FSConfig& config;
    const Profile& profile;

    bool use_fragment_shader_barycentric{};

    Id void_id{};
    Id bool_id{};
    Id f32_id{};
    Id i32_id{};
    Id u32_id{};

    VectorIds vec_ids{};
    VectorIds ivec_ids{};
    VectorIds uvec_ids{};
    VectorIds bvec_ids{};

    Id image2d_id{};
    Id image_cube_id{};
    Id image_buffer_id{};
    Id image_r32_id{};
    Id sampler_id{};
    Id shader_data_id{};

    Id primary_color_id{};
    Id texcoord_id[NUM_NON_PROC_TEX_UNITS]{};
    Id texcoord0_w_id{};
    Id normquat_id{};
    Id view_id{};
    Id color_id{};

    Id gl_frag_coord_id{};
    Id gl_frag_depth_id{};
    Id gl_bary_coord_id{};
    Id depth{};

    Id tex0_id{};
    Id tex1_id{};
    Id tex2_id{};
    Id texture_buffer_lut_lf_id{};
    Id texture_buffer_lut_rg_id{};
    Id texture_buffer_lut_rgba_id{};
    Id shadow_texture_px_id{};

    Id texture_buffer_lut_lf{};
    Id texture_buffer_lut_rg{};
    Id texture_buffer_lut_rgba{};

    Id rounded_primary_color{};
    Id primary_fragment_color{};
    Id secondary_fragment_color{};
    Id combiner_buffer{};
    Id next_combiner_buffer{};
    Id combiner_output{};

    Id color_results_1{};
    Id color_results_2{};
    Id color_results_3{};
    Id alpha_results_1{};
    Id alpha_results_2{};
    Id alpha_results_3{};

    Id sample_tex_unit_func[NUM_TEX_UNITS]{};
    Id noise1d_table{};
    Id noise2d_table{};
    Id lut_offsets{};
};

/**
 * Generates the SPIR-V fragment shader program source code for the current Pica state
 * @param config ShaderCacheKey object generated for the current Pica state, used for the shader
 *               configuration (NOTE: Use state in this struct only, not the Pica registers!)
 * @param separable_shader generates shader that can be used for separate shader object
 * @returns String of the shader source code
 */
std::vector<u32> GenerateFragmentShader(const FSConfig& config, const Profile& profile);

} // namespace Pica::Shader::Generator::SPIRV
