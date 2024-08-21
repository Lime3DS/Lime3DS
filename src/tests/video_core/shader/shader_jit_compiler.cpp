// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/arch.h"
#if LIME3DS_ARCH(x86_64) || LIME3DS_ARCH(arm64)

#include <algorithm>
#include <cmath>
#include <memory>
#include <span>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <nihstro/inline_assembly.h>
#include "video_core/pica/shader_setup.h"
#include "video_core/pica/shader_unit.h"
#include "video_core/shader/shader_interpreter.h"
#if LIME3DS_ARCH(x86_64)
#include "video_core/shader/shader_jit_x64_compiler.h"
#elif LIME3DS_ARCH(arm64)
#include "video_core/shader/shader_jit_a64_compiler.h"
#endif

using JitShader = Pica::Shader::JitShader;
using ShaderInterpreter = Pica::Shader::InterpreterEngine;

using DestRegister = nihstro::DestRegister;
using OpCode = nihstro::OpCode;
using SourceRegister = nihstro::SourceRegister;
using Type = nihstro::InlineAsm::Type;

static constexpr Common::Vec4f vec4_inf = Common::Vec4f::AssignToAll(INFINITY);
static constexpr Common::Vec4f vec4_nan = Common::Vec4f::AssignToAll(NAN);
static constexpr Common::Vec4f vec4_one = Common::Vec4f::AssignToAll(1.0f);
static constexpr Common::Vec4f vec4_zero = Common::Vec4f::AssignToAll(0.0f);

namespace Catch {
template <>
struct StringMaker<Common::Vec2f> {
    static std::string convert(Common::Vec2f value) {
        return fmt::format("({}, {})", value.x, value.y);
    }
};
template <>
struct StringMaker<Common::Vec3f> {
    static std::string convert(Common::Vec3f value) {
        return fmt::format("({}, {}, {})", value.r(), value.g(), value.b());
    }
};
template <>
struct StringMaker<Common::Vec4f> {
    static std::string convert(Common::Vec4f value) {
        return fmt::format("({}, {}, {}, {})", value.r(), value.g(), value.b(), value.a());
    }
};
} // namespace Catch

static std::unique_ptr<Pica::ShaderSetup> CompileShaderSetup(
    std::initializer_list<nihstro::InlineAsm> code) {
    const auto shbin = nihstro::InlineAsm::CompileToRawBinary(code);

    auto shader = std::make_unique<Pica::ShaderSetup>();

    std::transform(shbin.program.begin(), shbin.program.end(), shader->program_code.begin(),
                   [](const auto& x) { return x.hex; });
    std::transform(shbin.swizzle_table.begin(), shbin.swizzle_table.end(),
                   shader->swizzle_data.begin(), [](const auto& x) { return x.hex; });

    return shader;
}

class ShaderTest {
public:
    explicit ShaderTest(std::initializer_list<nihstro::InlineAsm> code)
        : shader_setup(CompileShaderSetup(code)) {
        shader_jit.Compile(&shader_setup->program_code, &shader_setup->swizzle_data);
    }

    explicit ShaderTest(std::unique_ptr<Pica::ShaderSetup> input_shader_setup)
        : shader_setup(std::move(input_shader_setup)) {
        shader_jit.Compile(&shader_setup->program_code, &shader_setup->swizzle_data);
    }

    Common::Vec4f Run(std::span<const Common::Vec4f> inputs) {
        Pica::ShaderUnit shader_unit;
        RunJit(shader_unit, inputs);
        return {shader_unit.output[0].x.ToFloat32(), shader_unit.output[0].y.ToFloat32(),
                shader_unit.output[0].z.ToFloat32(), shader_unit.output[0].w.ToFloat32()};
    }

    Common::Vec4f Run(std::initializer_list<float> inputs) {
        std::vector<Common::Vec4f> input_vecs;
        for (const float& input : inputs) {
            input_vecs.emplace_back(input, 0.0f, 0.0f, 0.0f);
        }
        return Run(input_vecs);
    }

