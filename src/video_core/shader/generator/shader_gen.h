// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/hash.h"

namespace Pica {
struct RegsInternal;
struct ShaderSetup;
} // namespace Pica

namespace Pica::Shader::Generator {

// NOTE: Changing the order impacts shader transferable and precompiled cache loading.
enum ProgramType : u32 {
    VS = 0,
    FS = 1,
    GS = 2,
};

enum Attributes {
    ATTRIBUTE_POSITION,
    ATTRIBUTE_COLOR,
    ATTRIBUTE_TEXCOORD0,
    ATTRIBUTE_TEXCOORD1,
    ATTRIBUTE_TEXCOORD2,
    ATTRIBUTE_TEXCOORD0_W,
    ATTRIBUTE_NORMQUAT,
    ATTRIBUTE_VIEW,
};

enum class AttribLoadFlags {
    Float = 1 << 0,
    Sint = 1 << 1,
    Uint = 1 << 2,
    ZeroW = 1 << 3,
};
DECLARE_ENUM_FLAG_OPERATORS(AttribLoadFlags)

/**
 * This struct contains common information to identify a GLSL geometry shader generated from
 * PICA geometry shader.
 */
struct PicaGSConfigState {
    void Init(const Pica::RegsInternal& regs, bool use_clip_planes_);

    bool use_clip_planes;

    u32 vs_output_attributes;
    u32 gs_output_attributes;

    struct SemanticMap {
        u32 attribute_index;
        u32 component_index;
    };

    // semantic_maps[semantic name] -> GS output attribute index + component index
    std::array<SemanticMap, 24> semantic_maps;
};

/**
 * This struct contains common information to identify a GLSL vertex shader generated from
 * PICA vertex shader.
 */
struct PicaVSConfigState {
    void Init(const Pica::RegsInternal& regs, Pica::ShaderSetup& setup, bool use_clip_planes_,
              bool use_geometry_shader_);

    bool use_clip_planes;
    bool use_geometry_shader;

    u64 program_hash;
    u64 swizzle_hash;
    u32 main_offset;
    bool sanitize_mul;

    u32 num_outputs;
    // Load operations to apply to the input vertex data
    std::array<AttribLoadFlags, 16> load_flags;

    // output_map[output register index] -> output attribute index
    std::array<u32, 16> output_map;

    PicaGSConfigState gs_state;
};

/**
 * This struct contains information to identify a GL vertex shader generated from PICA vertex
 * shader.
 */
struct PicaVSConfig : Common::HashableStruct<PicaVSConfigState> {
    explicit PicaVSConfig(const Pica::RegsInternal& regs, Pica::ShaderSetup& setup,
                          bool use_clip_planes_, bool use_geometry_shader_);
};

/**
 * This struct contains information to identify a GL geometry shader generated from PICA no-geometry
 * shader pipeline
 */
struct PicaFixedGSConfig : Common::HashableStruct<PicaGSConfigState> {
    explicit PicaFixedGSConfig(const Pica::RegsInternal& regs, bool use_clip_planes_);
};

} // namespace Pica::Shader::Generator

namespace std {
template <>
struct hash<Pica::Shader::Generator::PicaVSConfig> {
    std::size_t operator()(const Pica::Shader::Generator::PicaVSConfig& k) const noexcept {
        return k.Hash();
    }
};

template <>
struct hash<Pica::Shader::Generator::PicaFixedGSConfig> {
    std::size_t operator()(const Pica::Shader::Generator::PicaFixedGSConfig& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std
