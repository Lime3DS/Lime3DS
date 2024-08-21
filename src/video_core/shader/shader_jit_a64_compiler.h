// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if LIME3DS_ARCH(arm64)

#include <array>
#include <bitset>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>
#include <nihstro/shader_bytecode.h>
#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>
#include "common/common_types.h"
#include "video_core/pica/shader_setup.h"

using nihstro::Instruction;
using nihstro::OpCode;
using nihstro::SourceRegister;
using nihstro::SwizzlePattern;

namespace Pica {
struct ShaderUnit;
}

namespace Pica::Shader {

/// Memory allocated for each compiled shader
constexpr std::size_t MAX_SHADER_SIZE = MAX_PROGRAM_CODE_LENGTH * 256;

/**
 * This class implements the shader JIT compiler. It recompiles a Pica shader program into x86_64
 * code that can be executed on the host machine directly.
 */
class JitShader : private oaknut::CodeBlock, private oaknut::CodeGenerator {
public:
    JitShader();

    void Run(const ShaderSetup& setup, ShaderUnit& state, u32 offset) const {
        program(&setup.uniforms, &state,
                reinterpret_cast<std::byte*>(oaknut::CodeBlock::ptr()) +
                    instruction_labels[offset].offset());
    }

    void Compile(const std::array<u32, MAX_PROGRAM_CODE_LENGTH>* program_code,
                 const std::array<u32, MAX_SWIZZLE_DATA_LENGTH>* swizzle_data);

    void Compile_ADD(Instruction instr);
    void Compile_DP3(Instruction instr);
    void Compile_DP4(Instruction instr);
    void Compile_DPH(Instruction instr);
    void Compile_EX2(Instruction instr);
    void Compile_LG2(Instruction instr);
    void Compile_MUL(Instruction instr);
    void Compile_SGE(Instruction instr);
    void Compile_SLT(Instruction instr);
    void Compile_FLR(Instruction instr);
    void Compile_MAX(Instruction instr);
    void Compile_MIN(Instruction instr);
    void Compile_RCP(Instruction instr);
    void Compile_RSQ(Instruction instr);
    void Compile_MOVA(Instruction instr);
    void Compile_MOV(Instruction instr);
    void Compile_NOP(Instruction instr);
    void Compile_END(Instruction instr);
    void Compile_BREAKC(Instruction instr);
    void Compile_CALL(Instruction instr);
    void Compile_CALLC(Instruction instr);
    void Compile_CALLU(Instruction instr);
    void Compile_IF(Instruction instr);
    void Compile_LOOP(Instruction instr);
    void Compile_JMP(Instruction instr);
    void Compile_CMP(Instruction instr);
    void Compile_MAD(Instruction instr);
    void Compile_EMIT(Instruction instr);
    void Compile_SETE(Instruction instr);

private:
    void Compile_Block(u32 end);
    void Compile_NextInstr();

    void Compile_SwizzleSrc(Instruction instr, u32 src_num, SourceRegister src_reg,
                            oaknut::QReg dest);
    void Compile_DestEnable(Instruction instr, oaknut::QReg dest);

    /**
     * Compiles a `MUL src1, src2` operation, properly handling the PICA semantics when multiplying
     * zero by inf. Clobbers `src2` and `scratch`.
     */
    void Compile_SanitizedMul(oaknut::QReg src1, oaknut::QReg src2, oaknut::QReg scratch0);

    void Compile_EvaluateCondition(Instruction instr);
    void Compile_UniformCondition(Instruction instr);

    /**
     * Emits the code to conditionally return from a subroutine envoked by the `CALL` instruction.
     */
    void Compile_Return();

    std::bitset<64> PersistentCallerSavedRegs();

    /**
     * Assertion evaluated at compile-time, but only triggered if executed at runtime.
     * @param condition Condition to be evaluated.
     * @param msg       Message to be logged if the assertion fails.
     */
    void Compile_Assert(bool condition, const char* msg);

    /**
     * Analyzes the entire shader program for `CALL` instructions before emitting any code,
     * identifying the locations where a return needs to be inserted.
     */
    void FindReturnOffsets();

    /**
     * Emits data and code for utility functions.
     */
    void CompilePrelude();
    oaknut::Label CompilePrelude_Log2();
    oaknut::Label CompilePrelude_Exp2();

    const std::array<u32, MAX_PROGRAM_CODE_LENGTH>* program_code = nullptr;
    const std::array<u32, MAX_SWIZZLE_DATA_LENGTH>* swizzle_data = nullptr;

    /// Mapping of Pica VS instructions to pointers in the emitted code
    std::array<oaknut::Label, MAX_PROGRAM_CODE_LENGTH> instruction_labels;

    /// Labels pointing to the end of each nested LOOP block. Used by the BREAKC instruction to
    /// break out of a loop.
    std::vector<oaknut::Label> loop_break_labels;

    /// Offsets in code where a return needs to be inserted
    std::vector<u32> return_offsets;

    u32 program_counter = 0; ///< Offset of the next instruction to decode
    u8 loop_depth = 0;       ///< Depth of the (nested) loops currently compiled

    using CompiledShader = void(const void* setup, void* state, const std::byte* start_addr);
    CompiledShader* program = nullptr;

    oaknut::Label log2_subroutine;
    oaknut::Label exp2_subroutine;
};

} // namespace Pica::Shader

#endif
