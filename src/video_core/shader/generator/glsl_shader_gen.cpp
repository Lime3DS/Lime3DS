// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>
#include <fmt/format.h>

#include "video_core/pica/regs_rasterizer.h"
#include "video_core/shader/generator/glsl_shader_decompiler.h"
#include "video_core/shader/generator/glsl_shader_gen.h"
#include "video_core/shader/generator/shader_gen.h"

using VSOutputAttributes = Pica::RasterizerRegs::VSOutputAttributes;

namespace Pica::Shader::Generator::GLSL {

constexpr std::string_view VSPicaUniformBlockDef = R"(
struct pica_uniforms {
    bool b[16];
    uvec4 i[4];
    vec4 f[96];
};

#ifdef VULKAN
layout (set = 0, binding = 0, std140) uniform vs_pica_data {
#else
layout (binding = 0, std140) uniform vs_pica_data {
#endif
    pica_uniforms uniforms;
};
)";

constexpr std::string_view VSUniformBlockDef = R"(
#ifdef VULKAN
layout (set = 0, binding = 1, std140) uniform vs_data {
#else
layout (binding = 1, std140) uniform vs_data {
#endif
    bool enable_clip1;
    vec4 clip_coef;
};

const vec2 EPSILON_Z = vec2(0.000001f, -1.00001f);

vec4 SanitizeVertex(vec4 vtx_pos) {
    float ndc_z = vtx_pos.z / vtx_pos.w;
    if (ndc_z > 0.f && ndc_z < EPSILON_Z[0]) {
        vtx_pos.z = 0.f;
    }
    if (ndc_z < -1.f && ndc_z > EPSILON_Z[1]) {
        vtx_pos.z = -vtx_pos.w;
    }
    return vtx_pos;
}
)";

static std::string GetVertexInterfaceDeclaration(bool is_output, bool use_clip_planes,
                                                 bool separable_shader) {
    std::string out;

    const auto append_variable = [&](std::string_view var, int location) {
        if (separable_shader) {
            out += fmt::format("layout (location={}) ", location);
        }
        out += fmt::format("{}{};\n", is_output ? "out " : "in ", var);
    };

    append_variable("vec4 primary_color", ATTRIBUTE_COLOR);
    append_variable("vec2 texcoord0", ATTRIBUTE_TEXCOORD0);
    append_variable("vec2 texcoord1", ATTRIBUTE_TEXCOORD1);
    append_variable("vec2 texcoord2", ATTRIBUTE_TEXCOORD2);
    append_variable("float texcoord0_w", ATTRIBUTE_TEXCOORD0_W);
    append_variable("vec4 normquat", ATTRIBUTE_NORMQUAT);
    append_variable("vec3 view", ATTRIBUTE_VIEW);

    if (is_output && separable_shader) {
        // gl_PerVertex redeclaration is required for separate shader object
        out += "out gl_PerVertex {\n";
        // Apple Silicon GPU drivers optimize more aggressively, which can create
        // too much variance and cause visual artifacting in games like Pokemon.
#ifdef __APPLE__
        out += "    invariant vec4 gl_Position;\n";
#else
        out += "    vec4 gl_Position;\n";
#endif
        if (use_clip_planes) {
            out += "    float gl_ClipDistance[2];\n";
        }
        out += "};\n";
    }

    return out;
}

std::string GenerateTrivialVertexShader(bool use_clip_planes, bool separable_shader) {
    std::string out;
    if (separable_shader) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n";
    }

    out +=
        fmt::format("layout(location = {}) in vec4 vert_position;\n"
                    "layout(location = {}) in vec4 vert_color;\n"
                    "layout(location = {}) in vec2 vert_texcoord0;\n"
                    "layout(location = {}) in vec2 vert_texcoord1;\n"
                    "layout(location = {}) in vec2 vert_texcoord2;\n"
                    "layout(location = {}) in float vert_texcoord0_w;\n"
                    "layout(location = {}) in vec4 vert_normquat;\n"
                    "layout(location = {}) in vec3 vert_view;\n",
                    ATTRIBUTE_POSITION, ATTRIBUTE_COLOR, ATTRIBUTE_TEXCOORD0, ATTRIBUTE_TEXCOORD1,
                    ATTRIBUTE_TEXCOORD2, ATTRIBUTE_TEXCOORD0_W, ATTRIBUTE_NORMQUAT, ATTRIBUTE_VIEW);

    out += GetVertexInterfaceDeclaration(true, use_clip_planes, separable_shader);
    out += VSUniformBlockDef;