    Common::Vec4f Run(float input) {
        return Run({input});
    }

    Common::Vec4f Run(std::initializer_list<Common::Vec4f> inputs) {
        return Run(std::vector<Common::Vec4f>{inputs});
    }

    void RunJit(Pica::ShaderUnit& shader_unit, std::span<const Common::Vec4f> inputs) {
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const Common::Vec4f& input = inputs[i];
            shader_unit.input[i].x = Pica::f24::FromFloat32(input.x);
            shader_unit.input[i].y = Pica::f24::FromFloat32(input.y);
            shader_unit.input[i].z = Pica::f24::FromFloat32(input.z);
            shader_unit.input[i].w = Pica::f24::FromFloat32(input.w);
        }
        shader_unit.temporary.fill(Common::Vec4<Pica::f24>::AssignToAll(Pica::f24::Zero()));
        shader_jit.Run(*shader_setup, shader_unit, 0);
    }

    void RunJit(Pica::ShaderUnit& shader_unit, float input) {
        const Common::Vec4f input_vec(input, 0, 0, 0);
        RunJit(shader_unit, {&input_vec, 1});
    }

    void RunInterpreter(Pica::ShaderUnit& shader_unit, std::span<const Common::Vec4f> inputs) {
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const Common::Vec4f& input = inputs[i];
            shader_unit.input[i].x = Pica::f24::FromFloat32(input.x);
            shader_unit.input[i].y = Pica::f24::FromFloat32(input.y);
            shader_unit.input[i].z = Pica::f24::FromFloat32(input.z);
            shader_unit.input[i].w = Pica::f24::FromFloat32(input.w);
        }
        shader_unit.temporary.fill(Common::Vec4<Pica::f24>::AssignToAll(Pica::f24::Zero()));
        shader_interpreter.Run(*shader_setup, shader_unit);
    }

    void RunInterpreter(Pica::ShaderUnit& shader_unit, float input) {
        const Common::Vec4f input_vec(input, 0, 0, 0);
        RunInterpreter(shader_unit, {&input_vec, 1});
    }

public:
    JitShader shader_jit;
    ShaderInterpreter shader_interpreter;
    std::unique_ptr<Pica::ShaderSetup> shader_setup;
};

TEST_CASE("ADD", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::ADD, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({+1.0f, -1.0f}).x == +0.0f);
    REQUIRE(shader.Run({+0.0f, -0.0f}).x == -0.0f);
    REQUIRE(std::isnan(shader.Run({+INFINITY, -INFINITY}).x));
    REQUIRE(std::isinf(shader.Run({INFINITY, +1.0f}).x));
    REQUIRE(std::isinf(shader.Run({INFINITY, -1.0f}).x));
}

TEST_CASE("CALL", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader_setup = CompileShaderSetup({
        {OpCode::Id::NOP}, // call foo
        {OpCode::Id::END},
        // .proc foo
        {OpCode::Id::NOP}, // call ex2
        {OpCode::Id::END},
        // .proc ex2
        {OpCode::Id::EX2, sh_output, sh_input},
        {OpCode::Id::END},
    });

    // nihstro does not support the CALL* instructions, so the instruction-binary must be manually
    // inserted here:
    nihstro::Instruction CALL = {};
    CALL.opcode = nihstro::OpCode(nihstro::OpCode::Id::CALL);

    // call foo
    CALL.flow_control.dest_offset = 2;
    CALL.flow_control.num_instructions = 1;
    shader_setup->program_code[0] = CALL.hex;

    // call ex2
    CALL.flow_control.dest_offset = 4;
    CALL.flow_control.num_instructions = 1;
    shader_setup->program_code[2] = CALL.hex;

    auto shader = ShaderTest(std::move(shader_setup));

    REQUIRE(shader.Run(0.f).x == Catch::Approx(1.f));
}

