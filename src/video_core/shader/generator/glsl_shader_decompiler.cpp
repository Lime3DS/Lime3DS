// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <exception>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <fmt/format.h>
#include <nihstro/shader_bytecode.h>
#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/shader/generator/glsl_shader_decompiler.h"

namespace Pica::Shader::Generator::GLSL {

using nihstro::DestRegister;
using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::RegisterType;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

constexpr u32 PROGRAM_END = MAX_PROGRAM_CODE_LENGTH;

class DecompileFail : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Describes the behaviour of code path of a given entry point and a return point.
enum class ExitMethod {
    Undetermined, ///< Internal value. Only occur when analyzing JMP loop.
    AlwaysReturn, ///< All code paths reach the return point.
    Conditional,  ///< Code path reaches the return point or an END instruction conditionally.
    AlwaysEnd,    ///< All code paths reach a END instruction.
};

/// A subroutine is a range of code refereced by a CALL, IF or LOOP instruction.
struct Subroutine {
    /// Generates a name suitable for GLSL source code.
    std::string GetName() const {
        return "sub_" + std::to_string(begin) + "_" + std::to_string(end);
    }

    u32 begin;              ///< Entry point of the subroutine.
    u32 end;                ///< Return point of the subroutine.
    ExitMethod exit_method; ///< Exit method of the subroutine.
    std::set<u32> labels;   ///< Addresses refereced by JMP instructions.

    bool operator<(const Subroutine& rhs) const {
        return std::tie(begin, end) < std::tie(rhs.begin, rhs.end);
    }
};

/// Analyzes shader code and produces a set of subroutines.
class ControlFlowAnalyzer {
public:
    ControlFlowAnalyzer(const ProgramCode& program_code, u32 main_offset)
        : program_code(program_code) {

        // Recursively finds all subroutines.
        const Subroutine& program_main = AddSubroutine(main_offset, PROGRAM_END);
        if (program_main.exit_method != ExitMethod::AlwaysEnd)
            throw DecompileFail("Program does not always end");
    }

    std::set<Subroutine> MoveSubroutines() {
        return std::move(subroutines);
    }

private:
    const ProgramCode& program_code;
    std::set<Subroutine> subroutines;
    std::map<std::pair<u32, u32>, ExitMethod> exit_method_map;

    /// Adds and analyzes a new subroutine if it is not added yet.
    const Subroutine& AddSubroutine(u32 begin, u32 end) {
        auto iter = subroutines.find(Subroutine{begin, end});
        if (iter != subroutines.end())
            return *iter;

        Subroutine subroutine{begin, end};
        subroutine.exit_method = Scan(begin, end, subroutine.labels);
        if (subroutine.exit_method == ExitMethod::Undetermined)
            throw DecompileFail("Recursive function detected");
        return *subroutines.insert(std::move(subroutine)).first;
    }

    /// Merges exit method of two parallel branches.
    static ExitMethod ParallelExit(ExitMethod a, ExitMethod b) {
        if (a == ExitMethod::Undetermined) {
            return b;
        }
        if (b == ExitMethod::Undetermined) {
            return a;
        }
        if (a == b) {
            return a;
        }
        return ExitMethod::Conditional;
    }

    /// Cascades exit method of two blocks of code.
    static ExitMethod SeriesExit(ExitMethod a, ExitMethod b) {
        // This should be handled before evaluating b.
        DEBUG_ASSERT(a != ExitMethod::AlwaysEnd);

        if (a == ExitMethod::Undetermined) {
            return ExitMethod::Undetermined;
        }

        if (a == ExitMethod::AlwaysReturn) {
            return b;
        }

        if (b == ExitMethod::Undetermined || b == ExitMethod::AlwaysEnd) {
            return ExitMethod::AlwaysEnd;
        }

        return ExitMethod::Conditional;
    }

