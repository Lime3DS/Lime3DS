// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/shader/generator/glsl_fs_shader_gen.h"

namespace Pica::Shader::Generator::GLSL {

using ProcTexClamp = TexturingRegs::ProcTexClamp;
using ProcTexShift = TexturingRegs::ProcTexShift;
using ProcTexCombiner = TexturingRegs::ProcTexCombiner;
using ProcTexFilter = TexturingRegs::ProcTexFilter;
using TextureType = Pica::TexturingRegs::TextureConfig::TextureType;

constexpr static std::size_t RESERVE_SIZE = 8 * 1024 * 1024;

enum class Semantic : u32 {
    Position,
    Color,
    Texcoord0,
    Texcoord1,
    Texcoord2,
    Texcoord0_W,
    Normquat,
    View,
};

static bool IsPassThroughTevStage(const Pica::TexturingRegs::TevStageConfig& stage) {
    using TevStageConfig = Pica::TexturingRegs::TevStageConfig;
    return (stage.color_op == TevStageConfig::Operation::Replace &&
            stage.alpha_op == TevStageConfig::Operation::Replace &&
            stage.color_source1 == TevStageConfig::Source::Previous &&
            stage.alpha_source1 == TevStageConfig::Source::Previous &&
            stage.color_modifier1 == TevStageConfig::ColorModifier::SourceColor &&
            stage.alpha_modifier1 == TevStageConfig::AlphaModifier::SourceAlpha &&
            stage.GetColorMultiplier() == 1 && stage.GetAlphaMultiplier() == 1);
}

// High precision may or may not be supported in GLES3. If it isn't, use medium precision instead.
static constexpr char fragment_shader_precision_OES[] = R"(
#if GL_ES
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp int;
precision highp float;
precision highp samplerBuffer;
precision highp uimage2D;
#else
precision mediump int;
precision mediump float;
precision mediump samplerBuffer;
precision mediump uimage2D;
#endif // GL_FRAGMENT_PRECISION_HIGH
#endif
)";

constexpr static std::string_view FSUniformBlockDef = R"(
#define NUM_TEV_STAGES 6
#define NUM_LIGHTS 8
#define NUM_LIGHTING_SAMPLERS 24
struct LightSrc {
    vec3 specular_0;
    vec3 specular_1;
    vec3 diffuse;
    vec3 ambient;
    vec3 position;
    vec3 spot_direction;
    float dist_atten_bias;
    float dist_atten_scale;
};
layout (binding = 2, std140) uniform fs_data {
    int framebuffer_scale;
    int alphatest_ref;
    float depth_scale;
    float depth_offset;
    float shadow_bias_constant;
    float shadow_bias_linear;
    int scissor_x1;
    int scissor_y1;
    int scissor_x2;
    int scissor_y2;
    int fog_lut_offset;
    int proctex_noise_lut_offset;
    int proctex_color_map_offset;
    int proctex_alpha_map_offset;
    int proctex_lut_offset;
    int proctex_diff_lut_offset;
    float proctex_bias;
    int shadow_texture_bias;
    ivec4 lighting_lut_offset[NUM_LIGHTING_SAMPLERS / 4];
    vec3 fog_color;
    vec2 proctex_noise_f;
    vec2 proctex_noise_a;
    vec2 proctex_noise_p;
    vec3 lighting_global_ambient;
    LightSrc light_src[NUM_LIGHTS];
    vec4 const_color[NUM_TEV_STAGES];
    vec4 tev_combiner_buffer_color;
    vec3 tex_lod_bias;
    vec4 tex_border_color[3];
    vec4 blend_color;
};
)";

FragmentModule::FragmentModule(const FSConfig& config_, const Profile& profile_)
    : config{config_}, profile{profile_} {
    out.reserve(RESERVE_SIZE);
    DefineExtensions();
    DefineInterface();
    if (profile.is_vulkan) {
        DefineBindingsVK();
    } else {
        DefineBindingsGL();
    }
    DefineHelpers();
    DefineShadowHelpers();
    DefineLightingHelpers();
    DefineProcTexSampler();
    for (u32 i = 0; i < 4; i++) {
        DefineTexUnitSampler(i);
    }
}

FragmentModule::~FragmentModule() = default;

std::string FragmentModule::Generate() {
    // We round the interpolated primary color to the nearest 1/255th
    // This maintains the PICA's 8 bits of precision
    out += R"(
void main() {
vec4 rounded_primary_color = byteround(primary_color);
vec4 primary_fragment_color = vec4(0.0);
vec4 secondary_fragment_color = vec4(0.0);
)";

    // Do not do any sort of processing if it's obvious we're not going to pass the alpha test
    if (config.framebuffer.alpha_test_func == FramebufferRegs::CompareFunc::Never) {
        out += "discard; }";
        return out;
    }

    // Append the scissor and depth tests
    WriteScissor();
    WriteDepth();

    // Write shader source to emulate all enabled PICA lights
    WriteLighting();

    out += "vec4 combiner_buffer = vec4(0.0);\n"
           "vec4 next_combiner_buffer = tev_combiner_buffer_color;\n"
           "vec4 combiner_output = vec4(0.0);\n";

    out += "vec3 color_results_1 = vec3(0.0);\n"
           "vec3 color_results_2 = vec3(0.0);\n"
           "vec3 color_results_3 = vec3(0.0);\n";

    out += "float alpha_results_1 = 0.0;\n"
           "float alpha_results_2 = 0.0;\n"
           "float alpha_results_3 = 0.0;\n";

    // Write shader source to emulate PICA TEV stages
    for (u32 index = 0; index < config.texture.tev_stages.size(); index++) {
        WriteTevStage(index);
    }

    // Append the alpha test condition
    WriteAlphaTestCondition(config.framebuffer.alpha_test_func);

    // Emulate the fog
    switch (config.texture.fog_mode) {
    case TexturingRegs::FogMode::Fog:
        WriteFog();
        break;
    case TexturingRegs::FogMode::Gas:
        WriteGas();
        return out;
    default:
        break;
    }

    if (config.framebuffer.shadow_rendering) {
        WriteShadow();
    } else {
        out += "gl_FragDepth = depth;\n";
        // Round the final fragment color to maintain the PICA's 8 bits of precision
        out += "combiner_output = byteround(combiner_output);\n";
        WriteBlending();
        out += "color = combiner_output;\n";
    }

    WriteLogicOp();

    out += '}';
    return out;
}

void FragmentModule::WriteDepth() {
    // The PICA depth range is [-1, 0]. The vertex shader outputs the negated Z value, otherwise
    // unmodified. When the depth range is [-1, 1], it is converted into [near, far] = [0, 1].
    // This compresses our effective range into [0.5, 1]. To account for this we un-negate the value
    // to range [-1, -0.5], multiply by 2 to the range [-2, -1], and add 1 to arrive back at the
    // original range of [-1, 0]. If the depth range is [0, 1], so all we need to do is
    // un-negate the value to range [-1, 0]. Once we have z_over_w, we can do our own transformation
    // according to PICA specification.
    if (profile.has_minus_one_to_one_range) {
        out += "float z_over_w = -2.0 * gl_FragCoord.z + 1.0;\n";
    } else {
        out += "float z_over_w = -gl_FragCoord.z;\n";
    }
    out += "float depth = z_over_w * depth_scale + depth_offset;\n";
    if (config.framebuffer.depthmap_enable == RasterizerRegs::DepthBuffering::WBuffering) {
        out += "depth /= gl_FragCoord.w;\n";
    }
}

void FragmentModule::WriteScissor() {
    const auto scissor_mode = config.framebuffer.scissor_test_mode.Value();
    if (scissor_mode == RasterizerRegs::ScissorMode::Disabled) {
        return;
    }

    out += "if (";
    // Negate the condition if we have to keep only the pixels outside the scissor box
    if (scissor_mode == RasterizerRegs::ScissorMode::Include) {
        out += '!';
    }
    out += "(gl_FragCoord.x >= float(scissor_x1) && "
           "gl_FragCoord.y >= float(scissor_y1) && "
           "gl_FragCoord.x < float(scissor_x2) && "
           "gl_FragCoord.y < float(scissor_y2))) discard;\n";
}

std::string FragmentModule::GetSource(Pica::TexturingRegs::TevStageConfig::Source source,
                                      u32 tev_index) {
    using Source = Pica::TexturingRegs::TevStageConfig::Source;
    switch (source) {
    case Source::PrimaryColor:
        return "rounded_primary_color";
    case Source::PrimaryFragmentColor:
        return "primary_fragment_color";
    case Source::SecondaryFragmentColor:
        return "secondary_fragment_color";
    case Source::Texture0:
        return "sampleTexUnit0()";
    case Source::Texture1:
        return "sampleTexUnit1()";
    case Source::Texture2:
        return "sampleTexUnit2()";
    case Source::Texture3:
        return "sampleTexUnit3()";
    case Source::PreviousBuffer:
        return "combiner_buffer";
    case Source::Constant:
        return fmt::format("const_color[{}]", tev_index);
    case Source::Previous:
        return "combiner_output";
    default:
        LOG_CRITICAL(Render, "Unknown source op {}", source);
        return "vec4(0.0)";
    }
}