TEST_CASE("DP3", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::DP3, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({vec4_inf, vec4_zero}).x == 0.0f);
    REQUIRE(std::isnan(shader.Run({vec4_nan, vec4_zero}).x));

    REQUIRE(shader.Run({vec4_one, vec4_one}).x == 3.0f);
}

TEST_CASE("DP4", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::DP4, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({vec4_inf, vec4_zero}).x == 0.0f);
    REQUIRE(std::isnan(shader.Run({vec4_nan, vec4_zero}).x));

    REQUIRE(shader.Run({vec4_one, vec4_one}).x == 4.0f);
}

TEST_CASE("DPH", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::DPH, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({vec4_inf, vec4_zero}).x == 0.0f);
    REQUIRE(std::isnan(shader.Run({vec4_nan, vec4_zero}).x));

    REQUIRE(shader.Run({vec4_one, vec4_one}).x == 4.0f);
    REQUIRE(shader.Run({vec4_zero, vec4_one}).x == 1.0f);
}

TEST_CASE("LG2", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::LG2, sh_output, sh_input},
        {OpCode::Id::END},
    });

    REQUIRE(std::isnan(shader.Run(NAN).x));
    REQUIRE(std::isnan(shader.Run(-1.f).x));
    REQUIRE(std::isinf(shader.Run(0.f).x));
    REQUIRE(shader.Run(4.f).x == Catch::Approx(2.f));
    REQUIRE(shader.Run(64.f).x == Catch::Approx(6.f));
    REQUIRE(shader.Run(1.e24f).x == Catch::Approx(79.7262742773f));
}

TEST_CASE("EX2", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::EX2, sh_output, sh_input},
        {OpCode::Id::END},
    });

    REQUIRE(std::isnan(shader.Run(NAN).x));
    REQUIRE(shader.Run(-800.f).x == Catch::Approx(0.f));
    REQUIRE(shader.Run(0.f).x == Catch::Approx(1.f));
    REQUIRE(shader.Run(2.f).x == Catch::Approx(4.f));
    REQUIRE(shader.Run(6.f).x == Catch::Approx(64.f));
    REQUIRE(shader.Run(79.7262742773f).x == Catch::Approx(1.e24f));
    REQUIRE(std::isinf(shader.Run(800.f).x));
}

TEST_CASE("MUL", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::MUL, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({+1.0f, -1.0f}).x == -1.0f);
    REQUIRE(shader.Run({-1.0f, +1.0f}).x == -1.0f);

    REQUIRE(shader.Run({INFINITY, 0.0f}).x == 0.0f);
    REQUIRE(std::isnan(shader.Run({NAN, 0.0f}).x));
    REQUIRE(shader.Run({+INFINITY, +INFINITY}).x == INFINITY);
    REQUIRE(shader.Run({+INFINITY, -INFINITY}).x == -INFINITY);
}

TEST_CASE("SGE", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::SGE, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({INFINITY, 0.0f}).x == 1.0f);
    REQUIRE(shader.Run({0.0f, INFINITY}).x == 0.0f);
    REQUIRE(shader.Run({NAN, 0.0f}).x == 0.0f);
    REQUIRE(shader.Run({0.0f, NAN}).x == 0.0f);
    REQUIRE(shader.Run({+INFINITY, +INFINITY}).x == 1.0f);
    REQUIRE(shader.Run({+INFINITY, -INFINITY}).x == 1.0f);
    REQUIRE(shader.Run({-INFINITY, +INFINITY}).x == 0.0f);
    REQUIRE(shader.Run({+1.0f, -1.0f}).x == 1.0f);
    REQUIRE(shader.Run({-1.0f, +1.0f}).x == 0.0f);
}