    /// Scans a range of code for labels and determines the exit method.
    ExitMethod Scan(u32 begin, u32 end, std::set<u32>& labels) {
        auto [iter, inserted] =
            exit_method_map.emplace(std::make_pair(begin, end), ExitMethod::Undetermined);
        ExitMethod& exit_method = iter->second;
        if (!inserted)
            return exit_method;

        for (u32 offset = begin; offset != end && offset != PROGRAM_END; ++offset) {
            const Instruction instr = {program_code[offset]};
            switch (instr.opcode.Value()) {
            case OpCode::Id::END: {
                return exit_method = ExitMethod::AlwaysEnd;
            }
            case OpCode::Id::JMPC:
            case OpCode::Id::JMPU: {
                labels.insert(instr.flow_control.dest_offset);
                ExitMethod no_jmp = Scan(offset + 1, end, labels);
                ExitMethod jmp = Scan(instr.flow_control.dest_offset, end, labels);
                return exit_method = ParallelExit(no_jmp, jmp);
            }
            case OpCode::Id::CALL: {
                auto& call = AddSubroutine(instr.flow_control.dest_offset,
                                           instr.flow_control.dest_offset +
                                               instr.flow_control.num_instructions);
                if (call.exit_method == ExitMethod::AlwaysEnd)
                    return exit_method = ExitMethod::AlwaysEnd;
                ExitMethod after_call = Scan(offset + 1, end, labels);
                return exit_method = SeriesExit(call.exit_method, after_call);
            }
            case OpCode::Id::LOOP: {
                auto& loop = AddSubroutine(offset + 1, instr.flow_control.dest_offset + 1);
                if (loop.exit_method == ExitMethod::AlwaysEnd)
                    return exit_method = ExitMethod::AlwaysEnd;
                ExitMethod after_loop = Scan(instr.flow_control.dest_offset + 1, end, labels);
                return exit_method = SeriesExit(loop.exit_method, after_loop);
            }
            case OpCode::Id::CALLC:
            case OpCode::Id::CALLU: {
                auto& call = AddSubroutine(instr.flow_control.dest_offset,
                                           instr.flow_control.dest_offset +
                                               instr.flow_control.num_instructions);
                ExitMethod after_call = Scan(offset + 1, end, labels);
                return exit_method = SeriesExit(
                           ParallelExit(call.exit_method, ExitMethod::AlwaysReturn), after_call);
            }
            case OpCode::Id::IFU:
            case OpCode::Id::IFC: {
                auto& if_sub = AddSubroutine(offset + 1, instr.flow_control.dest_offset);
                ExitMethod else_method;
                if (instr.flow_control.num_instructions != 0) {
                    auto& else_sub = AddSubroutine(instr.flow_control.dest_offset,
                                                   instr.flow_control.dest_offset +
                                                       instr.flow_control.num_instructions);
                    else_method = else_sub.exit_method;
                } else {
                    else_method = ExitMethod::AlwaysReturn;
                }

                ExitMethod both = ParallelExit(if_sub.exit_method, else_method);
                if (both == ExitMethod::AlwaysEnd)
                    return exit_method = ExitMethod::AlwaysEnd;
                ExitMethod after_call =
                    Scan(instr.flow_control.dest_offset + instr.flow_control.num_instructions, end,
                         labels);
                return exit_method = SeriesExit(both, after_call);
            }
            default:
                break;
            }
        }
        return exit_method = ExitMethod::AlwaysReturn;
    }
};

class ShaderWriter {
public:
    // Forwards all arguments directly to libfmt.
    // Note that all formatting requirements for fmt must be
    // obeyed when using this function. (e.g. {{ must be used
    // printing the character '{' is desirable. Ditto for }} and '}',
    // etc).
    template <typename... Args>
    void AddLine(fmt::format_string<Args...> text, Args&&... args) {
        AddExpression(fmt::format(text, std::forward<Args>(args)...));
        AddNewLine();
    }

    void AddNewLine() {
        DEBUG_ASSERT(scope >= 0);
        shader_source += '\n';
    }

    std::string MoveResult() {
        return std::move(shader_source);
    }

    int scope = 0;

private:
    void AddExpression(std::string_view text) {
        if (!text.empty()) {
            shader_source.append(static_cast<std::size_t>(scope) * 4, ' ');
        }
        shader_source += text;
    }