    out += R"(
void main() {
    primary_color = vert_color;
    texcoord0 = vert_texcoord0;
    texcoord1 = vert_texcoord1;
    texcoord2 = vert_texcoord2;
    texcoord0_w = vert_texcoord0_w;
    normquat = vert_normquat;
    view = vert_view;
    vec4 vtx_pos = SanitizeVertex(vert_position);
    gl_Position = vec4(vtx_pos.x, vtx_pos.y, -vtx_pos.z, vtx_pos.w);
)";
    if (use_clip_planes) {
        out += R"(
        gl_ClipDistance[0] = -vtx_pos.z; // fixed PICA clipping plane z <= 0
        if (enable_clip1) {
            gl_ClipDistance[1] = dot(clip_coef, vtx_pos);
        } else {
            gl_ClipDistance[1] = 0.0;
        }
        )";
    }

    out += "}\n";

    return out;
}

std::string_view MakeLoadPrefix(AttribLoadFlags flag) {
    if (True(flag & AttribLoadFlags::Float)) {
        return "";
    } else if (True(flag & AttribLoadFlags::Sint)) {
        return "i";
    } else if (True(flag & AttribLoadFlags::Uint)) {
        return "u";
    }
    return "";
}

std::string GenerateVertexShader(const ShaderSetup& setup, const PicaVSConfig& config,
                                 bool separable_shader) {
    std::string out;
    if (separable_shader) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n";
    }

    out += VSPicaUniformBlockDef;
    out += VSUniformBlockDef;

    std::array<bool, 16> used_regs{};
    const auto get_input_reg = [&used_regs](u32 reg) {
        ASSERT(reg < 16);
        used_regs[reg] = true;
        return fmt::format("vs_in_reg{}", reg);
    };

    const auto get_output_reg = [&](u32 reg) -> std::string {
        ASSERT(reg < 16);
        if (config.state.output_map[reg] < config.state.num_outputs) {
            return fmt::format("vs_out_attr{}", config.state.output_map[reg]);
        }
        return "";
    };

    auto program_source =
        DecompileProgram(setup.program_code, setup.swizzle_data, config.state.main_offset,
                         get_input_reg, get_output_reg, config.state.sanitize_mul);

    if (program_source.empty()) {
        return "";
    }

    // input attributes declaration
    for (std::size_t i = 0; i < used_regs.size(); ++i) {
        if (used_regs[i]) {
            const auto flags = config.state.load_flags[i];
            const std::string_view prefix = MakeLoadPrefix(flags);
            out +=
                fmt::format("layout(location = {0}) in {1}vec4 vs_in_typed_reg{0};\n", i, prefix);
            out += fmt::format("vec4 vs_in_reg{0};\n", i);
        }
    }
    out += '\n';

    if (config.state.use_geometry_shader) {
        // output attributes declaration
        for (u32 i = 0; i < config.state.num_outputs; ++i) {
            if (separable_shader) {
                out += fmt::format("layout(location = {}) ", i);
            }
            out += fmt::format("out vec4 vs_out_attr{};\n", i);
        }
        out += "void EmitVtx() {}\n";
    } else {
        out += GetVertexInterfaceDeclaration(true, config.state.use_clip_planes, separable_shader);

        // output attributes declaration
        for (u32 i = 0; i < config.state.num_outputs; ++i) {
            out += fmt::format("vec4 vs_out_attr{};\n", i);
        }

        const auto semantic =
            [&state = config.state](VSOutputAttributes::Semantic slot_semantic) -> std::string {
            const u32 slot = static_cast<u32>(slot_semantic);
            const u32 attrib = state.gs_state.semantic_maps[slot].attribute_index;
            const u32 comp = state.gs_state.semantic_maps[slot].component_index;
            if (attrib < state.gs_state.gs_output_attributes) {
                return fmt::format("vs_out_attr{}.{}", attrib, "xyzw"[comp]);
            }
            return "1.0";
        };

        out += "vec4 GetVertexQuaternion() {\n";
        out += "    return vec4(" + semantic(VSOutputAttributes::QUATERNION_X) + ", " +
               semantic(VSOutputAttributes::QUATERNION_Y) + ", " +
               semantic(VSOutputAttributes::QUATERNION_Z) + ", " +
               semantic(VSOutputAttributes::QUATERNION_W) + ");\n";
        out += "}\n\n";

        out += "void EmitVtx() {\n";
        out += "    vec4 vtx_pos = vec4(" + semantic(VSOutputAttributes::POSITION_X) + ", " +
               semantic(VSOutputAttributes::POSITION_Y) + ", " +
               semantic(VSOutputAttributes::POSITION_Z) + ", " +
               semantic(VSOutputAttributes::POSITION_W) + ");\n";
        out += "    vtx_pos = SanitizeVertex(vtx_pos);\n";
        out += "    gl_Position = vec4(vtx_pos.x, vtx_pos.y, -vtx_pos.z, vtx_pos.w);\n";
        if (config.state.use_clip_planes) {
            out += "    gl_ClipDistance[0] = -vtx_pos.z;\n"; // fixed PICA clipping plane z <= 0
            out += "    if (enable_clip1) {\n";
            out += "        gl_ClipDistance[1] = dot(clip_coef, vtx_pos);\n";
            out += "    } else {\n";
            out += "        gl_ClipDistance[1] = 0.0;\n";
            out += "    }\n\n";
        }

        out += "    normquat = GetVertexQuaternion();\n";
        out += "    vec4 vtx_color = vec4(" + semantic(VSOutputAttributes::COLOR_R) + ", " +
               semantic(VSOutputAttributes::COLOR_G) + ", " +
               semantic(VSOutputAttributes::COLOR_B) + ", " +
               semantic(VSOutputAttributes::COLOR_A) + ");\n";
        out += "    primary_color = min(abs(vtx_color), vec4(1.0));\n\n";

        out += "    texcoord0 = vec2(" + semantic(VSOutputAttributes::TEXCOORD0_U) + ", " +
               semantic(VSOutputAttributes::TEXCOORD0_V) + ");\n";
        out += "    texcoord1 = vec2(" + semantic(VSOutputAttributes::TEXCOORD1_U) + ", " +
               semantic(VSOutputAttributes::TEXCOORD1_V) + ");\n\n";

        out += "    texcoord0_w = " + semantic(VSOutputAttributes::TEXCOORD0_W) + ";\n";
        out += "    view = vec3(" + semantic(VSOutputAttributes::VIEW_X) + ", " +
               semantic(VSOutputAttributes::VIEW_Y) + ", " + semantic(VSOutputAttributes::VIEW_Z) +
               ");\n\n";

        out += "    texcoord2 = vec2(" + semantic(VSOutputAttributes::TEXCOORD2_U) + ", " +
               semantic(VSOutputAttributes::TEXCOORD2_V) + ");\n\n";
        out += "}\n";
    }

    out += "bool exec_shader();\n\n";

    out += "\nvoid main() {\n";
    for (std::size_t i = 0; i < used_regs.size(); ++i) {
        if (used_regs[i]) {
            out += fmt::format("vs_in_reg{0} = vec4(vs_in_typed_reg{0});\n", i);
            if (True(config.state.load_flags[i] & AttribLoadFlags::ZeroW)) {
                out += fmt::format("vs_in_reg{0}.w = 0;\n", i);
            }
        }
    }
    for (u32 i = 0; i < config.state.num_outputs; ++i) {
        out += fmt::format("    vs_out_attr{} = vec4(0.0, 0.0, 0.0, 1.0);\n", i);
    }
    out += "\n    exec_shader();\n    EmitVtx();\n}\n\n";

    out += program_source;

    return out;
}