TEST_CASE("SLT", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::SLT, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({INFINITY, 0.0f}).x == 0.0f);
    REQUIRE(shader.Run({0.0f, INFINITY}).x == 1.0f);
    REQUIRE(shader.Run({NAN, 0.0f}).x == 0.0f);
    REQUIRE(shader.Run({0.0f, NAN}).x == 0.0f);
    REQUIRE(shader.Run({+INFINITY, +INFINITY}).x == 0.0f);
    REQUIRE(shader.Run({+INFINITY, -INFINITY}).x == 0.0f);
    REQUIRE(shader.Run({-INFINITY, +INFINITY}).x == 1.0f);
    REQUIRE(shader.Run({+1.0f, -1.0f}).x == 0.0f);
    REQUIRE(shader.Run({-1.0f, +1.0f}).x == 1.0f);
}

TEST_CASE("FLR", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::FLR, sh_output, sh_input1},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({0.5}).x == 0.0f);
    REQUIRE(shader.Run({-0.5}).x == -1.0f);
    REQUIRE(shader.Run({1.5}).x == 1.0f);
    REQUIRE(shader.Run({-1.5}).x == -2.0f);
    REQUIRE(std::isnan(shader.Run({NAN}).x));
    REQUIRE(std::isinf(shader.Run({INFINITY}).x));
}

TEST_CASE("MAX", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::MAX, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({1.0f, 0.0f}).x == 1.0f);
    REQUIRE(shader.Run({0.0f, 1.0f}).x == 1.0f);
    REQUIRE(shader.Run({0.0f, +INFINITY}).x == +INFINITY);
    // REQUIRE(shader.Run({0.0f, -INFINITY}).x == -INFINITY); // TODO: 3dbrew says this is -INFINITY
    REQUIRE(std::isnan(shader.Run({0.0f, NAN}).x));
    REQUIRE(shader.Run({NAN, 0.0f}).x == 0.0f);
    REQUIRE(shader.Run({-INFINITY, +INFINITY}).x == +INFINITY);
}

TEST_CASE("MIN", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::MIN, sh_output, sh_input1, sh_input2},
        {OpCode::Id::END},
    });

    REQUIRE(shader.Run({1.0f, 0.0f}).x == 0.0f);
    REQUIRE(shader.Run({0.0f, 1.0f}).x == 0.0f);
    REQUIRE(shader.Run({0.0f, +INFINITY}).x == 0.0f);
    REQUIRE(shader.Run({0.0f, -INFINITY}).x == -INFINITY);
    REQUIRE(std::isnan(shader.Run({0.0f, NAN}).x));
    REQUIRE(shader.Run({NAN, 0.0f}).x == 0.0f);
    REQUIRE(shader.Run({-INFINITY, +INFINITY}).x == -INFINITY);
}

TEST_CASE("RCP", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::RCP, sh_output, sh_input},
        {OpCode::Id::END},
    });

    // REQUIRE(shader.Run({-0.0f}).x == INFINITY); // Violates IEEE
    REQUIRE(shader.Run({0.0f}).x == INFINITY);
    REQUIRE(shader.Run({INFINITY}).x == 0.0f);
    REQUIRE(std::isnan(shader.Run({NAN}).x));

    REQUIRE(shader.Run({16.0f}).x == Catch::Approx(0.0625f).margin(0.001f));
    REQUIRE(shader.Run({8.0f}).x == Catch::Approx(0.125f).margin(0.001f));
    REQUIRE(shader.Run({4.0f}).x == Catch::Approx(0.25f).margin(0.001f));
    REQUIRE(shader.Run({2.0f}).x == Catch::Approx(0.5f).margin(0.001f));
    REQUIRE(shader.Run({1.0f}).x == Catch::Approx(1.0f).margin(0.001f));
    REQUIRE(shader.Run({0.5f}).x == Catch::Approx(2.0f).margin(0.001f));
    REQUIRE(shader.Run({0.25f}).x == Catch::Approx(4.0f).margin(0.001f));
    REQUIRE(shader.Run({0.125f}).x == Catch::Approx(8.0f).margin(0.002f));
    REQUIRE(shader.Run({0.0625f}).x == Catch::Approx(16.0f).margin(0.004f));
}