void FragmentModule::AppendColorModifier(
    Pica::TexturingRegs::TevStageConfig::ColorModifier modifier,
    Pica::TexturingRegs::TevStageConfig::Source source, u32 tev_index) {
    using Source = Pica::TexturingRegs::TevStageConfig::Source;
    using ColorModifier = Pica::TexturingRegs::TevStageConfig::ColorModifier;
    const TexturingRegs::TevStageConfig stage = config.texture.tev_stages[tev_index];
    const bool force_source3 = tev_index == 0 && source == Source::Previous;
    const auto color_source =
        GetSource(force_source3 ? stage.color_source3.Value() : source, tev_index);
    switch (modifier) {
    case ColorModifier::SourceColor:
        out += fmt::format("{}.rgb", color_source);
        break;
    case ColorModifier::OneMinusSourceColor:
        out += fmt::format("vec3(1.0) - {}.rgb", color_source);
        break;
    case ColorModifier::SourceAlpha:
        out += fmt::format("{}.aaa", color_source);
        break;
    case ColorModifier::OneMinusSourceAlpha:
        out += fmt::format("vec3(1.0) - {}.aaa", color_source);
        break;
    case ColorModifier::SourceRed:
        out += fmt::format("{}.rrr", color_source);
        break;
    case ColorModifier::OneMinusSourceRed:
        out += fmt::format("vec3(1.0) - {}.rrr", color_source);
        break;
    case ColorModifier::SourceGreen:
        out += fmt::format("{}.ggg", color_source);
        break;
    case ColorModifier::OneMinusSourceGreen:
        out += fmt::format("vec3(1.0) - {}.ggg", color_source);
        break;
    case ColorModifier::SourceBlue:
        out += fmt::format("{}.bbb", color_source);
        break;
    case ColorModifier::OneMinusSourceBlue:
        out += fmt::format("vec3(1.0) - {}.bbb", color_source);
        break;
    default:
        out += "vec3(0.0)";
        LOG_CRITICAL(Render, "Unknown color modifier op {}", modifier);
        break;
    }
}

void FragmentModule::AppendAlphaModifier(
    Pica::TexturingRegs::TevStageConfig::AlphaModifier modifier,
    Pica::TexturingRegs::TevStageConfig::Source source, u32 tev_index) {
    using Source = Pica::TexturingRegs::TevStageConfig::Source;
    using AlphaModifier = Pica::TexturingRegs::TevStageConfig::AlphaModifier;
    const TexturingRegs::TevStageConfig stage = config.texture.tev_stages[tev_index];
    const bool force_source3 = tev_index == 0 && source == Source::Previous;
    const auto alpha_source =
        GetSource(force_source3 ? stage.alpha_source3.Value() : source, tev_index);
    switch (modifier) {
    case AlphaModifier::SourceAlpha:
        out += fmt::format("{}.a", alpha_source);
        break;
    case AlphaModifier::OneMinusSourceAlpha:
        out += fmt::format("1.0 - {}.a", alpha_source);
        break;
    case AlphaModifier::SourceRed:
        out += fmt::format("{}.r", alpha_source);
        break;
    case AlphaModifier::OneMinusSourceRed:
        out += fmt::format("1.0 - {}.r", alpha_source);
        break;
    case AlphaModifier::SourceGreen:
        out += fmt::format("{}.g", alpha_source);
        break;
    case AlphaModifier::OneMinusSourceGreen:
        out += fmt::format("1.0 - {}.g", alpha_source);
        break;
    case AlphaModifier::SourceBlue:
        out += fmt::format("{}.b", alpha_source);
        break;
    case AlphaModifier::OneMinusSourceBlue:
        out += fmt::format("1.0 - {}.b", alpha_source);
        break;
    default:
        out += "0.0";
        LOG_CRITICAL(Render, "Unknown alpha modifier op {}", modifier);
        break;
    }
}

void FragmentModule::AppendColorCombiner(Pica::TexturingRegs::TevStageConfig::Operation operation) {
    const auto get_combiner = [operation] {
        using Operation = Pica::TexturingRegs::TevStageConfig::Operation;
        switch (operation) {
        case Operation::Replace:
            return "color_results_1";
        case Operation::Modulate:
            return "color_results_1 * color_results_2";
        case Operation::Add:
            return "color_results_1 + color_results_2";
        case Operation::AddSigned:
            return "color_results_1 + color_results_2 - vec3(0.5)";
        case Operation::Lerp:
            return "mix(color_results_2, color_results_1, color_results_3)";
        case Operation::Subtract:
            return "color_results_1 - color_results_2";
        case Operation::MultiplyThenAdd:
            return "fma(color_results_1, color_results_2, color_results_3)";
        case Operation::AddThenMultiply:
            return "min(color_results_1 + color_results_2, vec3(1.0)) * color_results_3";
        case Operation::Dot3_RGB:
        case Operation::Dot3_RGBA:
            return "vec3(dot(color_results_1 - vec3(0.5), color_results_2 - vec3(0.5)) * 4.0)";
        default:
            LOG_CRITICAL(Render, "Unknown color combiner operation: {}", operation);
            return "vec3(0.0)";
        }
    };
    out += fmt::format("clamp({}, vec3(0.0), vec3(1.0))", get_combiner());
}

void FragmentModule::AppendAlphaCombiner(Pica::TexturingRegs::TevStageConfig::Operation operation) {
    const auto get_combiner = [operation] {
        using Operation = Pica::TexturingRegs::TevStageConfig::Operation;
        switch (operation) {
        case Operation::Replace:
            return "alpha_results_1";
        case Operation::Modulate:
            return "alpha_results_1 * alpha_results_2";
        case Operation::Add:
            return "alpha_results_1 + alpha_results_2";
        case Operation::AddSigned:
            return "alpha_results_1 + alpha_results_2 - 0.5";
        case Operation::Lerp:
            return "mix(alpha_results_2, alpha_results_1, alpha_results_3)";
        case Operation::Subtract:
            return "alpha_results_1 - alpha_results_2";
        case Operation::MultiplyThenAdd:
            return "fma(alpha_results_1, alpha_results_2, alpha_results_3)";
        case Operation::AddThenMultiply:
            return "min(alpha_results_1 + alpha_results_2, 1.0) * alpha_results_3";
        default:
            LOG_CRITICAL(Render, "Unknown alpha combiner operation: {}", operation);
            return "0.0";
        }
    };
    out += fmt::format("clamp({}, 0.0, 1.0)", get_combiner());
}

void FragmentModule::WriteAlphaTestCondition(FramebufferRegs::CompareFunc func) {
    const auto get_cond = [func]() -> std::string {
        using CompareFunc = Pica::FramebufferRegs::CompareFunc;
        switch (func) {
        case CompareFunc::Never:
            return "true";
        case CompareFunc::Always:
            return "false";
        case CompareFunc::Equal:
        case CompareFunc::NotEqual:
        case CompareFunc::LessThan:
        case CompareFunc::LessThanOrEqual:
        case CompareFunc::GreaterThan:
        case CompareFunc::GreaterThanOrEqual: {
            static constexpr std::array op{"!=", "==", ">=", ">", "<=", "<"};
            const auto index = static_cast<u32>(func) - static_cast<u32>(CompareFunc::Equal);
            return fmt::format("int(combiner_output.a * 255.0) {} alphatest_ref", op[index]);
        }
        default:
            LOG_CRITICAL(Render, "Unknown alpha test condition {}", func);
            return "false";
            break;
        }
    };
    out += fmt::format("if ({}) discard;\n", get_cond());
}

void FragmentModule::WriteTevStage(u32 index) {
    const TexturingRegs::TevStageConfig stage = config.texture.tev_stages[index];
    if (!IsPassThroughTevStage(stage)) {
        out += "color_results_1 = ";
        AppendColorModifier(stage.color_modifier1, stage.color_source1, index);
        out += ";\ncolor_results_2 = ";
        AppendColorModifier(stage.color_modifier2, stage.color_source2, index);
        out += ";\ncolor_results_3 = ";
        AppendColorModifier(stage.color_modifier3, stage.color_source3, index);

        // Round the output of each TEV stage to maintain the PICA's 8 bits of precision
        out += fmt::format(";\nvec3 color_output_{} = byteround(", index);
        AppendColorCombiner(stage.color_op);
        out += ");\n";

        if (stage.color_op == Pica::TexturingRegs::TevStageConfig::Operation::Dot3_RGBA) {
            // result of Dot3_RGBA operation is also placed to the alpha component
            out += fmt::format("float alpha_output_{0} = color_output_{0}[0];\n", index);
        } else {
            out += "alpha_results_1 = ";
            AppendAlphaModifier(stage.alpha_modifier1, stage.alpha_source1, index);
            out += ";\nalpha_results_2 = ";
            AppendAlphaModifier(stage.alpha_modifier2, stage.alpha_source2, index);
            out += ";\nalpha_results_3 = ";
            AppendAlphaModifier(stage.alpha_modifier3, stage.alpha_source3, index);

            out += fmt::format(";\nfloat alpha_output_{} = byteround(", index);
            AppendAlphaCombiner(stage.alpha_op);
            out += ");\n";
        }

        out += fmt::format("combiner_output = vec4("
                           "clamp(color_output_{} * {}.0, vec3(0.0), vec3(1.0)), "
                           "clamp(alpha_output_{} * {}.0, 0.0, 1.0));\n",
                           index, stage.GetColorMultiplier(), index, stage.GetAlphaMultiplier());
    }

    out += "combiner_buffer = next_combiner_buffer;\n";
    if (config.TevStageUpdatesCombinerBufferColor(index)) {
        out += "next_combiner_buffer.rgb = combiner_output.rgb;\n";
    }
    if (config.TevStageUpdatesCombinerBufferAlpha(index)) {
        out += "next_combiner_buffer.a = combiner_output.a;\n";
    }
}