    std::string shader_source;
};

/// An adaptor for getting swizzle pattern string from nihstro interfaces.
template <SwizzlePattern::Selector (SwizzlePattern::*getter)(int) const>
std::string GetSelectorSrc(const SwizzlePattern& pattern) {
    std::string out;
    for (int i = 0; i < 4; ++i) {
        switch ((pattern.*getter)(i)) {
        case SwizzlePattern::Selector::x:
            out += 'x';
            break;
        case SwizzlePattern::Selector::y:
            out += 'y';
            break;
        case SwizzlePattern::Selector::z:
            out += 'z';
            break;
        case SwizzlePattern::Selector::w:
            out += 'w';
            break;
        default:
            UNREACHABLE();
            return "";
        }
    }
    return out;
}

constexpr auto GetSelectorSrc1 = GetSelectorSrc<&SwizzlePattern::GetSelectorSrc1>;
constexpr auto GetSelectorSrc2 = GetSelectorSrc<&SwizzlePattern::GetSelectorSrc2>;
constexpr auto GetSelectorSrc3 = GetSelectorSrc<&SwizzlePattern::GetSelectorSrc3>;

class GLSLGenerator {
public:
    GLSLGenerator(const std::set<Subroutine>& subroutines, const ProgramCode& program_code,
                  const SwizzleData& swizzle_data, u32 main_offset,
                  const RegGetter& inputreg_getter, const RegGetter& outputreg_getter,
                  bool sanitize_mul)
        : subroutines(subroutines), program_code(program_code), swizzle_data(swizzle_data),
          main_offset(main_offset), inputreg_getter(inputreg_getter),
          outputreg_getter(outputreg_getter), sanitize_mul(sanitize_mul) {

        Generate();
    }

    std::string MoveShaderCode() {
        return shader.MoveResult();
    }

private:
    /// Gets the Subroutine object corresponding to the specified address.
    const Subroutine& GetSubroutine(u32 begin, u32 end) const {
        auto iter = subroutines.find(Subroutine{begin, end});
        ASSERT(iter != subroutines.end());
        return *iter;
    }

    /// Generates condition evaluation code for the flow control instruction.
    static std::string EvaluateCondition(Instruction::FlowControlType flow_control) {
        using Op = Instruction::FlowControlType::Op;

        const std::string_view result_x =
            flow_control.refx.Value() ? "conditional_code.x" : "!conditional_code.x";
        const std::string_view result_y =
            flow_control.refy.Value() ? "conditional_code.y" : "!conditional_code.y";

        switch (flow_control.op) {
        case Op::JustX:
            return std::string(result_x);
        case Op::JustY:
            return std::string(result_y);
        case Op::Or:
        case Op::And: {
            const std::string_view and_or = flow_control.op == Op::Or ? "any" : "all";
            std::string bvec;
            if (flow_control.refx.Value() && flow_control.refy.Value()) {
                bvec = "conditional_code";
            } else if (!flow_control.refx.Value() && !flow_control.refy.Value()) {
                bvec = "not(conditional_code)";
            } else {
                bvec = fmt::format("bvec2({}, {})", result_x, result_y);
            }
            return fmt::format("{}({})", and_or, bvec);
        }
        default:
            UNREACHABLE();
            return "";
        }
    }

    /// Generates code representing a source register.
    std::string GetSourceRegister(const SourceRegister& source_reg,
                                  u32 address_register_index) const {
        const u32 index = static_cast<u32>(source_reg.GetIndex());

        switch (source_reg.GetRegisterType()) {
        case RegisterType::Input:
            return inputreg_getter(index);
        case RegisterType::Temporary:
            return fmt::format("reg_tmp{}", index);
        case RegisterType::FloatUniform:
            if (address_register_index != 0) {
                return fmt::format("get_offset_register({}, address_registers.{})", index,
                                   "xyz"[address_register_index - 1]);
            }
            return fmt::format("uniforms.f[{}]", index);
        default:
            UNREACHABLE();
            return "";
        }
    }

    /// Generates code representing a destination register.
    std::string GetDestRegister(const DestRegister& dest_reg) const {
        const u32 index = static_cast<u32>(dest_reg.GetIndex());

        switch (dest_reg.GetRegisterType()) {
        case RegisterType::Output:
            return outputreg_getter(index);
        case RegisterType::Temporary:
            return fmt::format("reg_tmp{}", index);
        default:
            UNREACHABLE();
            return "";
        }
    }

    /// Generates code representing a bool uniform
    std::string GetUniformBool(u32 index) const {
        return fmt::format("uniforms.b[{}]", index);
    }

    /**
     * Adds code that calls a subroutine.
     * @param subroutine the subroutine to call.
     */
    void CallSubroutine(const Subroutine& subroutine) {
        if (subroutine.exit_method == ExitMethod::AlwaysEnd) {
            shader.AddLine("{}();", subroutine.GetName());
            shader.AddLine("return true;");
        } else if (subroutine.exit_method == ExitMethod::Conditional) {
            shader.AddLine("if ({}()) {{ return true; }}", subroutine.GetName());
        } else {
            shader.AddLine("{}();", subroutine.GetName());
        }
    }