TEST_CASE("RSQ", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        {OpCode::Id::RSQ, sh_output, sh_input},
        {OpCode::Id::END},
    });

    // REQUIRE(shader.Run({-0.0f}).x == INFINITY); // Violates IEEE
    REQUIRE(std::isnan(shader.Run({-2.0f}).x));
    REQUIRE(shader.Run({INFINITY}).x == 0.0f);
    REQUIRE(std::isnan(shader.Run({-INFINITY}).x));
    REQUIRE(std::isnan(shader.Run({NAN}).x));

    REQUIRE(shader.Run({16.0f}).x == Catch::Approx(0.25f).margin(0.001f));
    REQUIRE(shader.Run({8.0f}).x == Catch::Approx(1.0f / std::sqrt(8.0f)).margin(0.001f));
    REQUIRE(shader.Run({4.0f}).x == Catch::Approx(0.5f).margin(0.001f));
    REQUIRE(shader.Run({2.0f}).x == Catch::Approx(1.0f / std::sqrt(2.0f)).margin(0.001f));
    REQUIRE(shader.Run({1.0f}).x == Catch::Approx(1.0f).margin(0.001f));
    REQUIRE(shader.Run({0.5f}).x == Catch::Approx(1.0f / std::sqrt(0.5f)).margin(0.001f));
    REQUIRE(shader.Run({0.25f}).x == Catch::Approx(2.0f).margin(0.001f));
    REQUIRE(shader.Run({0.125f}).x == Catch::Approx(1.0 / std::sqrt(0.125)).margin(0.002f));
    REQUIRE(shader.Run({0.0625f}).x == Catch::Approx(4.0f).margin(0.004f));
}

TEST_CASE("Uniform Read", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_c0 = SourceRegister::MakeFloat(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        // mova a0.x, sh_input.x
        {OpCode::Id::MOVA, DestRegister{}, "x", sh_input, "x", SourceRegister{}, "",
         nihstro::InlineAsm::RelativeAddress::A1},
        // mov sh_output.xyzw, c0[a0.x].xyzw
        {OpCode::Id::MOV, sh_output, "xyzw", sh_c0, "xyzw", SourceRegister{}, "",
         nihstro::InlineAsm::RelativeAddress::A1},
        {OpCode::Id::END},
    });

    // Prepare shader uniforms
    std::array<Common::Vec4f, 96> f_uniforms = {};
    for (u32 i = 0; i < 96; ++i) {
        const float color = (i * 2.0f) / 255.0f;
        const auto color_f24 = Pica::f24::FromFloat32(color);
        shader.shader_setup->uniforms.f[i] = {color_f24, color_f24, color_f24, Pica::f24::One()};
        f_uniforms[i] = {color, color, color, 1.0f};
    }

    for (u32 i = 0; i < 96; ++i) {
        const float index = static_cast<float>(i);
        // Add some fractional values to test proper float->integer truncation
        const float fractional = (i % 17) / 17.0f;

        REQUIRE(shader.Run(index + fractional) == f_uniforms[i]);
    }
}