void FragmentModule::WriteLighting() {
    if (!config.lighting.enable) {
        return;
    }

    const auto& lighting = config.lighting;

    // Define lighting globals
    out += "vec4 diffuse_sum = vec4(0.0, 0.0, 0.0, 1.0);\n"
           "vec4 specular_sum = vec4(0.0, 0.0, 0.0, 1.0);\n"
           "vec3 light_vector = vec3(0.0);\n"
           "float light_distance = 0.0;\n"
           "vec3 refl_value = vec3(0.0);\n"
           "vec3 spot_dir = vec3(0.0);\n"
           "vec3 half_vector = vec3(0.0);\n"
           "float dot_product = 0.0;\n"
           "float clamp_highlights = 1.0;\n"
           "float geo_factor = 1.0;\n";

    // Compute fragment normals and tangents
    const auto perturbation = [&] {
        return fmt::format("2.0 * (sampleTexUnit{}()).rgb - 1.0", lighting.bump_selector.Value());
    };

    if (config.user.use_custom_normal) {
        const auto texel = fmt::format("2.0 * (texture(tex_normal, texcoord0)).rgb - 1.0");
        out += fmt::format("vec3 surface_normal = {};\n", texel);
        out += "vec3 surface_tangent = vec3(1.0, 0.0, 0.0);\n";
    } else {
        switch (lighting.bump_mode) {
        case LightingRegs::LightingBumpMode::NormalMap: {
            // Bump mapping is enabled using a normal map
            out += fmt::format("vec3 surface_normal = {};\n", perturbation());

            // Recompute Z-component of perturbation if 'renorm' is enabled, this provides a higher
            // precision result
            if (lighting.bump_renorm) {
                constexpr std::string_view val = "(1.0 - (surface_normal.x*surface_normal.x + "
                                                 "surface_normal.y*surface_normal.y))";
                out += fmt::format("surface_normal.z = sqrt(max({}, 0.0));\n", val);
            }

            // The tangent vector is not perturbed by the normal map and is just a unit vector.
            out += "vec3 surface_tangent = vec3(1.0, 0.0, 0.0);\n";
            break;
        }
        case LightingRegs::LightingBumpMode::TangentMap: {
            // Bump mapping is enabled using a tangent map
            out += fmt::format("vec3 surface_tangent = {};\n", perturbation());
            // Mathematically, recomputing Z-component of the tangent vector won't affect the
            // relevant computation below, which is also confirmed on 3DS. So we don't bother
            // recomputing here even if 'renorm' is enabled.

            // The normal vector is not perturbed by the tangent map and is just a unit vector.
            out += "vec3 surface_normal = vec3(0.0, 0.0, 1.0);\n";
            break;
        }
        default:
            // No bump mapping - surface local normal and tangent are just unit vectors
            out += "vec3 surface_normal = vec3(0.0, 0.0, 1.0);\n"
                   "vec3 surface_tangent = vec3(1.0, 0.0, 0.0);\n";
        }
    }

    // If the barycentric extension is enabled, perform quaternion correction here.
    if (use_fragment_shader_barycentric) {
        out += "vec4 normquat_0 = normquats[0];\n"
               "vec4 normquat_1 = mix(normquats[1], -normquats[1], "
               "bvec4(AreQuaternionsOpposite(normquats[0], normquats[1])));\n"
               "vec4 normquat_2 = mix(normquats[2], -normquats[2], "
               "bvec4(AreQuaternionsOpposite(normquats[0], normquats[2])));\n"
               "vec4 normquat = gl_BaryCoord.x * normquat_0 + gl_BaryCoord.y * normquat_1 + "
               "gl_BaryCoord.z * normquat_2;\n";
    }

    // Rotate the surface-local normal by the interpolated normal quaternion to convert it to
    // eyespace.
    out += "vec4 normalized_normquat = normalize(normquat);\n"
           "vec3 normal = quaternion_rotate(normalized_normquat, surface_normal);\n"
           "vec3 tangent = quaternion_rotate(normalized_normquat, surface_tangent);\n";

    if (lighting.enable_shadow) {
        std::string shadow_texture =
            fmt::format("sampleTexUnit{}()", lighting.shadow_selector.Value());
        if (lighting.shadow_invert) {
            out += fmt::format("vec4 shadow = vec4(1.0) - {};\n", shadow_texture);
        } else {
            out += fmt::format("vec4 shadow = {};\n", shadow_texture);
        }
    } else {
        out += "vec4 shadow = vec4(1.0);\n";
    }

    // Samples the specified lookup table for specular lighting
    const auto get_lut_value = [&lighting](LightingRegs::LightingSampler sampler, u32 light_num,
                                           LightingRegs::LightingLutInput input, bool abs) {
        std::string index;
        switch (input) {
        case LightingRegs::LightingLutInput::NH:
            index = "dot(normal, normalize(half_vector))";
            break;
        case LightingRegs::LightingLutInput::VH:
            index = "dot(normalize(view), normalize(half_vector))";
            break;
        case LightingRegs::LightingLutInput::NV:
            index = "dot(normal, normalize(view))";
            break;
        case LightingRegs::LightingLutInput::LN:
            index = "dot(light_vector, normal)";
            break;
        case LightingRegs::LightingLutInput::SP:
            index = "dot(light_vector, spot_dir)";
            break;
        case LightingRegs::LightingLutInput::CP:
            // CP input is only available with configuration 7
            if (lighting.config == LightingRegs::LightingConfig::Config7) {
                // Note: even if the normal vector is modified by normal map, which is not the
                // normal of the tangent plane anymore, the half angle vector is still projected
                // using the modified normal vector.
                constexpr std::string_view half_angle_proj =
                    "normalize(half_vector) - normal * dot(normal, normalize(half_vector))";
                // Note: the half angle vector projection is confirmed not normalized before the dot
                // product. The result is in fact not cos(phi) as the name suggested.
                index = fmt::format("dot({}, tangent)", half_angle_proj);
            } else {
                index = "0.0";
            }
            break;
        default:
            LOG_CRITICAL(HW_GPU, "Unknown lighting LUT input {}", static_cast<int>(input));
            UNIMPLEMENTED();
            index = "0.0";
            break;
        }

        const auto sampler_index = static_cast<u32>(sampler);

        if (abs) {
            // LUT index is in the range of (0.0, 1.0)
            index = lighting.lights[light_num].two_sided_diffuse
                        ? fmt::format("abs({})", index)
                        : fmt::format("max({}, 0.0)", index);
            return fmt::format("LookupLightingLUTUnsigned({}, {})", sampler_index, index);
        } else {
            // LUT index is in the range of (-1.0, 1.0)
            return fmt::format("LookupLightingLUTSigned({}, {})", sampler_index, index);
        }
    };

    // Write the code to emulate each enabled light
    for (u32 light_index = 0; light_index < lighting.src_num; ++light_index) {
        const auto& light_config = lighting.lights[light_index];
        const std::string light_src = fmt::format("light_src[{}]", light_config.num.Value());

        // Compute light vector (directional or positional)
        if (light_config.directional) {
            out += fmt::format("light_vector = {}.position;\n", light_src);
        } else {
            out += fmt::format("light_vector = {}.position + view;\n", light_src);
        }
        out += fmt::format("light_distance = length(light_vector);\n", light_src);
        out += fmt::format("light_vector = normalize(light_vector);\n", light_src);

        out += fmt::format("spot_dir = {}.spot_direction;\n", light_src);
        out += "half_vector = normalize(view) + light_vector;\n";

        // Compute dot product of light_vector and normal, adjust if lighting is one-sided or
        // two-sided
        out += "dot_product = ";
        out += light_config.two_sided_diffuse ? "abs(dot(light_vector, normal));\n"
                                              : "max(dot(light_vector, normal), 0.0);\n";

        // If enabled, clamp specular component if lighting result is zero
        if (lighting.clamp_highlights) {
            out += "clamp_highlights = sign(dot_product);\n";
        }

        // If enabled, compute spot light attenuation value
        std::string spot_atten = "1.0";
        if (light_config.spot_atten_enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::SpotlightAttenuation)) {
            const std::string value =
                get_lut_value(LightingRegs::SpotlightAttenuationSampler(light_config.num),
                              light_config.num, lighting.lut_sp.type, lighting.lut_sp.abs_input);
            spot_atten = fmt::format("({:#} * {})", lighting.lut_sp.scale, value);
        }

        // If enabled, compute distance attenuation value
        std::string dist_atten = "1.0";
        if (light_config.dist_atten_enable) {
            const std::string index = fmt::format("clamp({}.dist_atten_scale * light_distance "
                                                  "+ {}.dist_atten_bias, 0.0, 1.0)",
                                                  light_src, light_src, light_src);
            const auto sampler = LightingRegs::DistanceAttenuationSampler(light_config.num);
            dist_atten = fmt::format("LookupLightingLUTUnsigned({}, {})", sampler, index);
        }

        if (light_config.geometric_factor_0 || light_config.geometric_factor_1) {
            out += "geo_factor = dot(half_vector, half_vector);\n"
                   "geo_factor = geo_factor == 0.0 ? 0.0 : min("
                   "dot_product / geo_factor, 1.0);\n";
        }

        // Specular 0 component
        std::string d0_lut_value = "1.0";
        if (lighting.lut_d0.enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::Distribution0)) {
            // Lookup specular "distribution 0" LUT value
            const std::string value =
                get_lut_value(LightingRegs::LightingSampler::Distribution0, light_config.num,
                              lighting.lut_d0.type, lighting.lut_d0.abs_input);
            d0_lut_value = fmt::format("({:#} * {})", lighting.lut_d0.scale, value);
        }
        std::string specular_0 = fmt::format("({} * {}.specular_0)", d0_lut_value, light_src);
        if (light_config.geometric_factor_0) {
            specular_0 = fmt::format("({} * geo_factor)", specular_0);
        }

        // If enabled, lookup ReflectRed value, otherwise, 1.0 is used
        if (lighting.lut_rr.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectRed)) {
            std::string value =
                get_lut_value(LightingRegs::LightingSampler::ReflectRed, light_config.num,
                              lighting.lut_rr.type, lighting.lut_rr.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_rr.scale, value);
            out += fmt::format("refl_value.r = {};\n", value);
        } else {
            out += "refl_value.r = 1.0;\n";
        }

        // If enabled, lookup ReflectGreen value, otherwise, ReflectRed value is used
        if (lighting.lut_rg.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectGreen)) {
            std::string value =
                get_lut_value(LightingRegs::LightingSampler::ReflectGreen, light_config.num,
                              lighting.lut_rg.type, lighting.lut_rg.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_rg.scale, value);
            out += fmt::format("refl_value.g = {};\n", value);
        } else {
            out += "refl_value.g = refl_value.r;\n";
        }

        // If enabled, lookup ReflectBlue value, otherwise, ReflectRed value is used
        if (lighting.lut_rb.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::ReflectBlue)) {
            std::string value =
                get_lut_value(LightingRegs::LightingSampler::ReflectBlue, light_config.num,
                              lighting.lut_rb.type, lighting.lut_rb.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_rb.scale, value);
            out += fmt::format("refl_value.b = {};\n", value);
        } else {
            out += "refl_value.b = refl_value.r;\n";
        }

        // Specular 1 component
        std::string d1_lut_value = "1.0";
        if (lighting.lut_d1.enable &&
            LightingRegs::IsLightingSamplerSupported(
                lighting.config, LightingRegs::LightingSampler::Distribution1)) {
            // Lookup specular "distribution 1" LUT value
            const std::string value =
                get_lut_value(LightingRegs::LightingSampler::Distribution1, light_config.num,
                              lighting.lut_d1.type, lighting.lut_d1.abs_input);
            d1_lut_value = fmt::format("({:#} * {})", lighting.lut_d1.scale, value);
        }
        std::string specular_1 =
            fmt::format("({} * refl_value * {}.specular_1)", d1_lut_value, light_src);
        if (light_config.geometric_factor_1) {
            specular_1 = fmt::format("({} * geo_factor)", specular_1);
        }

        // Fresnel
        // Note: only the last entry in the light slots applies the Fresnel factor
        if (light_index == lighting.src_num - 1 && lighting.lut_fr.enable &&
            LightingRegs::IsLightingSamplerSupported(lighting.config,
                                                     LightingRegs::LightingSampler::Fresnel)) {
            // Lookup fresnel LUT value
            std::string value =
                get_lut_value(LightingRegs::LightingSampler::Fresnel, light_config.num,
                              lighting.lut_fr.type, lighting.lut_fr.abs_input);
            value = fmt::format("({:#} * {})", lighting.lut_fr.scale, value);

            // Enabled for diffuse lighting alpha component
            if (lighting.enable_primary_alpha) {
                out += fmt::format("diffuse_sum.a = {};\n", value);
            }

            // Enabled for the specular lighting alpha component
            if (lighting.enable_secondary_alpha) {
                out += fmt::format("specular_sum.a = {};\n", value);
            }
        }

        const bool shadow_primary_enable = lighting.shadow_primary && light_config.shadow_enable;
        const bool shadow_secondary_enable =
            lighting.shadow_secondary && light_config.shadow_enable;
        const auto shadow_primary = shadow_primary_enable ? " * shadow.rgb" : "";
        const auto shadow_secondary = shadow_secondary_enable ? " * shadow.rgb" : "";

        // Compute primary fragment color (diffuse lighting) function
        out += fmt::format(
            "diffuse_sum.rgb += (({}.diffuse * dot_product{}) + {}.ambient) * {} * {};\n",
            light_src, shadow_primary, light_src, dist_atten, spot_atten);

        // Compute secondary fragment color (specular lighting) function
        out += fmt::format("specular_sum.rgb += ({} + {}) * clamp_highlights * {} * {}{};\n",
                           specular_0, specular_1, dist_atten, spot_atten, shadow_secondary);
    }

    // Apply shadow attenuation to alpha components if enabled
    if (lighting.shadow_alpha) {
        if (lighting.enable_primary_alpha) {
            out += "diffuse_sum.a *= shadow.a;\n";
        }
        if (lighting.enable_secondary_alpha) {
            out += "specular_sum.a *= shadow.a;\n";
        }
    }

    // Sum final lighting result
    out += "diffuse_sum.rgb += lighting_global_ambient;\n"
           "primary_fragment_color = clamp(diffuse_sum, vec4(0.0), vec4(1.0));\n"
           "secondary_fragment_color = clamp(specular_sum, vec4(0.0), vec4(1.0));\n";
}