    /**
     * Writes code that does an assignment operation.
     * @param swizzle the swizzle data of the current instruction.
     * @param reg the destination register code.
     * @param value the code representing the value to assign.
     * @param dest_num_components number of components of the destination register.
     * @param value_num_components number of components of the value to assign.
     */
    void SetDest(const SwizzlePattern& swizzle, std::string_view reg, std::string_view value,
                 u32 dest_num_components, u32 value_num_components) {
        u32 dest_mask_num_components = 0;
        std::string dest_mask_swizzle = ".";

        for (u32 i = 0; i < dest_num_components; ++i) {
            if (swizzle.DestComponentEnabled(static_cast<int>(i))) {
                dest_mask_swizzle += "xyzw"[i];
                ++dest_mask_num_components;
            }
        }

        if (reg.empty() || dest_mask_num_components == 0) {
            return;
        }
        DEBUG_ASSERT(value_num_components >= dest_num_components || value_num_components == 1);

        const std::string dest =
            fmt::format("{}{}", reg, dest_num_components != 1 ? dest_mask_swizzle : "");

        std::string src{value};
        if (value_num_components == 1) {
            if (dest_mask_num_components != 1) {
                src = fmt::format("vec{}({})", dest_mask_num_components, value);
            }
        } else if (value_num_components != dest_mask_num_components) {
            src = fmt::format("({}){}", value, dest_mask_swizzle);
        }

        shader.AddLine("{} = {};", dest, src);
    }