TEST_CASE("Address Register Offset", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_c40 = SourceRegister::MakeFloat(40);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader = ShaderTest({
        // mova a0.x, sh_input.x
        {OpCode::Id::MOVA, DestRegister{}, "x", sh_input, "x", SourceRegister{}, "",
         nihstro::InlineAsm::RelativeAddress::A1},
        // mov sh_output.xyzw, c40[a0.x].xyzw
        {OpCode::Id::MOV, sh_output, "xyzw", sh_c40, "xyzw", SourceRegister{}, "",
         nihstro::InlineAsm::RelativeAddress::A1},
        {OpCode::Id::END},
    });

    // Prepare shader uniforms
    const bool inverted = true;
    std::array<Common::Vec4f, 96> f_uniforms;
    for (u32 i = 0; i < 0x80; i++) {
        if (i >= 0x00 && i < 0x60) {
            const u32 base = inverted ? (0x60 - i) : i;
            const auto color = (base * 2.f) / 255.0f;
            const auto color_f24 = Pica::f24::FromFloat32(color);
            shader.shader_setup->uniforms.f[i] = {color_f24, color_f24, color_f24,
                                                  Pica::f24::One()};
            f_uniforms[i] = {color, color, color, 1.f};
        } else if (i >= 0x60 && i < 0x64) {
            const u8 color = static_cast<u8>((i - 0x60) * 0x10);
            shader.shader_setup->uniforms.i[i - 0x60] = {color, color, color, 255};
        } else if (i >= 0x70 && i < 0x80) {
            shader.shader_setup->uniforms.b[i - 0x70] = i >= 0x78;
        }
    }

    REQUIRE(shader.Run(0.f) == f_uniforms[40]);
    REQUIRE(shader.Run(13.f) == f_uniforms[53]);
    REQUIRE(shader.Run(50.f) == f_uniforms[90]);
    REQUIRE(shader.Run(60.f) == vec4_one);
    REQUIRE(shader.Run(74.f) == vec4_one);
    REQUIRE(shader.Run(87.f) == vec4_one);
    REQUIRE(shader.Run(88.f) == f_uniforms[0]);
    REQUIRE(shader.Run(128.f) == f_uniforms[40]);
    REQUIRE(shader.Run(-40.f) == f_uniforms[0]);
    REQUIRE(shader.Run(-42.f) == vec4_one);
    REQUIRE(shader.Run(-70.f) == vec4_one);
    REQUIRE(shader.Run(-73.f) == f_uniforms[95]);
    REQUIRE(shader.Run(-127.f) == f_uniforms[41]);
    REQUIRE(shader.Run(-129.f) == f_uniforms[40]);
}

TEST_CASE("Dest Mask", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    const auto shader = [&sh_input, &sh_output](const char* dest_mask) {
        return std::unique_ptr<ShaderTest>(new ShaderTest{
            {OpCode::Id::MOV, sh_output, dest_mask, sh_input, "xyzw", SourceRegister{}, ""},
            {OpCode::Id::END},
        });
    };

    const Common::Vec4f iota_vec = {1.0f, 2.0f, 3.0f, 4.0f};

    REQUIRE(shader("x")->Run({iota_vec}).x == iota_vec.x);
    REQUIRE(shader("y")->Run({iota_vec}).y == iota_vec.y);
    REQUIRE(shader("z")->Run({iota_vec}).z == iota_vec.z);
    REQUIRE(shader("w")->Run({iota_vec}).w == iota_vec.w);
    REQUIRE(shader("xy")->Run({iota_vec}).xy() == iota_vec.xy());
    REQUIRE(shader("xz")->Run({iota_vec}).xz() == iota_vec.xz());
    REQUIRE(shader("xw")->Run({iota_vec}).xw() == iota_vec.xw());
    REQUIRE(shader("yz")->Run({iota_vec}).yz() == iota_vec.yz());
    REQUIRE(shader("yw")->Run({iota_vec}).yw() == iota_vec.yw());
    REQUIRE(shader("zw")->Run({iota_vec}).zw() == iota_vec.zw());
    REQUIRE(shader("xyz")->Run({iota_vec}).xyz() == iota_vec.xyz());
    REQUIRE(shader("xyw")->Run({iota_vec}).xyw() == iota_vec.xyw());
    REQUIRE(shader("xzw")->Run({iota_vec}).xzw() == iota_vec.xzw());
    REQUIRE(shader("yzw")->Run({iota_vec}).yzw() == iota_vec.yzw());
    REQUIRE(shader("xyzw")->Run({iota_vec}) == iota_vec);
}