static std::string GetGSCommonSource(const PicaGSConfigState& state, bool separable_shader) {
    std::string out = GetVertexInterfaceDeclaration(true, state.use_clip_planes, separable_shader);
    out += VSUniformBlockDef;

    out += '\n';
    for (u32 i = 0; i < state.vs_output_attributes; ++i) {
        if (separable_shader) {
            out += fmt::format("layout(location = {}) ", i);
        }
        out += fmt::format("in vec4 vs_out_attr{}[];\n", i);
    }

    out += R"(
struct Vertex {
)";
    out += fmt::format("    vec4 attributes[{}];\n", state.gs_output_attributes);
    out += "};\n\n";

    const auto semantic = [&state](VSOutputAttributes::Semantic slot_semantic) -> std::string {
        const u32 slot = static_cast<u32>(slot_semantic);
        const u32 attrib = state.semantic_maps[slot].attribute_index;
        const u32 comp = state.semantic_maps[slot].component_index;
        if (attrib < state.gs_output_attributes) {
            return fmt::format("vtx.attributes[{}].{}", attrib, "xyzw"[comp]);
        }
        return "1.0";
    };

    out += "vec4 GetVertexQuaternion(Vertex vtx) {\n";
    out += "    return vec4(" + semantic(VSOutputAttributes::QUATERNION_X) + ", " +
           semantic(VSOutputAttributes::QUATERNION_Y) + ", " +
           semantic(VSOutputAttributes::QUATERNION_Z) + ", " +
           semantic(VSOutputAttributes::QUATERNION_W) + ");\n";
    out += "}\n\n";

    out += "void EmitVtx(Vertex vtx, bool quats_opposite) {\n";
    out += "    vec4 vtx_pos = vec4(" + semantic(VSOutputAttributes::POSITION_X) + ", " +
           semantic(VSOutputAttributes::POSITION_Y) + ", " +
           semantic(VSOutputAttributes::POSITION_Z) + ", " +
           semantic(VSOutputAttributes::POSITION_W) + ");\n";
    out += "    vtx_pos = SanitizeVertex(vtx_pos);\n";
    out += "    gl_Position = vec4(vtx_pos.x, vtx_pos.y, -vtx_pos.z, vtx_pos.w);\n";
    if (state.use_clip_planes) {
        out += "    gl_ClipDistance[0] = -vtx_pos.z;\n"; // fixed PICA clipping plane z <= 0
        out += "    if (enable_clip1) {\n";
        out += "        gl_ClipDistance[1] = dot(clip_coef, vtx_pos);\n";
        out += "    } else {\n";
        out += "        gl_ClipDistance[1] = 0.0;\n";
        out += "    }\n\n";
    }

    out += "    vec4 vtx_quat = GetVertexQuaternion(vtx);\n";
    out += "    normquat = mix(vtx_quat, -vtx_quat, bvec4(quats_opposite));\n\n";

    out += "    vec4 vtx_color = vec4(" + semantic(VSOutputAttributes::COLOR_R) + ", " +
           semantic(VSOutputAttributes::COLOR_G) + ", " + semantic(VSOutputAttributes::COLOR_B) +
           ", " + semantic(VSOutputAttributes::COLOR_A) + ");\n";
    out += "    primary_color = min(abs(vtx_color), vec4(1.0));\n\n";

    out += "    texcoord0 = vec2(" + semantic(VSOutputAttributes::TEXCOORD0_U) + ", " +
           semantic(VSOutputAttributes::TEXCOORD0_V) + ");\n";
    out += "    texcoord1 = vec2(" + semantic(VSOutputAttributes::TEXCOORD1_U) + ", " +
           semantic(VSOutputAttributes::TEXCOORD1_V) + ");\n\n";

    out += "    texcoord0_w = " + semantic(VSOutputAttributes::TEXCOORD0_W) + ";\n";
    out += "    view = vec3(" + semantic(VSOutputAttributes::VIEW_X) + ", " +
           semantic(VSOutputAttributes::VIEW_Y) + ", " + semantic(VSOutputAttributes::VIEW_Z) +
           ");\n\n";

    out += "    texcoord2 = vec2(" + semantic(VSOutputAttributes::TEXCOORD2_U) + ", " +
           semantic(VSOutputAttributes::TEXCOORD2_V) + ");\n\n";

    out += "    EmitVertex();\n";
    out += "}\n";

    out += R"(
bool AreQuaternionsOpposite(vec4 qa, vec4 qb) {
    return (dot(qa, qb) < 0.0);
}

void EmitPrim(Vertex vtx0, Vertex vtx1, Vertex vtx2) {
    EmitVtx(vtx0, false);
    EmitVtx(vtx1, AreQuaternionsOpposite(GetVertexQuaternion(vtx0), GetVertexQuaternion(vtx1)));
    EmitVtx(vtx2, AreQuaternionsOpposite(GetVertexQuaternion(vtx0), GetVertexQuaternion(vtx2)));
    EndPrimitive();
}
)";

    return out;
};

std::string GenerateFixedGeometryShader(const PicaFixedGSConfig& config, bool separable_shader) {
    std::string out;
    if (separable_shader) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n";
    }

    out += R"(
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

)";

    out += GetGSCommonSource(config.state, separable_shader);

    out += R"(
void main() {
    Vertex prim_buffer[3];
)";
    for (u32 vtx = 0; vtx < 3; ++vtx) {
        out += fmt::format("    prim_buffer[{}].attributes = vec4[{}](", vtx,
                           config.state.gs_output_attributes);
        for (u32 i = 0; i < config.state.vs_output_attributes; ++i) {
            out += fmt::format("{}vs_out_attr{}[{}]", i == 0 ? "" : ", ", i, vtx);
        }
        out += ");\n";
    }
    out += "    EmitPrim(prim_buffer[0], prim_buffer[1], prim_buffer[2]);\n";
    out += "}\n";

    return out;
}
} // namespace Pica::Shader::Generator::GLSL