void FragmentModule::WriteFog() {
    // Get index into fog LUT
    if (config.texture.fog_flip) {
        out += "float fog_index = (1.0 - float(depth)) * 128.0;\n";
    } else {
        out += "float fog_index = depth * 128.0;\n";
    }

    // Generate clamped fog factor from LUT for given fog index
    out += "float fog_i = clamp(floor(fog_index), 0.0, 127.0);\n"
           "float fog_f = fog_index - fog_i;\n"
           "vec2 fog_lut_entry = texelFetch(texture_buffer_lut_lf, int(fog_i) + "
           "fog_lut_offset).rg;\n"
           "float fog_factor = fog_lut_entry.r + fog_lut_entry.g * fog_f;\n"
           "fog_factor = clamp(fog_factor, 0.0, 1.0);\n";

    // Blend the fog
    out += "combiner_output.rgb = mix(fog_color.rgb, combiner_output.rgb, fog_factor);\n";
}

void FragmentModule::WriteGas() {
    // TODO: Implement me
    LOG_CRITICAL(Render, "Unimplemented gas mode");
    out += "discard; }";
}

void FragmentModule::WriteShadow() {
    out += R"(
uint d = uint(clamp(depth, 0.0, 1.0) * float(0xFFFFFF));
uint s = uint(combiner_output.g * float(0xFF));
ivec2 image_coord = ivec2(gl_FragCoord.xy);
)";

    if (use_fragment_shader_interlock) {
        out += R"(
beginInvocationInterlock();
uint old_shadow = imageLoad(shadow_buffer, image_coord).x;
uint new_shadow = UpdateShadow(old_shadow, d, s);
imageStore(shadow_buffer, image_coord, uvec4(new_shadow));
endInvocationInterlock();
)";
    } else {
        out += R"(
uint old = imageLoad(shadow_buffer, image_coord).x;
uint new1;
uint old2;
do {
    old2 = old;
    new1 = UpdateShadow(old, d, s);
} while ((old = imageAtomicCompSwap(shadow_buffer, image_coord, old, new1)) != old2);
)";
    }
}

void FragmentModule::WriteLogicOp() {
    const auto logic_op = config.framebuffer.logic_op.Value();
    switch (logic_op) {
    case FramebufferRegs::LogicOp::Clear:
        out += "color = vec4(0);\n";
        break;
    case FramebufferRegs::LogicOp::Set:
        out += "color = vec4(1);\n";
        break;
    case FramebufferRegs::LogicOp::Copy:
        // Take the color output as-is
        break;
    case FramebufferRegs::LogicOp::CopyInverted:
        out += "color = ~color;\n";
        break;
    case FramebufferRegs::LogicOp::NoOp:
        // We need to discard the color, but not necessarily the depth. This is not possible
        // with fragment shader alone, so we emulate this behavior on GLES with glColorMask.
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unhandled logic_op {:x}", logic_op);
        UNIMPLEMENTED();
    }
}