TEST_CASE("MAD", "[video_core][shader][shader_jit]") {
    const auto sh_input1 = SourceRegister::MakeInput(0);
    const auto sh_input2 = SourceRegister::MakeInput(1);
    const auto sh_input3 = SourceRegister::MakeInput(2);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader_setup = CompileShaderSetup({
        // TODO: Requires fix from https://github.com/neobrain/nihstro/issues/68
        // {OpCode::Id::MAD, sh_output, sh_input1, sh_input2, sh_input3},
        {OpCode::Id::NOP},
        {OpCode::Id::END},
    });

    // nihstro does not support the MAD* instructions, so the instruction-binary must be manually
    // inserted here:
    nihstro::Instruction MAD = {};
    MAD.opcode = nihstro::OpCode::Id::MAD;
    MAD.mad.operand_desc_id = 0;
    MAD.mad.src1 = sh_input1;
    MAD.mad.src2 = sh_input2;
    MAD.mad.src3 = sh_input3;
    MAD.mad.dest = sh_output;
    shader_setup->program_code[0] = MAD.hex;

    nihstro::SwizzlePattern swizzle = {};
    swizzle.dest_mask = 0b1111;
    swizzle.SetSelectorSrc1(0, SwizzlePattern::Selector::x);
    swizzle.SetSelectorSrc1(1, SwizzlePattern::Selector::y);
    swizzle.SetSelectorSrc1(2, SwizzlePattern::Selector::z);
    swizzle.SetSelectorSrc1(3, SwizzlePattern::Selector::w);
    swizzle.SetSelectorSrc2(0, SwizzlePattern::Selector::x);
    swizzle.SetSelectorSrc2(1, SwizzlePattern::Selector::y);
    swizzle.SetSelectorSrc2(2, SwizzlePattern::Selector::z);
    swizzle.SetSelectorSrc2(3, SwizzlePattern::Selector::w);
    swizzle.SetSelectorSrc3(0, SwizzlePattern::Selector::x);
    swizzle.SetSelectorSrc3(1, SwizzlePattern::Selector::y);
    swizzle.SetSelectorSrc3(2, SwizzlePattern::Selector::z);
    swizzle.SetSelectorSrc3(3, SwizzlePattern::Selector::w);
    shader_setup->swizzle_data[0] = swizzle.hex;

    auto shader = ShaderTest(std::move(shader_setup));

    REQUIRE(shader.Run({vec4_zero, vec4_zero, vec4_zero}) == vec4_zero);
    REQUIRE(shader.Run({vec4_one, vec4_one, vec4_one}) == (vec4_one * 2.0f));
    REQUIRE(shader.Run({vec4_inf, vec4_zero, vec4_zero}) == vec4_zero);
    REQUIRE(shader.Run({vec4_nan, vec4_zero, vec4_zero}) == vec4_nan);
}