    /**
     * Compiles a single instruction from PICA to GLSL.
     * @param offset the offset of the PICA shader instruction.
     * @return the offset of the next instruction to execute. Usually it is the current offset + 1.
     * If the current instruction is IF or LOOP, the next instruction is after the IF or LOOP block.
     * If the current instruction always terminates the program, returns PROGRAM_END.
     */
    u32 CompileInstr(u32 offset) {
        const Instruction instr = {program_code[offset]};

        std::size_t swizzle_offset =
            instr.opcode.Value().GetInfo().type == OpCode::Type::MultiplyAdd
                ? instr.mad.operand_desc_id
                : instr.common.operand_desc_id;
        const SwizzlePattern swizzle = {swizzle_data[swizzle_offset]};

        shader.AddLine("// {}: {}", offset, instr.opcode.Value().GetInfo().name);

        switch (instr.opcode.Value().GetInfo().type) {
        case OpCode::Type::Arithmetic: {
            const bool is_inverted =
                (0 != (instr.opcode.Value().GetInfo().subtype & OpCode::Info::SrcInversed));

            std::string src1 = swizzle.negate_src1 ? "-" : "";
            src1 += GetSourceRegister(instr.common.GetSrc1(is_inverted),
                                      !is_inverted * instr.common.address_register_index);
            src1 += "." + GetSelectorSrc1(swizzle);

            std::string src2 = swizzle.negate_src2 ? "-" : "";
            src2 += GetSourceRegister(instr.common.GetSrc2(is_inverted),
                                      is_inverted * instr.common.address_register_index);
            src2 += "." + GetSelectorSrc2(swizzle);

            std::string dest_reg = GetDestRegister(instr.common.dest.Value());

            switch (instr.opcode.Value().EffectiveOpCode()) {
            case OpCode::Id::ADD: {
                SetDest(swizzle, dest_reg, fmt::format("{} + {}", src1, src2), 4, 4);
                break;
            }

            case OpCode::Id::MUL: {
                if (sanitize_mul) {
                    SetDest(swizzle, dest_reg, fmt::format("sanitize_mul({}, {})", src1, src2), 4,
                            4);
                } else {
                    SetDest(swizzle, dest_reg, fmt::format("{} * {}", src1, src2), 4, 4);
                }
                break;
            }

            case OpCode::Id::FLR: {
                SetDest(swizzle, dest_reg, fmt::format("floor({})", src1), 4, 4);
                break;
            }

            case OpCode::Id::MAX: {
                if (sanitize_mul) {
                    SetDest(swizzle, dest_reg,
                            fmt::format("mix({1}, {0}, greaterThan({0}, {1}))", src1, src2), 4, 4);
                } else {
                    SetDest(swizzle, dest_reg, fmt::format("max({}, {})", src1, src2), 4, 4);
                }
                break;
            }

            case OpCode::Id::MIN: {
                if (sanitize_mul) {
                    SetDest(swizzle, dest_reg,
                            fmt::format("mix({1}, {0}, lessThan({0}, {1}))", src1, src2), 4, 4);
                } else {
                    SetDest(swizzle, dest_reg, fmt::format("min({}, {})", src1, src2), 4, 4);
                }
                break;
            }

            case OpCode::Id::DP3:
            case OpCode::Id::DP4:
            case OpCode::Id::DPH:
            case OpCode::Id::DPHI: {
                OpCode::Id opcode = instr.opcode.Value().EffectiveOpCode();
                std::string dot;
                if (opcode == OpCode::Id::DP3) {
                    if (sanitize_mul) {
                        dot = fmt::format("dot(vec3(sanitize_mul({}, {})), vec3(1.0))", src1, src2);
                    } else {
                        dot = fmt::format("dot(vec3({}), vec3({}))", src1, src2);
                    }
                } else {
                    if (sanitize_mul) {
                        const std::string src1_ =
                            (opcode == OpCode::Id::DPH || opcode == OpCode::Id::DPHI)
                                ? fmt::format("vec4({}.xyz, 1.0)", src1)
                                : std::move(src1);

                        dot = fmt::format("dot(sanitize_mul({}, {}), vec4(1.0))", src1_, src2);
                    } else {
                        dot = fmt::format("dot({}, {})", src1, src2);
                    }
                }

                SetDest(swizzle, dest_reg, dot, 4, 1);
                break;
            }

            case OpCode::Id::RCP: {
                if (!sanitize_mul) {
                    // When accurate multiplication is OFF, NaN are not really handled. This is a
                    // workaround to cheaply avoid NaN. Fixes graphical issues in Ocarina of Time.
                    shader.AddLine("if ({}.x != 0.0)", src1);
                }
                SetDest(swizzle, dest_reg, fmt::format("(1.0 / {}.x)", src1), 4, 1);
                break;
            }

            case OpCode::Id::RSQ: {
                if (!sanitize_mul) {
                    // When accurate multiplication is OFF, NaN are not really handled. This is a
                    // workaround to cheaply avoid NaN. Fixes graphical issues in Ocarina of Time.
                    shader.AddLine("if ({}.x > 0.0)", src1);
                }
                SetDest(swizzle, dest_reg, fmt::format("inversesqrt({}.x)", src1), 4, 1);
                break;
            }

            case OpCode::Id::MOVA: {
                SetDest(swizzle, "address_registers", fmt::format("ivec2({})", src1), 2, 2);
                break;
            }

            case OpCode::Id::MOV: {
                SetDest(swizzle, dest_reg, src1, 4, 4);
                break;
            }

            case OpCode::Id::SGE:
            case OpCode::Id::SGEI: {
                SetDest(swizzle, dest_reg,
                        fmt::format("vec4(greaterThanEqual({}, {}))", src1, src2), 4, 4);
                break;
            }

            case OpCode::Id::SLT:
            case OpCode::Id::SLTI: {
                SetDest(swizzle, dest_reg, fmt::format("vec4(lessThan({}, {}))", src1, src2), 4, 4);
                break;
            }

            case OpCode::Id::CMP: {
                using CompareOp = Instruction::Common::CompareOpType::Op;
                const std::map<CompareOp, std::pair<std::string_view, std::string_view>> cmp_ops{
                    {CompareOp::Equal, {"==", "equal"}},
                    {CompareOp::NotEqual, {"!=", "notEqual"}},
                    {CompareOp::LessThan, {"<", "lessThan"}},
                    {CompareOp::LessEqual, {"<=", "lessThanEqual"}},
                    {CompareOp::GreaterThan, {">", "greaterThan"}},
                    {CompareOp::GreaterEqual, {">=", "greaterThanEqual"}},
                };

                const CompareOp op_x = instr.common.compare_op.x.Value();
                const CompareOp op_y = instr.common.compare_op.y.Value();

                if (cmp_ops.find(op_x) == cmp_ops.end()) {
                    LOG_ERROR(HW_GPU, "Unknown compare mode {:x}", op_x);
                } else if (cmp_ops.find(op_y) == cmp_ops.end()) {
                    LOG_ERROR(HW_GPU, "Unknown compare mode {:x}", op_y);
                } else if (op_x != op_y) {
                    shader.AddLine("conditional_code.x = {}.x {} {}.x;", src1,
                                   cmp_ops.find(op_x)->second.first, src2);
                    shader.AddLine("conditional_code.y = {}.y {} {}.y;", src1,
                                   cmp_ops.find(op_y)->second.first, src2);
                } else {
                    shader.AddLine("conditional_code = {}(vec2({}), vec2({}));",
                                   cmp_ops.find(op_x)->second.second, src1, src2);
                }
                break;
            }

            case OpCode::Id::EX2: {
                SetDest(swizzle, dest_reg, fmt::format("exp2({}.x)", src1), 4, 1);
                break;
            }

            case OpCode::Id::LG2: {
                SetDest(swizzle, dest_reg, fmt::format("log2({}.x)", src1), 4, 1);
                break;
            }

            default: {
                LOG_ERROR(HW_GPU, "Unhandled arithmetic instruction: 0x{:02x} ({}): 0x{:08x}",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
                throw DecompileFail("Unhandled instruction");
                break;
            }
            }

            break;
        }

        case OpCode::Type::MultiplyAdd: {
            if ((instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MAD) ||
                (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI)) {
                bool is_inverted = (instr.opcode.Value().EffectiveOpCode() == OpCode::Id::MADI);

                std::string src1 = swizzle.negate_src1 ? "-" : "";
                src1 += GetSourceRegister(instr.mad.GetSrc1(is_inverted), 0);
                src1 += "." + GetSelectorSrc1(swizzle);

                std::string src2 = swizzle.negate_src2 ? "-" : "";
                src2 += GetSourceRegister(instr.mad.GetSrc2(is_inverted),
                                          !is_inverted * instr.mad.address_register_index);
                src2 += "." + GetSelectorSrc2(swizzle);

                std::string src3 = swizzle.negate_src3 ? "-" : "";
                src3 += GetSourceRegister(instr.mad.GetSrc3(is_inverted),
                                          is_inverted * instr.mad.address_register_index);
                src3 += "." + GetSelectorSrc3(swizzle);

                std::string dest_reg =
                    (instr.mad.dest.Value() < 0x10)
                        ? outputreg_getter(static_cast<u32>(instr.mad.dest.Value().GetIndex()))
                    : (instr.mad.dest.Value() < 0x20)
                        ? "reg_tmp" + std::to_string(instr.mad.dest.Value().GetIndex())
                        : "";

                if (sanitize_mul) {
                    SetDest(swizzle, dest_reg,
                            fmt::format("sanitize_mul({}, {}) + {}", src1, src2, src3), 4, 4);
                } else {
                    SetDest(swizzle, dest_reg, fmt::format("{} * {} + {}", src1, src2, src3), 4, 4);
                }
            } else {
                LOG_ERROR(HW_GPU, "Unhandled multiply-add instruction: 0x{:02x} ({}): 0x{:08x}",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
                throw DecompileFail("Unhandled instruction");
            }
            break;
        }

        default: {
            switch (instr.opcode.Value()) {
            case OpCode::Id::END: {
                shader.AddLine("return true;");
                offset = PROGRAM_END - 1;
                break;
            }

            case OpCode::Id::JMPC:
            case OpCode::Id::JMPU: {
                std::string condition;
                if (instr.opcode.Value() == OpCode::Id::JMPC) {
                    condition = EvaluateCondition(instr.flow_control);
                } else {
                    bool invert_test = instr.flow_control.num_instructions & 1;
                    condition = (invert_test ? "!" : "") +
                                GetUniformBool(instr.flow_control.bool_uniform_id);
                }

                shader.AddLine("if ({}) {{", condition);
                ++shader.scope;
                shader.AddLine("{{ jmp_to = {}u; break; }}",
                               instr.flow_control.dest_offset.Value());

                --shader.scope;
                shader.AddLine("}}");
                break;
            }

            case OpCode::Id::CALL:
            case OpCode::Id::CALLC:
            case OpCode::Id::CALLU: {
                std::string condition;
                if (instr.opcode.Value() == OpCode::Id::CALLC) {
                    condition = EvaluateCondition(instr.flow_control);
                } else if (instr.opcode.Value() == OpCode::Id::CALLU) {
                    condition = GetUniformBool(instr.flow_control.bool_uniform_id);
                }

                if (condition.empty()) {
                    shader.AddLine("{{");
                } else {
                    shader.AddLine("if ({}) {{", condition);
                }
                ++shader.scope;

                auto& call_sub = GetSubroutine(instr.flow_control.dest_offset,
                                               instr.flow_control.dest_offset +
                                                   instr.flow_control.num_instructions);

                CallSubroutine(call_sub);
                if (instr.opcode.Value() == OpCode::Id::CALL &&
                    call_sub.exit_method == ExitMethod::AlwaysEnd) {
                    offset = PROGRAM_END - 1;
                }

                --shader.scope;
                shader.AddLine("}}");
                break;
            }

            case OpCode::Id::NOP: {
                break;
            }

            case OpCode::Id::IFC:
            case OpCode::Id::IFU: {
                std::string condition;
                if (instr.opcode.Value() == OpCode::Id::IFC) {
                    condition = EvaluateCondition(instr.flow_control);
                } else {
                    condition = GetUniformBool(instr.flow_control.bool_uniform_id);
                }

                const u32 if_offset = offset + 1;
                const u32 else_offset = instr.flow_control.dest_offset;
                const u32 endif_offset =
                    instr.flow_control.dest_offset + instr.flow_control.num_instructions;

                shader.AddLine("if ({}) {{", condition);
                ++shader.scope;

                auto& if_sub = GetSubroutine(if_offset, else_offset);
                CallSubroutine(if_sub);
                offset = else_offset - 1;

                if (instr.flow_control.num_instructions != 0) {
                    --shader.scope;
                    shader.AddLine("}} else {{");
                    ++shader.scope;

                    auto& else_sub = GetSubroutine(else_offset, endif_offset);
                    CallSubroutine(else_sub);
                    offset = endif_offset - 1;

                    if (if_sub.exit_method == ExitMethod::AlwaysEnd &&
                        else_sub.exit_method == ExitMethod::AlwaysEnd) {
                        offset = PROGRAM_END - 1;
                    }
                }

                --shader.scope;
                shader.AddLine("}}");
                break;
            }

            case OpCode::Id::LOOP: {
                const std::string int_uniform =
                    fmt::format("uniforms.i[{}]", instr.flow_control.int_uniform_id.Value());

                shader.AddLine("address_registers.z = int({}.y);", int_uniform);

                const std::string loop_var = fmt::format("loop{}", offset);
                shader.AddLine(
                    "for (uint {} = 0u; {} <= {}.x; address_registers.z += int({}.z), ++{}) {{",
                    loop_var, loop_var, int_uniform, int_uniform, loop_var);
                ++shader.scope;

                auto& loop_sub = GetSubroutine(offset + 1, instr.flow_control.dest_offset + 1);
                CallSubroutine(loop_sub);
                offset = instr.flow_control.dest_offset;

                --shader.scope;
                shader.AddLine("}}");

                if (loop_sub.exit_method == ExitMethod::AlwaysEnd) {
                    offset = PROGRAM_END - 1;
                }

                break;
            }

            case OpCode::Id::EMIT:
            case OpCode::Id::SETEMIT:
                LOG_ERROR(HW_GPU, "Geometry shader operation detected in vertex shader");
                break;

            default: {
                LOG_ERROR(HW_GPU, "Unhandled instruction: 0x{:02x} ({}): 0x{:08x}",
                          (int)instr.opcode.Value().EffectiveOpCode(),
                          instr.opcode.Value().GetInfo().name, instr.hex);
                throw DecompileFail("Unhandled instruction");
                break;
            }
            }

            break;
        }
        }
        return offset + 1;
    }

    /**
     * Compiles a range of instructions from PICA to GLSL.
     * @param begin the offset of the starting instruction.
     * @param end the offset where the compilation should stop (exclusive).
     * @return the offset of the next instruction to compile. PROGRAM_END if the program terminates.
     */
    u32 CompileRange(u32 begin, u32 end) {
        u32 program_counter;
        for (program_counter = begin; program_counter < (begin > end ? PROGRAM_END : end);) {
            program_counter = CompileInstr(program_counter);
        }
        return program_counter;
    }

    void Generate() {
        if (sanitize_mul) {
            shader.AddLine("vec4 sanitize_mul(vec4 lhs, vec4 rhs) {{");
            ++shader.scope;
            shader.AddLine("vec4 product = lhs * rhs;");
            shader.AddLine("return mix(product, mix(mix(vec4(0.0), product, isnan(rhs)), product, "
                           "isnan(lhs)), isnan(product));");
            --shader.scope;
            shader.AddLine("}}\n");
        }

        shader.AddLine("vec4 get_offset_register(int base_index, int offset) {{");
        ++shader.scope;
        shader.AddLine("int fixed_offset = offset >= -128 && offset <= 127 ? offset : 0;");
        shader.AddLine("uint index = uint((base_index + fixed_offset) & 0x7F);");
        shader.AddLine("return index < 96u ? uniforms.f[index] : vec4(1.0);");
        --shader.scope;
        shader.AddLine("}}\n");

        // Add declarations for registers
        shader.AddLine("bvec2 conditional_code = bvec2(false);");
        shader.AddLine("ivec3 address_registers = ivec3(0);");
        for (int i = 0; i < 16; ++i) {
            shader.AddLine("vec4 reg_tmp{} = vec4(0.0, 0.0, 0.0, 1.0);", i);
        }
        shader.AddNewLine();

        // Add declarations for all subroutines
        for (const auto& subroutine : subroutines) {
            shader.AddLine("bool {}();", subroutine.GetName());
        }
        shader.AddNewLine();

        // Add the main entry point
        shader.AddLine("bool exec_shader() {{");
        ++shader.scope;
        CallSubroutine(GetSubroutine(main_offset, PROGRAM_END));
        --shader.scope;
        shader.AddLine("}}\n");

        // Add definitions for all subroutines
        for (const auto& subroutine : subroutines) {
            std::set<u32> labels = subroutine.labels;

            shader.AddLine("bool {}() {{", subroutine.GetName());
            ++shader.scope;

            if (labels.empty()) {
                if (CompileRange(subroutine.begin, subroutine.end) != PROGRAM_END) {
                    shader.AddLine("return false;");
                }
            } else {
                labels.insert(subroutine.begin);
                shader.AddLine("uint jmp_to = {}u;", subroutine.begin);
                shader.AddLine("while (true) {{");
                ++shader.scope;

                shader.AddLine("switch (jmp_to) {{");

                for (auto label : labels) {
                    shader.AddLine("case {}u: {{", label);
                    ++shader.scope;

                    auto next_it = labels.lower_bound(label + 1);
                    u32 next_label = next_it == labels.end() ? subroutine.end : *next_it;

                    u32 compile_end = CompileRange(label, next_label);
                    if (compile_end > next_label && compile_end != PROGRAM_END) {
                        // This happens only when there is a label inside a IF/LOOP block
                        shader.AddLine("{{ jmp_to = {}u; break; }}", compile_end);
                        labels.emplace(compile_end);
                    }

                    --shader.scope;
                    shader.AddLine("}}");
                }

                shader.AddLine("default: return false;");
                shader.AddLine("}}");

                --shader.scope;
                shader.AddLine("}}");

                shader.AddLine("return false;");
            }

            --shader.scope;
            shader.AddLine("}}\n");

            DEBUG_ASSERT(shader.scope == 0);
        }
    }

private:
    const std::set<Subroutine>& subroutines;
    const ProgramCode& program_code;
    const SwizzleData& swizzle_data;
    const u32 main_offset;
    const RegGetter& inputreg_getter;
    const RegGetter& outputreg_getter;
    const bool sanitize_mul;

    ShaderWriter shader;
};

std::string DecompileProgram(const ProgramCode& program_code, const SwizzleData& swizzle_data,
                             u32 main_offset, const RegGetter& inputreg_getter,
                             const RegGetter& outputreg_getter, bool sanitize_mul) {

    try {
        auto subroutines = ControlFlowAnalyzer(program_code, main_offset).MoveSubroutines();
        GLSLGenerator generator(subroutines, program_code, swizzle_data, main_offset,
                                inputreg_getter, outputreg_getter, sanitize_mul);
        return generator.MoveShaderCode();
    } catch (const DecompileFail& exception) {
        LOG_INFO(HW_GPU, "Shader decompilation failed: {}", exception.what());
        return "";
    }
}

} // namespace Pica::Shader::Generator::GLSL