void FragmentModule::WriteBlending() {
    if (!config.EmulateBlend() || profile.is_vulkan) [[likely]] {
        return;
    }

    using BlendFactor = Pica::FramebufferRegs::BlendFactor;
    out += "vec4 source_color = combiner_output;\n";
    out += "vec4 dest_color = destFactor;\n";
    const auto get_factor = [&](BlendFactor factor) -> std::string {
        switch (factor) {
        case BlendFactor::Zero:
            return "vec4(0.f)";
        case BlendFactor::One:
            return "vec4(1.f)";
        case BlendFactor::SourceColor:
            return "source_color";
        case BlendFactor::OneMinusSourceColor:
            return "vec4(1.f) - source_color";
        case BlendFactor::DestColor:
            return "dest_color";
        case BlendFactor::OneMinusDestColor:
            return "vec4(1.f) - dest_color";
        case BlendFactor::SourceAlpha:
            return "source_color.aaaa";
        case BlendFactor::OneMinusSourceAlpha:
            return "vec4(1.f) - source_color.aaaa";
        case BlendFactor::DestAlpha:
            return "dest_color.aaaa";
        case BlendFactor::OneMinusDestAlpha:
            return "vec4(1.f) - dest_color.aaaa";
        case BlendFactor::ConstantColor:
            return "blend_color";
        case BlendFactor::OneMinusConstantColor:
            return "vec4(1.f) - blend_color";
        case BlendFactor::ConstantAlpha:
            return "blend_color.aaaa";
        case BlendFactor::OneMinusConstantAlpha:
            return "vec4(1.f) - blend_color.aaaa";
        default:
            LOG_CRITICAL(Render_OpenGL, "Unknown blend factor {}", factor);
            return "vec4(1.f)";
        }
    };
    const auto get_func = [](Pica::FramebufferRegs::BlendEquation eq) {
        return eq == Pica::FramebufferRegs::BlendEquation::Min ? "min" : "max";
    };

    if (config.framebuffer.rgb_blend.eq != Pica::FramebufferRegs::BlendEquation::Add) {
        out += fmt::format(
            "combiner_output.rgb = {}(source_color.rgb * ({}).rgb, dest_color.rgb * ({}).rgb);\n",
            get_func(config.framebuffer.rgb_blend.eq),
            get_factor(config.framebuffer.rgb_blend.src_factor),
            get_factor(config.framebuffer.rgb_blend.dst_factor));
    }
    if (config.framebuffer.alpha_blend.eq != Pica::FramebufferRegs::BlendEquation::Add) {
        out +=
            fmt::format("combiner_output.a = {}(source_color.a * ({}).a, dest_color.a * ({}).a);\n",
                        get_func(config.framebuffer.alpha_blend.eq),
                        get_factor(config.framebuffer.alpha_blend.src_factor),
                        get_factor(config.framebuffer.alpha_blend.dst_factor));
    }
}

void FragmentModule::AppendProcTexShiftOffset(std::string_view v, ProcTexShift mode,
                                              ProcTexClamp clamp_mode) {
    const auto offset = (clamp_mode == ProcTexClamp::MirroredRepeat) ? "1.0" : "0.5";
    switch (mode) {
    case ProcTexShift::None:
        out += "0.0";
        break;
    case ProcTexShift::Odd:
        out += fmt::format("{} * float((int({}) / 2) % 2)", offset, v);
        break;
    case ProcTexShift::Even:
        out += fmt::format("{} * float(((int({}) + 1) / 2) % 2)", offset, v);
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unknown shift mode {}", mode);
        out += "0.0";
        break;
    }
}

void FragmentModule::AppendProcTexClamp(std::string_view var, ProcTexClamp mode) {
    switch (mode) {
    case ProcTexClamp::ToZero:
        out += fmt::format("{0} = {0} > 1.0 ? 0 : {0};\n", var);
        break;
    case ProcTexClamp::ToEdge:
        out += fmt::format("{0} = min({0}, 1.0);\n", var);
        break;
    case ProcTexClamp::SymmetricalRepeat:
        out += fmt::format("{0} = fract({0});\n", var);
        break;
    case ProcTexClamp::MirroredRepeat:
        out += fmt::format("{0} = int({0}) % 2 == 0 ? fract({0}) : 1.0 - fract({0});\n", var);
        break;
    case ProcTexClamp::Pulse:
        out += fmt::format("{0} = {0} > 0.5 ? 1.0 : 0.0;\n", var);
        break;
    default:
        LOG_CRITICAL(HW_GPU, "Unknown clamp mode {}", mode);
        out += fmt::format("{0} = min({0}, 1.0);\n", var);
        break;
    }
}

void FragmentModule::AppendProcTexCombineAndMap(ProcTexCombiner combiner, std::string_view offset) {
    const auto combined = [combiner] {
        switch (combiner) {
        case ProcTexCombiner::U:
            return "u";
        case ProcTexCombiner::U2:
            return "(u * u)";
        case TexturingRegs::ProcTexCombiner::V:
            return "v";
        case TexturingRegs::ProcTexCombiner::V2:
            return "(v * v)";
        case TexturingRegs::ProcTexCombiner::Add:
            return "((u + v) * 0.5)";
        case TexturingRegs::ProcTexCombiner::Add2:
            return "((u * u + v * v) * 0.5)";
        case TexturingRegs::ProcTexCombiner::SqrtAdd2:
            return "min(sqrt(u * u + v * v), 1.0)";
        case TexturingRegs::ProcTexCombiner::Min:
            return "min(u, v)";
        case TexturingRegs::ProcTexCombiner::Max:
            return "max(u, v)";
        case TexturingRegs::ProcTexCombiner::RMax:
            return "min(((u + v) * 0.5 + sqrt(u * u + v * v)) * 0.5, 1.0)";
        default:
            LOG_CRITICAL(HW_GPU, "Unknown combiner {}", combiner);
            return "0.0";
        }
    }();
    out += fmt::format("ProcTexLookupLUT({}, {})", offset, combined);
}