TEST_CASE("Nested Loop", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_temp = SourceRegister::MakeTemporary(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    auto shader_test = ShaderTest({
        // clang-format off
        {OpCode::Id::MOV, sh_temp, sh_input},
        {OpCode::Id::LOOP, 0},
            {OpCode::Id::LOOP, 1},
                {OpCode::Id::ADD, sh_temp, sh_temp, sh_input},
            {Type::EndLoop},
        {Type::EndLoop},
        {OpCode::Id::MOV, sh_output, sh_temp},
        {OpCode::Id::END},
        // clang-format on
    });

    {
        shader_test.shader_setup->uniforms.i[0] = {4, 0, 1, 0};
        shader_test.shader_setup->uniforms.i[1] = {4, 0, 1, 0};
        Common::Vec4<u8> loop_parms{shader_test.shader_setup->uniforms.i[0]};

        const int expected_aL = loop_parms[1] + ((loop_parms[0] + 1) * loop_parms[2]);
        const float input = 1.0f;
        const float expected_out = (((shader_test.shader_setup->uniforms.i[0][0] + 1) *
                                     (shader_test.shader_setup->uniforms.i[1][0] + 1)) *
                                    input) +
                                   input;

        Pica::ShaderUnit shader_unit_jit;
        shader_test.RunJit(shader_unit_jit, input);

        REQUIRE(shader_unit_jit.address_registers[2] == expected_aL);
        REQUIRE(shader_unit_jit.output[0].x.ToFloat32() == Catch::Approx(expected_out));
    }
    {
        shader_test.shader_setup->uniforms.i[0] = {9, 0, 2, 0};
        shader_test.shader_setup->uniforms.i[1] = {7, 0, 1, 0};

        const Common::Vec4<u8> loop_parms{shader_test.shader_setup->uniforms.i[0]};
        const int expected_aL = loop_parms[1] + ((loop_parms[0] + 1) * loop_parms[2]);
        const float input = 1.0f;
        const float expected_out = (((shader_test.shader_setup->uniforms.i[0][0] + 1) *
                                     (shader_test.shader_setup->uniforms.i[1][0] + 1)) *
                                    input) +
                                   input;
        Pica::ShaderUnit shader_unit_jit;
        shader_test.RunJit(shader_unit_jit, input);

        REQUIRE(shader_unit_jit.address_registers[2] == expected_aL);
        REQUIRE(shader_unit_jit.output[0].x.ToFloat32() == Catch::Approx(expected_out));
    }
}

TEST_CASE("Source Swizzle", "[video_core][shader][shader_jit]") {
    const auto sh_input = SourceRegister::MakeInput(0);
    const auto sh_output = DestRegister::MakeOutput(0);

    const auto shader = [&sh_input, &sh_output](const char* swizzle) {
        return std::unique_ptr<ShaderTest>(new ShaderTest{
            {OpCode::Id::MOV, sh_output, "xyzw", sh_input, swizzle, SourceRegister{}, ""},
            {OpCode::Id::END},
        });
    };

    const Common::Vec4f iota_vec = {1.0f, 2.0f, 3.0f, 4.0f};

    REQUIRE(shader("x")->Run({iota_vec}).x == iota_vec.x);
    REQUIRE(shader("y")->Run({iota_vec}).x == iota_vec.y);
    REQUIRE(shader("z")->Run({iota_vec}).x == iota_vec.z);
    REQUIRE(shader("w")->Run({iota_vec}).x == iota_vec.w);
    REQUIRE(shader("xy")->Run({iota_vec}).xy() == iota_vec.xy());
    REQUIRE(shader("xz")->Run({iota_vec}).xy() == iota_vec.xz());
    REQUIRE(shader("xw")->Run({iota_vec}).xy() == iota_vec.xw());
    REQUIRE(shader("yz")->Run({iota_vec}).xy() == iota_vec.yz());
    REQUIRE(shader("yw")->Run({iota_vec}).xy() == iota_vec.yw());
    REQUIRE(shader("zw")->Run({iota_vec}).xy() == iota_vec.zw());
    REQUIRE(shader("yy")->Run({iota_vec}).xy() == iota_vec.yy());
    REQUIRE(shader("wx")->Run({iota_vec}).xy() == iota_vec.wx());
    REQUIRE(shader("xyz")->Run({iota_vec}).xyz() == iota_vec.xyz());
    REQUIRE(shader("xyw")->Run({iota_vec}).xyz() == iota_vec.xyw());
    REQUIRE(shader("xzw")->Run({iota_vec}).xyz() == iota_vec.xzw());
    REQUIRE(shader("yzw")->Run({iota_vec}).xyz() == iota_vec.yzw());
    REQUIRE(shader("yyy")->Run({iota_vec}).xyz() == iota_vec.yyy());
    REQUIRE(shader("yxw")->Run({iota_vec}).xyz() == iota_vec.yxw());
    REQUIRE(shader("xyzw")->Run({iota_vec}) == iota_vec);
    REQUIRE(shader("wzxy")->Run({iota_vec}) ==
            Common::Vec4f(iota_vec.w, iota_vec.z, iota_vec.x, iota_vec.y));
    REQUIRE(shader("yyyy")->Run({iota_vec}) ==
            Common::Vec4f(iota_vec.y, iota_vec.y, iota_vec.y, iota_vec.y));
}

#endif // LIME3DS_ARCH(x86_64) || LIME3DS_ARCH(arm64)