void FragmentModule::DefineProcTexSampler() {
    if (!config.proctex.enable) {
        return;
    }

    // LUT sampling uitlity
    // For NoiseLUT/ColorMap/AlphaMap, coord=0.0 is lut[0], coord=127.0/128.0 is lut[127] and
    // coord=1.0 is lut[127]+lut_diff[127]. For other indices, the result is interpolated using
    // value entries and difference entries.
    out += R"(
float ProcTexLookupLUT(int offset, float coord) {
    coord *= 128.0;
    float index_i = clamp(floor(coord), 0.0, 127.0);
    float index_f = coord - index_i; // fract() cannot be used here because 128.0 needs to be
                                     // extracted as index_i = 127.0 and index_f = 1.0
    vec2 entry = texelFetch(texture_buffer_lut_rg, int(index_i) + offset).rg;
    return clamp(entry.r + entry.g * index_f, 0.0, 1.0);
}
    )";

    // Noise utility
    if (config.proctex.noise_enable) {
        // See swrasterizer/proctex.cpp for more information about these functions
        out += R"(
int ProcTexNoiseRand1D(int v) {
    const int table[] = int[](0,4,10,8,4,9,7,12,5,15,13,14,11,15,2,11);
    return ((v % 9 + 2) * 3 & 0xF) ^ table[(v / 9) & 0xF];
}

float ProcTexNoiseRand2D(vec2 point) {
    const int table[] = int[](10,2,15,8,0,7,4,5,5,13,2,6,13,9,3,14);
    int u2 = ProcTexNoiseRand1D(int(point.x));
    int v2 = ProcTexNoiseRand1D(int(point.y));
    v2 += ((u2 & 3) == 1) ? 4 : 0;
    v2 ^= (u2 & 1) * 6;
    v2 += 10 + u2;
    v2 &= 0xF;
    v2 ^= table[u2];
    return -1.0 + float(v2) * (2.0/15.0);
}

float ProcTexNoiseCoef(vec2 x) {
    vec2 grid  = 9.0 * proctex_noise_f * abs(x + proctex_noise_p);
    vec2 point = floor(grid);
    vec2 frac  = grid - point;

    float g0 = ProcTexNoiseRand2D(point) * (frac.x + frac.y);
    float g1 = ProcTexNoiseRand2D(point + vec2(1.0, 0.0)) * (frac.x + frac.y - 1.0);
    float g2 = ProcTexNoiseRand2D(point + vec2(0.0, 1.0)) * (frac.x + frac.y - 1.0);
    float g3 = ProcTexNoiseRand2D(point + vec2(1.0, 1.0)) * (frac.x + frac.y - 2.0);

    float x_noise = ProcTexLookupLUT(proctex_noise_lut_offset, frac.x);
    float y_noise = ProcTexLookupLUT(proctex_noise_lut_offset, frac.y);
    float x0 = mix(g0, g1, x_noise);
    float x1 = mix(g2, g3, x_noise);
    return mix(x0, x1, y_noise);
}
        )";
    }

    out += "vec4 SampleProcTexColor(float lut_coord, int level) {\n";
    out += fmt::format("int lut_width = {} >> level;\n", config.proctex.lut_width);
    // Offsets for level 4-7 seem to be hardcoded
    out += fmt::format("int lut_offsets[8] = int[]({}, {}, {}, {}, 0xF0, 0xF8, 0xFC, 0xFE);\n",
                       config.proctex.lut_offset0, config.proctex.lut_offset1,
                       config.proctex.lut_offset2, config.proctex.lut_offset3);
    out += "int lut_offset = lut_offsets[level];\n";
    // For the color lut, coord=0.0 is lut[offset] and coord=1.0 is lut[offset+width-1]
    out += "lut_coord *= float(lut_width - 1);\n";

    switch (config.proctex.lut_filter) {
    case ProcTexFilter::Linear:
    case ProcTexFilter::LinearMipmapLinear:
    case ProcTexFilter::LinearMipmapNearest:
        out += "int lut_index_i = int(lut_coord) + lut_offset;\n";
        out += "float lut_index_f = fract(lut_coord);\n";
        out += "return texelFetch(texture_buffer_lut_rgba, lut_index_i + "
               "proctex_lut_offset) + "
               "lut_index_f * "
               "texelFetch(texture_buffer_lut_rgba, lut_index_i + proctex_diff_lut_offset);\n";
        break;
    case ProcTexFilter::Nearest:
    case ProcTexFilter::NearestMipmapLinear:
    case ProcTexFilter::NearestMipmapNearest:
        out += "lut_coord += float(lut_offset);\n";
        out += "return texelFetch(texture_buffer_lut_rgba, int(round(lut_coord)) + "
               "proctex_lut_offset);\n";
        break;
    }

    out += "}\n";

    out += "vec4 ProcTex() {\n";
    if (config.proctex.coord < 3) {
        out += fmt::format("vec2 uv = abs(texcoord{});\n", config.proctex.coord.Value());
    } else {
        LOG_CRITICAL(Render, "Unexpected proctex.coord >= 3");
        out += "vec2 uv = abs(texcoord0);\n";
    }

    // This LOD formula is the same as the LOD upper limit defined in OpenGL.
    // f(x, y) <= m_u + m_v + m_w
    // (See OpenGL 4.6 spec, 8.14.1 - Scale Factor and Level-of-Detail)
    // Note: this is different from the one normal 2D textures use.
    out += "vec2 duv = max(abs(dFdx(uv)), abs(dFdy(uv)));\n";
    // unlike normal texture, the bias is inside the log2
    out += fmt::format("float lod = log2(abs(float({}) * proctex_bias) * (duv.x + duv.y));\n",
                       config.proctex.lut_width);
    out += "if (proctex_bias == 0.0) lod = 0.0;\n";
    out += fmt::format("lod = clamp(lod, {:#}, {:#});\n",
                       std::max(0.0f, static_cast<f32>(config.proctex.lod_min)),
                       std::min(7.0f, static_cast<f32>(config.proctex.lod_max)));

    // Get shift offset before noise generation
    out += "float u_shift = ";
    AppendProcTexShiftOffset("uv.y", config.proctex.u_shift, config.proctex.u_clamp);
    out += ";\n";
    out += "float v_shift = ";
    AppendProcTexShiftOffset("uv.x", config.proctex.v_shift, config.proctex.v_clamp);
    out += ";\n";

    // Generate noise
    if (config.proctex.noise_enable) {
        out += "uv += proctex_noise_a * ProcTexNoiseCoef(uv);\n"
               "uv = abs(uv);\n";
    }

    // Shift
    out += "float u = uv.x + u_shift;\n"
           "float v = uv.y + v_shift;\n";

    // Clamp
    AppendProcTexClamp("u", config.proctex.u_clamp);
    AppendProcTexClamp("v", config.proctex.v_clamp);

    // Combine and map
    out += "float lut_coord = ";
    AppendProcTexCombineAndMap(config.proctex.color_combiner, "proctex_color_map_offset");
    out += ";\n";

    switch (config.proctex.lut_filter) {
    case ProcTexFilter::Linear:
    case ProcTexFilter::Nearest:
        out += "vec4 final_color = SampleProcTexColor(lut_coord, 0);\n";
        break;
    case ProcTexFilter::NearestMipmapNearest:
    case ProcTexFilter::LinearMipmapNearest:
        out += "vec4 final_color = SampleProcTexColor(lut_coord, int(round(lod)));\n";
        break;
    case ProcTexFilter::NearestMipmapLinear:
    case ProcTexFilter::LinearMipmapLinear:
        out += "int lod_i = int(lod);\n"
               "float lod_f = fract(lod);\n"
               "vec4 final_color = mix(SampleProcTexColor(lut_coord, lod_i), "
               "SampleProcTexColor(lut_coord, lod_i + 1), lod_f);\n";
        break;
    }

    if (config.proctex.separate_alpha) {
        // Note: in separate alpha mode, the alpha channel skips the color LUT look up stage. It
        // uses the output of CombineAndMap directly instead.
        out += "float final_alpha = ";
        AppendProcTexCombineAndMap(config.proctex.alpha_combiner, "proctex_alpha_map_offset");
        out += ";\n";
        out += "return vec4(final_color.xyz, final_alpha);\n}\n";
    } else {
        out += "return final_color;\n}\n";
    }
}

void FragmentModule::DefineExtensions() {
    if (profile.has_separable_shaders) {
        out += "#extension GL_ARB_separate_shader_objects : enable\n";
    }
    if (config.framebuffer.shadow_rendering) {
        use_fragment_shader_interlock = true;
        if (profile.has_fragment_shader_interlock) {
            out += "#extension GL_ARB_fragment_shader_interlock : enable\n";
            out += "#define beginInvocationInterlock beginInvocationInterlockARB\n";
            out += "#define endInvocationInterlock endInvocationInterlockARB\n";
        } else if (profile.has_gl_nv_fragment_shader_interlock) {
            out += "#extension GL_NV_fragment_shader_interlock : enable\n";
            out += "#define beginInvocationInterlock beginInvocationInterlockNV\n";
            out += "#define endInvocationInterlock endInvocationInterlockNV\n";
        } else if (profile.has_gl_intel_fragment_shader_ordering) {
            // NOTE: Intel does not have an end function for this.
            out += "#extension GL_INTEL_fragment_shader_ordering : enable\n";
            out += "#define beginInvocationInterlock beginFragmentShaderOrderingINTEL\n";
            out += "#define endInvocationInterlock()\n";
        } else {
            use_fragment_shader_interlock = false;
        }
    }
    if (config.lighting.enable) {
        use_fragment_shader_barycentric = true;
        if (profile.has_fragment_shader_barycentric) {
            out += "#extension GL_EXT_fragment_shader_barycentric : enable\n";
            out += "#define pervertex pervertexEXT\n";
            out += "#define gl_BaryCoord gl_BaryCoordEXT\n";
        } else if (profile.has_gl_nv_fragment_shader_barycentric) {
            out += "#extension GL_NV_fragment_shader_barycentric : enable\n";
            out += "#define pervertex pervertexNV\n";
            out += "#define gl_BaryCoord gl_BaryCoordNV\n";
        } else {
            use_fragment_shader_barycentric = false;
        }
    }
    if (config.EmulateBlend() && !profile.is_vulkan) {
        if (profile.has_gl_ext_framebuffer_fetch) {
            out += "#extension GL_EXT_shader_framebuffer_fetch : enable\n";
            out += "#define destFactor color\n";
        } else if (profile.has_gl_arm_framebuffer_fetch) {
            out += "#extension GL_ARM_shader_framebuffer_fetch : enable\n";
            out += "#define destFactor gl_LastFragColorARM\n";
        } else {
            out += "#define destFactor texelFetch(tex_color, ivec2(gl_FragCoord.xy), 0)\n";
            use_blend_fallback = true;
        }
    }

    if (!profile.is_vulkan) {
        out += fragment_shader_precision_OES;
    }
}

void FragmentModule::DefineInterface() {
    const auto define_input = [&](std::string_view var, Semantic location) {
        if (profile.has_separable_shaders) {
            out += fmt::format("layout (location = {}) ", location);
        }
        out += fmt::format("in {};\n", var);
    };

    // Input attributes
    define_input("vec4 primary_color", Semantic::Color);
    define_input("vec2 texcoord0", Semantic::Texcoord0);
    define_input("vec2 texcoord1", Semantic::Texcoord1);
    define_input("vec2 texcoord2", Semantic::Texcoord2);
    define_input("float texcoord0_w", Semantic::Texcoord0_W);
    if (use_fragment_shader_barycentric) {
        define_input("pervertex vec4 normquats[]", Semantic::Normquat);
    } else {
        define_input("vec4 normquat", Semantic::Normquat);
    }
    define_input("vec3 view", Semantic::View);

    // Output attributes
    out += "layout (location = 0) out vec4 color;\n\n";
}

void FragmentModule::DefineBindingsVK() {
    // Uniform and texture buffers
    out += FSUniformBlockDef;
    out += "layout(set = 0, binding = 3) uniform samplerBuffer texture_buffer_lut_lf;\n";
    out += "layout(set = 0, binding = 4) uniform samplerBuffer texture_buffer_lut_rg;\n";
    out += "layout(set = 0, binding = 5) uniform samplerBuffer texture_buffer_lut_rgba;\n\n";

    // Texture samplers
    const auto texture_type = config.texture.texture0_type.Value();
    const auto sampler_tex0 = [&] {
        switch (texture_type) {
        case TextureType::Shadow2D:
        case TextureType::ShadowCube:
            return "usampler2D";
        case TextureType::TextureCube:
            return "samplerCube";
        default:
            return "sampler2D";
        }
    }();
    for (u32 i = 0; i < 3; i++) {
        const auto sampler = i == 0 ? sampler_tex0 : "sampler2D";
        const auto num_descriptors = i == 0 && texture_type == TextureType::ShadowCube ? "[6]" : "";
        out += fmt::format("layout(set = 1, binding = {0}) uniform {1} tex{0}{2};\n", i, sampler,
                           num_descriptors);
    }

    // Utility textures
    if (config.framebuffer.shadow_rendering) {
        out += "layout(set = 2, binding = 0, r32ui) uniform uimage2D shadow_buffer;\n\n";
    }
    if (config.user.use_custom_normal) {
        out += "layout(set = 2, binding = 1) uniform sampler2D tex_normal;\n";
    }
}

void FragmentModule::DefineBindingsGL() {
    // Uniform and texture buffers
    out += FSUniformBlockDef;
    out += "layout(binding = 3) uniform samplerBuffer texture_buffer_lut_lf;\n";
    out += "layout(binding = 4) uniform samplerBuffer texture_buffer_lut_rg;\n";
    out += "layout(binding = 5) uniform samplerBuffer texture_buffer_lut_rgba;\n\n";

    // Texture samplers
    const auto texture_type = config.texture.texture0_type.Value();
    for (u32 i = 0; i < 3; i++) {
        const auto sampler =
            i == 0 && texture_type == TextureType::TextureCube ? "samplerCube" : "sampler2D";
        out += fmt::format("layout(binding = {0}) uniform {1} tex{0};\n", i, sampler);
    }

    // Utility textures
    if (config.user.use_custom_normal) {
        out += "layout(binding = 6) uniform sampler2D tex_normal;\n";
    }
    if (use_blend_fallback) {
        out += "layout(location = 7) uniform sampler2D tex_color;\n";
    }

    // Shadow textures
    if (texture_type == TextureType::Shadow2D || texture_type == TextureType::ShadowCube) {
        static constexpr std::array postfixes = {"px", "nx", "py", "ny", "pz", "nz"};
        for (u32 i = 0; i < postfixes.size(); i++) {
            out += fmt::format(
                "layout(binding = {}, r32ui) uniform readonly uimage2D shadow_texture_{};\n", i,
                postfixes[i]);
        }
    }
    if (config.framebuffer.shadow_rendering) {
        out += "layout(binding = 6, r32ui) uniform uimage2D shadow_buffer;\n\n";
    }
}

void FragmentModule::DefineHelpers() {
    out += R"(
vec3 quaternion_rotate(vec4 q, vec3 v) {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}

float byteround(float x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

vec2 byteround(vec2 x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

vec3 byteround(vec3 x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

vec4 byteround(vec4 x) {
    return round(x * 255.0) * (1.0 / 255.0);
}

float getLod(vec2 coord) {
    vec2 d = max(abs(dFdx(coord)), abs(dFdy(coord)));
    return log2(max(d.x, d.y));
}

uvec2 DecodeShadow(uint pixel) {
    return uvec2(pixel >> 8, pixel & 0xFFu);
}
)";
}

void FragmentModule::DefineLightingHelpers() {
    if (!config.lighting.enable) {
        return;
    }

    out += R"(
float LookupLightingLUT(int lut_index, int index, float delta) {
    vec2 entry = texelFetch(texture_buffer_lut_lf, lighting_lut_offset[lut_index >> 2][lut_index & 3] + index).rg;
    return entry.r + entry.g * delta;
}

float LookupLightingLUTUnsigned(int lut_index, float pos) {
    int index = int(clamp(floor(pos * 256.0), 0.f, 255.f));
    float delta = pos * 256.0 - float(index);
    return LookupLightingLUT(lut_index, index, delta);
}

float LookupLightingLUTSigned(int lut_index, float pos) {
    int index = int(clamp(floor(pos * 128.0), -128.f, 127.f));
    float delta = pos * 128.0 - float(index);
    if (index < 0) index += 256;
    return LookupLightingLUT(lut_index, index, delta);
}
)";

    if (use_fragment_shader_barycentric) {
        out += R"(
bool AreQuaternionsOpposite(vec4 qa, vec4 qb) {
    return (dot(qa, qb) < 0.0);
}
)";
    }
}

void FragmentModule::DefineShadowHelpers() {
    if (config.framebuffer.shadow_rendering) {
        out += R"(
uint EncodeShadow(uvec2 pixel) {
    return (pixel.x << 8) | pixel.y;
}

uint UpdateShadow(uint pixel, uint d, uint s) {
    uvec2 ref = DecodeShadow(pixel);
    if (d < ref.x) {
        if (s == 0u) {
            ref.x = d;
        } else {
            s = uint(float(s) / (shadow_bias_constant + shadow_bias_linear * float(d) / float(ref.x)));
            ref.y = min(s, ref.y);
        }
    }
    return EncodeShadow(ref);
}
)";
    }

    if (config.texture.texture0_type == TexturingRegs::TextureConfig::Shadow2D ||
        config.texture.texture0_type == TexturingRegs::TextureConfig::ShadowCube) {
        out += R"(
float CompareShadow(uint pixel, uint z) {
    uvec2 p = DecodeShadow(pixel);
    return mix(float(p.y) * (1.0 / 255.0), 0.0, p.x <= z);
}

float mix2(vec4 s, vec2 a) {
    vec2 t = mix(s.xy, s.zw, a.yy);
    return mix(t.x, t.y, a.x);
}
)";

        if (config.texture.texture0_type == TexturingRegs::TextureConfig::Shadow2D) {
            if (profile.is_vulkan) {
                out += R"(
float SampleShadow2D(ivec2 uv, uint z) {
    if (any(bvec4(lessThan(uv, ivec2(0)), greaterThanEqual(uv, textureSize(tex0, 0)))))
        return 1.0;
    return CompareShadow(texelFetch(tex0, uv, 0).x, z);
}

vec4 shadowTexture(vec2 uv, float w) {
)";
                if (!config.texture.shadow_texture_orthographic) {
                    out += "uv /= w;";
                }
                out += R"(
    uint z = uint(max(0, int(min(abs(w), 1.0) * float(0xFFFFFF)) - shadow_texture_bias));
    vec2 coord = vec2(textureSize(tex0, 0)) * uv - vec2(0.5);
    vec2 coord_floor = floor(coord);
    vec2 f = coord - coord_floor;
    ivec2 i = ivec2(coord_floor);
    vec4 s = vec4(
        SampleShadow2D(i              , z),
        SampleShadow2D(i + ivec2(1, 0), z),
        SampleShadow2D(i + ivec2(0, 1), z),
        SampleShadow2D(i + ivec2(1, 1), z));
    return vec4(mix2(s, f));
}
)";

            } else {
                out += R"(
float SampleShadow2D(ivec2 uv, uint z) {
    if (any(bvec4(lessThan(uv, ivec2(0)), greaterThanEqual(uv, imageSize(shadow_texture_px)))))
        return 1.0;
    return CompareShadow(imageLoad(shadow_texture_px, uv).x, z);
}

vec4 shadowTexture(vec2 uv, float w) {
)";
                if (!config.texture.shadow_texture_orthographic) {
                    out += "uv /= w;";
                }
                out += R"(
    uint z = uint(max(0, int(min(abs(w), 1.0) * float(0xFFFFFF)) - shadow_texture_bias));
    vec2 coord = vec2(imageSize(shadow_texture_px)) * uv - vec2(0.5);
    vec2 coord_floor = floor(coord);
    vec2 f = coord - coord_floor;
    ivec2 i = ivec2(coord_floor);
    vec4 s = vec4(
        SampleShadow2D(i              , z),
        SampleShadow2D(i + ivec2(1, 0), z),
        SampleShadow2D(i + ivec2(0, 1), z),
        SampleShadow2D(i + ivec2(1, 1), z));
    return vec4(mix2(s, f));
}
)";
            }
        } else if (config.texture.texture0_type == TexturingRegs::TextureConfig::ShadowCube) {
            if (profile.is_vulkan) {
                out += R"(
uvec4 SampleShadowCube(int face, ivec2 i00, ivec2 i10, ivec2 i01, ivec2 i11) {
    return uvec4(
        texelFetch(tex0[face], i00, 0).r,
        texelFetch(tex0[face], i10, 0).r,
        texelFetch(tex0[face], i01, 0).r,
        texelFetch(tex0[face], i11, 0).r);
}

vec4 shadowTextureCube(vec2 uv, float w) {
    ivec2 size = textureSize(tex0[0], 0);
    vec3 c = vec3(uv, w);
    vec3 a = abs(c);
    if (a.x > a.y && a.x > a.z) {
        w = a.x;
        uv = -c.zy;
        if (c.x < 0.0) uv.x = -uv.x;
    } else if (a.y > a.z) {
        w = a.y;
        uv = c.xz;
        if (c.y < 0.0) uv.y = -uv.y;
    } else {
        w = a.z;
        uv = -c.xy;
        if (c.z > 0.0) uv.x = -uv.x;
    }
    uint z = uint(max(0, int(min(w, 1.0) * float(0xFFFFFF)) - shadow_texture_bias));
    vec2 coord = vec2(size) * (uv / w * vec2(0.5) + vec2(0.5)) - vec2(0.5);
    vec2 coord_floor = floor(coord);
    vec2 f = coord - coord_floor;
    ivec2 i00 = ivec2(coord_floor);
    ivec2 i10 = i00 + ivec2(1, 0);
    ivec2 i01 = i00 + ivec2(0, 1);
    ivec2 i11 = i00 + ivec2(1, 1);
    ivec2 cmin = ivec2(0), cmax = size - ivec2(1, 1);
    i00 = clamp(i00, cmin, cmax);
    i10 = clamp(i10, cmin, cmax);
    i01 = clamp(i01, cmin, cmax);
    i11 = clamp(i11, cmin, cmax);
    uvec4 pixels;
    if (a.x > a.y && a.x > a.z) {
        if (c.x > 0.0)
            pixels = SampleShadowCube(0, i00, i10, i01, i11);
        else
            pixels = SampleShadowCube(1, i00, i10, i01, i11);
    } else if (a.y > a.z) {
        if (c.y > 0.0)
            pixels = SampleShadowCube(2, i00, i10, i01, i11);
        else
            pixels = SampleShadowCube(3, i00, i10, i01, i11);
    } else {
        if (c.z > 0.0)
            pixels = SampleShadowCube(4, i00, i10, i01, i11);
        else
            pixels = SampleShadowCube(5, i00, i10, i01, i11);
    }
    vec4 s = vec4(
        CompareShadow(pixels.x, z),
        CompareShadow(pixels.y, z),
        CompareShadow(pixels.z, z),
        CompareShadow(pixels.w, z));
    return vec4(mix2(s, f));
}
    )";
            } else {
                out += R"(
vec4 shadowTextureCube(vec2 uv, float w) {
    ivec2 size = imageSize(shadow_texture_px);
    vec3 c = vec3(uv, w);
    vec3 a = abs(c);
    if (a.x > a.y && a.x > a.z) {
        w = a.x;
        uv = -c.zy;
        if (c.x < 0.0) uv.x = -uv.x;
    } else if (a.y > a.z) {
        w = a.y;
        uv = c.xz;
        if (c.y < 0.0) uv.y = -uv.y;
    } else {
        w = a.z;
        uv = -c.xy;
        if (c.z > 0.0) uv.x = -uv.x;
    }
    uint z = uint(max(0, int(min(w, 1.0) * float(0xFFFFFF)) - shadow_texture_bias));
    vec2 coord = vec2(size) * (uv / w * vec2(0.5) + vec2(0.5)) - vec2(0.5);
    vec2 coord_floor = floor(coord);
    vec2 f = coord - coord_floor;
    ivec2 i00 = ivec2(coord_floor);
    ivec2 i10 = i00 + ivec2(1, 0);
    ivec2 i01 = i00 + ivec2(0, 1);
    ivec2 i11 = i00 + ivec2(1, 1);
    ivec2 cmin = ivec2(0), cmax = size - ivec2(1, 1);
    i00 = clamp(i00, cmin, cmax);
    i10 = clamp(i10, cmin, cmax);
    i01 = clamp(i01, cmin, cmax);
    i11 = clamp(i11, cmin, cmax);
    uvec4 pixels;
    // This part should have been refactored into functions,
    // but many drivers don't like passing uimage2D as parameters
    if (a.x > a.y && a.x > a.z) {
        if (c.x > 0.0)
            pixels = uvec4(
                imageLoad(shadow_texture_px, i00).r,
                imageLoad(shadow_texture_px, i10).r,
                imageLoad(shadow_texture_px, i01).r,
                imageLoad(shadow_texture_px, i11).r);
        else
            pixels = uvec4(
                imageLoad(shadow_texture_nx, i00).r,
                imageLoad(shadow_texture_nx, i10).r,
                imageLoad(shadow_texture_nx, i01).r,
                imageLoad(shadow_texture_nx, i11).r);
    } else if (a.y > a.z) {
        if (c.y > 0.0)
            pixels = uvec4(
                imageLoad(shadow_texture_py, i00).r,
                imageLoad(shadow_texture_py, i10).r,
                imageLoad(shadow_texture_py, i01).r,
                imageLoad(shadow_texture_py, i11).r);
        else
            pixels = uvec4(
                imageLoad(shadow_texture_ny, i00).r,
                imageLoad(shadow_texture_ny, i10).r,
                imageLoad(shadow_texture_ny, i01).r,
                imageLoad(shadow_texture_ny, i11).r);
    } else {
        if (c.z > 0.0)
            pixels = uvec4(
                imageLoad(shadow_texture_pz, i00).r,
                imageLoad(shadow_texture_pz, i10).r,
                imageLoad(shadow_texture_pz, i01).r,
                imageLoad(shadow_texture_pz, i11).r);
        else
            pixels = uvec4(
                imageLoad(shadow_texture_nz, i00).r,
                imageLoad(shadow_texture_nz, i10).r,
                imageLoad(shadow_texture_nz, i01).r,
                imageLoad(shadow_texture_nz, i11).r);
    }
    vec4 s = vec4(
        CompareShadow(pixels.x, z),
        CompareShadow(pixels.y, z),
        CompareShadow(pixels.z, z),
        CompareShadow(pixels.w, z));
    return vec4(mix2(s, f));
}
    )";
            }
        }
    }
}

void FragmentModule::DefineTexUnitSampler(u32 texture_unit) {
    out += fmt::format("vec4 sampleTexUnit{}() {{\n", texture_unit);
    if (texture_unit == 0 &&
        config.texture.texture0_type == TexturingRegs::TextureConfig::Disabled) {
        out += "return vec4(0.0);\n}";
        return;
    }

    if (texture_unit < 3) {
        const u32 texcoord_num =
            texture_unit == 2 && config.texture.texture2_use_coord1 ? 1 : texture_unit;
        if (config.texture.texture_border_color[texture_unit].enable_s) {
            out += fmt::format(R"(
                if (texcoord{}.x < 0 || texcoord{}.x > 1) {{
                    return tex_border_color[{}];
                }}
                )",
                               texcoord_num, texcoord_num, texture_unit);
        }
        if (config.texture.texture_border_color[texture_unit].enable_t) {
            out += fmt::format(R"(
                if (texcoord{}.y < 0 || texcoord{}.y > 1) {{
                    return tex_border_color[{}];
                }}
                )",
                               texcoord_num, texcoord_num, texture_unit);
        }
    }

    switch (texture_unit) {
    case 0:
        switch (config.texture.texture0_type) {
        case TexturingRegs::TextureConfig::Texture2D:
            out += "return textureLod(tex0, texcoord0, getLod(texcoord0 * "
                   "vec2(textureSize(tex0, 0))) + tex_lod_bias[0]);";
            break;
        case TexturingRegs::TextureConfig::Projection2D:
            // TODO (wwylele): find the exact LOD formula for projection texture
            out += "return textureProj(tex0, vec3(texcoord0, texcoord0_w));";
            break;
        case TexturingRegs::TextureConfig::TextureCube:
            out += "return texture(tex0, vec3(texcoord0, texcoord0_w));";
            break;
        case TexturingRegs::TextureConfig::Shadow2D:
            out += "return shadowTexture(texcoord0, texcoord0_w);";
            break;
        case TexturingRegs::TextureConfig::ShadowCube:
            out += "return shadowTextureCube(texcoord0, texcoord0_w);";
            break;
        default:
            LOG_CRITICAL(HW_GPU, "Unhandled texture type {:x}",
                         config.texture.texture0_type.Value());
            UNIMPLEMENTED();
            out += "return texture(tex0, texcoord0);";
            break;
        }
        break;
    case 1:
        out += "return textureLod(tex1, texcoord1, getLod(texcoord1 * vec2(textureSize(tex1, "
               "0))) + tex_lod_bias[1]);";
        break;
    case 2:
        if (config.texture.texture2_use_coord1) {
            out += "return textureLod(tex2, texcoord1, getLod(texcoord1 * "
                   "vec2(textureSize(tex2, 0))) + tex_lod_bias[2]);";
        } else {
            out += "return textureLod(tex2, texcoord2, getLod(texcoord2 * "
                   "vec2(textureSize(tex2, 0))) + tex_lod_bias[2]);";
        }
        break;
    case 3:
        if (config.proctex.enable) {
            out += "return ProcTex();";
        } else {
            out += "return vec4(0.0);";
        }
        break;
    default:
        UNREACHABLE();
        break;
    }

    out += "\n}\n";
}

std::string GenerateFragmentShader(const FSConfig& config, const Profile& profile) {
    FragmentModule module{config, profile};
    return module.Generate();
}

} // namespace Pica::Shader::Generator::GLSL
