// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// FastInterp decode: raw ARM/Thumb16/Thumb32 instructions -> DecodedInst.

#include <bit>
#include <cstring>
#include "core/arm/dynarmic/arm_tick_counts.h"
#include "core/arm/fastinterp/fastinterp.h"
#include "core/memory.h"

namespace Core::FastInterp {

// Temporarily disable all fusion for debugging - set to 1 to disable
#define FASTINTERP_DISABLE_FUSION 0

static bool IsFusedOrUnrolled(Opcode op) {
    switch (op) {
    case Opcode::FusedSubsBcc:
    case Opcode::FusedSubsRegBcc:
    case Opcode::FusedCmpBcc:
    case Opcode::FusedCmpRegBcc:
    case Opcode::FusedTstBcc:
    case Opcode::FusedTstRegBcc:
    case Opcode::FusedTeqBcc:
    case Opcode::FusedTeqRegBcc:
    case Opcode::FusedAddCmpBcc:
    case Opcode::ThumbFusedSubsBcc:
    case Opcode::ThumbFusedCmpBcc:
    case Opcode::ThumbFusedCmpRegBcc:
    case Opcode::ThumbFusedTstBcc:
    case Opcode::ArmUnrolledSubsLoop:
    case Opcode::ThumbUnrolledSubsLoop:
    case Opcode::ThumbUnrolledCmpLoop:
        return true;
    default:
        return false;
    }
}

static u8 ComputeTicks(const DecodedInst& inst, bool is_thumb) {
    // Fused/unrolled Thumb opcodes stand in for multiple Thumb instructions, so they must be
    // handled before the generic "Thumb = 1 tick" early-return below (otherwise they are
    // silently undercounted).
    switch (inst.opcode) {
    case Opcode::ThumbFusedSubsBcc:
    case Opcode::ThumbFusedCmpBcc:
    case Opcode::ThumbFusedCmpRegBcc:
    case Opcode::ThumbFusedTstBcc:
    case Opcode::ThumbUnrolledSubsLoop:
    case Opcode::ThumbUnrolledCmpLoop:
        return 2;
    default:
        break;
    }
    if (is_thumb)
        return 1;

    switch (inst.opcode) {
    // Branches
    case Opcode::B:
    case Opcode::Bl:
        return 4;
    case Opcode::Bx:
        return 5;
    case Opcode::Blx: // register form (immediate form is BlxImm)
        return 6;
    case Opcode::BlxImm:
        return 5;

    // Exceptions
    case Opcode::Swi:
        return 8;

    // Data processing (all variants: imm, reg, rsr)
    case Opcode::And:
    case Opcode::Eor:
    case Opcode::Sub:
    case Opcode::Rsb:
    case Opcode::Add:
    case Opcode::Adc:
    case Opcode::Sbc:
    case Opcode::Rsc:
    case Opcode::Orr:
    case Opcode::Mov:
    case Opcode::Bic:
    case Opcode::Mvn:
    case Opcode::AndS:
    case Opcode::EorS:
    case Opcode::SubS:
    case Opcode::RsbS:
    case Opcode::AddS:
    case Opcode::AdcS:
    case Opcode::SbcS:
    case Opcode::RscS:
    case Opcode::OrrS:
    case Opcode::MovS:
    case Opcode::BicS:
    case Opcode::MvnS:
    case Opcode::Tst:
    case Opcode::Teq:
    case Opcode::Cmp:
    case Opcode::Cmn:
        if (inst.dp.rm != 0xFF && inst.dp.rs != 0xFF) {
            return (inst.rd == 15) ? 8 : 2; // Register-shifted register
        }
        return (inst.rd == 15) ? 7 : 1;

    // Single load/store
    case Opcode::Ldr:
    case Opcode::LdrB:
    case Opcode::LdrH:
    case Opcode::LdrSB:
    case Opcode::LdrSH:
    case Opcode::Str:
    case Opcode::StrB:
    case Opcode::StrH:
    case Opcode::Ldrd:
    case Opcode::Strd:
        return 2;

    // Load/store multiple
    case Opcode::Ldm:
    case Opcode::Stm:
        return 1 + std::popcount(static_cast<unsigned>(inst.lsm.reg_list)) / 2;

    // Multiply (short)
    case Opcode::Mul:
    case Opcode::Mla:
        return 2;
    case Opcode::MulS:
    case Opcode::MlaS:
        return 5;

    // Multiply (long)
    case Opcode::Umull:
    case Opcode::Smull:
    case Opcode::Umlal:
    case Opcode::Smlal:
        return 3;
    case Opcode::UmullS:
    case Opcode::SmullS:
    case Opcode::UmlalS:
    case Opcode::SmlalS:
        return 6;

    // Status register access (MSR with control bits costs 4, flags-only costs 1)
    case Opcode::Msr:
        return (inst.dp.flags == 0x08) ? 1 : 4; // 0x08 = flags-only mask

    // Coprocessor register transfer
    case Opcode::Mrc:
    case Opcode::Mcr:
        return 2;

    // Exclusive load/store
    case Opcode::Ldrex:
    case Opcode::Strex:
    case Opcode::Ldrexb:
    case Opcode::Strexb:
    case Opcode::Ldrexh:
    case Opcode::Strexh:
    case Opcode::Ldrexd:
    case Opcode::Strexd:
        return 2;

    // VFP register transfer
    case Opcode::VmovArmToS:
    case Opcode::VmovSToArm:
    case Opcode::VmovArmToD:
    case Opcode::VmovDToArm:
    case Opcode::Vmrs:
    case Opcode::Vmsr:
        return 2;

    // Fused ARM: ALU(1) + B(4) = 5
    case Opcode::FusedSubsBcc:
    case Opcode::FusedSubsRegBcc:
    case Opcode::FusedCmpBcc:
    case Opcode::FusedCmpRegBcc:
    case Opcode::FusedTstBcc:
    case Opcode::FusedTstRegBcc:
    case Opcode::FusedTeqBcc:
    case Opcode::FusedTeqRegBcc:
        return 5;

    // Fused ARM: ADD(1) + CMP(1) + B(4) = 6
    case Opcode::FusedAddCmpBcc:
        return 6;

    // Fused Thumb: ALU(1) + B(1) = 2
    case Opcode::ThumbFusedSubsBcc:
    case Opcode::ThumbFusedCmpBcc:
    case Opcode::ThumbFusedCmpRegBcc:
    case Opcode::ThumbFusedTstBcc:
        return 2;

    // Unrolled loops: per-iteration tick cost
    case Opcode::ArmUnrolledSubsLoop:
        return 5; // ARM SUBS(1) + BNE(4)
    case Opcode::ThumbUnrolledSubsLoop:
    case Opcode::ThumbUnrolledCmpLoop:
        return 2; // Thumb: SUBS(1) + BNE(1)

    default:
        return 1;
    }
}

// ============================================================================
// Dead-Flag Elimination
// ============================================================================
// Backward liveness pass over a decoded block: a flag-setting instruction whose
// flags are all provably overwritten before any read is swapped for its
// non-flag-setting equivalent, skipping flag computation at execution time.
// Thumb16 sets flags on nearly every ALU instruction, so this is where most of
// the wins are; dead Thumb ALU ops are retargeted to the ARM handlers (same
// dp-union layout, with field fixups).
//
// Conservative rules: flags are live at block end (successor unknown), any
// unclassified opcode is a barrier (all flags live), and conditional
// instructions never count as kills (they may not execute).

namespace {

constexpr u8 FLAG_N = 1;
constexpr u8 FLAG_Z = 2;
constexpr u8 FLAG_C = 4;
constexpr u8 FLAG_V = 8;
constexpr u8 FLAG_NZ = FLAG_N | FLAG_Z;
constexpr u8 FLAG_NZC = FLAG_N | FLAG_Z | FLAG_C;
constexpr u8 FLAG_NZCV = FLAG_N | FLAG_Z | FLAG_C | FLAG_V;

struct FlagEffects {
    u8 writes;    // Flags this instruction sets
    u8 reads;     // Flags this instruction consumes (beyond its condition code)
    bool barrier; // Unknown flag behavior: treat all flags as live
};

u8 CondReadMask(u8 cond) {
    switch (cond) {
    case 0x0:
    case 0x1:
        return FLAG_Z; // EQ/NE
    case 0x2:
    case 0x3:
        return FLAG_C; // CS/CC
    case 0x4:
    case 0x5:
        return FLAG_N; // MI/PL
    case 0x6:
    case 0x7:
        return FLAG_V; // VS/VC
    case 0x8:
    case 0x9:
        return FLAG_C | FLAG_Z; // HI/LS
    case 0xA:
    case 0xB:
        return FLAG_N | FLAG_V; // GE/LT
    case 0xC:
    case 0xD:
        return FLAG_N | FLAG_Z | FLAG_V; // GT/LE
    default:
        return 0; // AL/NV
    }
}

// RRX operands (ROR #0 immediate shift) consume the current C flag
bool OperandIsRrx(const DecodedInst& inst) {
    return inst.dp.rm != 0xFF && inst.dp.rs == 0xFF && inst.dp.shift_imm == 0 &&
           inst.dp.shift_type == ShiftType::ROR;
}

// True when a carry-aware operand may pass the current C through as shifter
// carry (immediate without rotation, LSL #0, register-specified amount 0, RRX)
bool OperandMayPassCarry(const DecodedInst& inst) {
    if (inst.dp.rm == 0xFF) {
        return (inst.dp.flags & 1) == 0;
    }
    return true;
}

FlagEffects ClassifyFlagEffects(const DecodedInst& inst) {
    switch (inst.opcode) {
    // ARM data processing, no flag writes
    case Opcode::And:
    case Opcode::Eor:
    case Opcode::Sub:
    case Opcode::Rsb:
    case Opcode::Add:
    case Opcode::Orr:
    case Opcode::Mov:
    case Opcode::Bic:
    case Opcode::Mvn:
        return {0, OperandIsRrx(inst) ? FLAG_C : u8{0}, false};
    case Opcode::Adc:
    case Opcode::Sbc:
    case Opcode::Rsc:
        return {0, FLAG_C, false};

    // ARM flag-setting arithmetic
    case Opcode::AddS:
    case Opcode::SubS:
    case Opcode::RsbS:
    case Opcode::Cmp:
    case Opcode::Cmn:
        return {FLAG_NZCV, OperandIsRrx(inst) ? FLAG_C : u8{0}, false};
    case Opcode::AdcS:
    case Opcode::SbcS:
    case Opcode::RscS:
        return {FLAG_NZCV, FLAG_C, false};

    // ARM flag-setting logical (V preserved, shifter carry may pass old C)
    case Opcode::AndS:
    case Opcode::EorS:
    case Opcode::OrrS:
    case Opcode::BicS:
    case Opcode::MovS:
    case Opcode::MvnS:
    case Opcode::Tst:
    case Opcode::Teq:
        return {FLAG_NZC, OperandMayPassCarry(inst) ? FLAG_C : u8{0}, false};

    // Multiplies (S variants set N,Z only; C,V preserved)
    case Opcode::Mul:
    case Opcode::Mla:
    case Opcode::Umull:
    case Opcode::Umlal:
    case Opcode::Smull:
    case Opcode::Smlal:
        return {0, 0, false};
    case Opcode::MulS:
    case Opcode::MlaS:
    case Opcode::UmullS:
    case Opcode::UmlalS:
    case Opcode::SmullS:
    case Opcode::SmlalS:
        return {FLAG_NZ, 0, false};

    // Loads/stores, branches, and other NZCV-neutral ARM instructions
    case Opcode::EndBlock:
    case Opcode::Nop:
    case Opcode::Ldr:
    case Opcode::LdrB:
    case Opcode::LdrH:
    case Opcode::LdrSB:
    case Opcode::LdrSH:
    case Opcode::Str:
    case Opcode::StrB:
    case Opcode::StrH:
    case Opcode::LdrImm:
    case Opcode::StrImm:
    case Opcode::LdrBImm:
    case Opcode::StrBImm:
    case Opcode::LdrHImm:
    case Opcode::StrHImm:
    case Opcode::LdrLit:
    case Opcode::PcSetup:
    case Opcode::B:
    case Opcode::Bl:
    case Opcode::Bx:
    case Opcode::Blx:
    case Opcode::BlxImm:
    case Opcode::Clz:
    case Opcode::Sxtb:
    case Opcode::Sxth:
    case Opcode::Uxtb:
    case Opcode::Uxth:
    case Opcode::Rev:
    case Opcode::Rev16:
    case Opcode::Revsh:
    case Opcode::Ldrex:
    case Opcode::Strex:
    case Opcode::Ldrexb:
    case Opcode::Strexb:
    case Opcode::Ldrexh:
    case Opcode::Strexh:
    case Opcode::Ldrexd:
    case Opcode::Strexd:
    case Opcode::Clrex:
    case Opcode::Ldrd:
    case Opcode::Strd:
        return {0, 0, false};

    // LDM/STM: neutral unless the S bit (user regs / SPSR restore) is set
    case Opcode::Ldm:
    case Opcode::Stm:
        return {0, 0, (inst.lsm.flags & 0x02) != 0};

    // Thumb flag-setting arithmetic (full NZCV)
    case Opcode::ThumbAddReg3:
    case Opcode::ThumbSubReg3:
    case Opcode::ThumbAddImm3:
    case Opcode::ThumbSubImm3:
    case Opcode::ThumbAddImm8:
    case Opcode::ThumbSubImm8:
    case Opcode::ThumbNeg:
    case Opcode::ThumbCmpImm8:
    case Opcode::ThumbCmpReg:
    case Opcode::ThumbCmn:
    case Opcode::ThumbCmpHi:
        return {FLAG_NZCV, 0, false};
    case Opcode::ThumbAdc:
    case Opcode::ThumbSbc:
        return {FLAG_NZCV, FLAG_C, false};

    // Thumb shifts (set N,Z,C; LSL #0 and register amounts of 0 pass old C)
    case Opcode::ThumbLslImm:
        return {FLAG_NZC, inst.dp.shift_imm == 0 ? FLAG_C : u8{0}, false};
    case Opcode::ThumbLsrImm:
    case Opcode::ThumbAsrImm:
        return {FLAG_NZC, 0, false};
    case Opcode::ThumbLslReg:
    case Opcode::ThumbLsrReg:
    case Opcode::ThumbAsrReg:
    case Opcode::ThumbRor:
        return {FLAG_NZC, FLAG_C, false};

    // Thumb logical / moves / multiply (set N,Z only; C,V preserved)
    case Opcode::ThumbMovImm8:
    case Opcode::ThumbAnd:
    case Opcode::ThumbEor:
    case Opcode::ThumbTst:
    case Opcode::ThumbOrr:
    case Opcode::ThumbBic:
    case Opcode::ThumbMvn:
    case Opcode::ThumbMul:
        return {FLAG_NZ, 0, false};

    // NZCV-neutral Thumb instructions
    case Opcode::ThumbAddHi:
    case Opcode::ThumbMovHi:
    case Opcode::ThumbBx:
    case Opcode::ThumbBlxReg:
    case Opcode::ThumbLdrReg:
    case Opcode::ThumbStrReg:
    case Opcode::ThumbLdrImm5:
    case Opcode::ThumbStrImm5:
    case Opcode::ThumbLdrbReg:
    case Opcode::ThumbStrbReg:
    case Opcode::ThumbLdrbImm5:
    case Opcode::ThumbStrbImm5:
    case Opcode::ThumbLdrhReg:
    case Opcode::ThumbStrhReg:
    case Opcode::ThumbLdrhImm5:
    case Opcode::ThumbStrhImm5:
    case Opcode::ThumbLdrsb:
    case Opcode::ThumbLdrsh:
    case Opcode::ThumbLdrSp:
    case Opcode::ThumbStrSp:
    case Opcode::ThumbLdrPc:
    case Opcode::ThumbAddPcImm:
    case Opcode::ThumbAddSpImm:
    case Opcode::ThumbAddSpImm7:
    case Opcode::ThumbSubSpImm7:
    case Opcode::ThumbPush:
    case Opcode::ThumbPop:
    case Opcode::ThumbLdmia:
    case Opcode::ThumbStmia:
    case Opcode::ThumbBCond:
    case Opcode::ThumbB:
    case Opcode::ThumbSxth:
    case Opcode::ThumbSxtb:
    case Opcode::ThumbUxth:
    case Opcode::ThumbUxtb:
    case Opcode::ThumbRev:
    case Opcode::ThumbRev16:
    case Opcode::ThumbRevsh:
    case Opcode::ThumbBl:
    case Opcode::ThumbBlxImm:
        return {0, 0, false};

    // Everything else (SVC, MRS/MSR, coprocessor, VFP, fused, unrolled, ...):
    // unknown flag behavior, treat as barrier
    default:
        return {0, 0, true};
    }
}

// Rewrite a flag-setting instruction to its non-flag-setting equivalent.
// Thumb ops are retargeted to ARM handlers; the fixups write every dp field
// the ARM handlers read (Thumb decode leaves unused fields stale).
bool TryDowngrade(DecodedInst& inst) {
    switch (inst.opcode) {
    // ARM S -> non-S: identical operand layout, opcode swap only
    case Opcode::AndS:
        inst.opcode = Opcode::And;
        return true;
    case Opcode::EorS:
        inst.opcode = Opcode::Eor;
        return true;
    case Opcode::SubS:
        inst.opcode = Opcode::Sub;
        return true;
    case Opcode::RsbS:
        inst.opcode = Opcode::Rsb;
        return true;
    case Opcode::AddS:
        inst.opcode = Opcode::Add;
        return true;
    case Opcode::AdcS:
        inst.opcode = Opcode::Adc;
        return true;
    case Opcode::SbcS:
        inst.opcode = Opcode::Sbc;
        return true;
    case Opcode::RscS:
        inst.opcode = Opcode::Rsc;
        return true;
    case Opcode::OrrS:
        inst.opcode = Opcode::Orr;
        return true;
    case Opcode::MovS:
        inst.opcode = Opcode::Mov;
        return true;
    case Opcode::BicS:
        inst.opcode = Opcode::Bic;
        return true;
    case Opcode::MvnS:
        inst.opcode = Opcode::Mvn;
        return true;
    case Opcode::MulS:
        inst.opcode = Opcode::Mul;
        return true;
    case Opcode::MlaS:
        inst.opcode = Opcode::Mla;
        return true;
    case Opcode::UmullS:
        inst.opcode = Opcode::Umull;
        return true;
    case Opcode::UmlalS:
        inst.opcode = Opcode::Umlal;
        return true;
    case Opcode::SmullS:
        inst.opcode = Opcode::Smull;
        return true;
    case Opcode::SmlalS:
        inst.opcode = Opcode::Smlal;
        return true;

    // Thumb immediate forms
    case Opcode::ThumbMovImm8: // Rd = imm8
        inst.opcode = Opcode::Mov;
        inst.dp.rm = 0xFF;
        return true;
    case Opcode::ThumbAddImm8: // Rd = Rd + imm8
        inst.opcode = Opcode::Add;
        inst.dp.rn = inst.rd;
        inst.dp.rm = 0xFF;
        return true;
    case Opcode::ThumbSubImm8: // Rd = Rd - imm8
        inst.opcode = Opcode::Sub;
        inst.dp.rn = inst.rd;
        inst.dp.rm = 0xFF;
        return true;
    case Opcode::ThumbAddImm3: // Rd = Rn + imm3
        inst.opcode = Opcode::Add;
        inst.dp.rm = 0xFF;
        return true;
    case Opcode::ThumbSubImm3: // Rd = Rn - imm3
        inst.opcode = Opcode::Sub;
        inst.dp.rm = 0xFF;
        return true;

    // Thumb three-register forms
    case Opcode::ThumbAddReg3: // Rd = Rn + Rm
        inst.opcode = Opcode::Add;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbSubReg3: // Rd = Rn - Rm
        inst.opcode = Opcode::Sub;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;

    // Thumb two-register forms (Rd is both source and destination)
    case Opcode::ThumbAnd:
        inst.opcode = Opcode::And;
        inst.dp.rn = inst.rd;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbEor:
        inst.opcode = Opcode::Eor;
        inst.dp.rn = inst.rd;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbOrr:
        inst.opcode = Opcode::Orr;
        inst.dp.rn = inst.rd;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbBic:
        inst.opcode = Opcode::Bic;
        inst.dp.rn = inst.rd;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbAdc:
        inst.opcode = Opcode::Adc;
        inst.dp.rn = inst.rd;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbSbc:
        inst.opcode = Opcode::Sbc;
        inst.dp.rn = inst.rd;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbMvn: // Rd = ~Rm
        inst.opcode = Opcode::Mvn;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        inst.dp.shift_imm = 0;
        return true;
    case Opcode::ThumbNeg: // Rd = 0 - Rm
        inst.opcode = Opcode::Rsb;
        inst.dp.rn = inst.dp.rm;
        inst.dp.rm = 0xFF;
        inst.dp.imm = 0;
        return true;

    // Thumb shifts -> MOV with shifted operand
    case Opcode::ThumbLslImm: // Rd = Rm LSL #imm5
        inst.opcode = Opcode::Mov;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSL;
        return true;
    case Opcode::ThumbLsrImm: // Rd = Rm LSR #imm5 (#0 = 32, same as ARM)
        inst.opcode = Opcode::Mov;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::LSR;
        return true;
    case Opcode::ThumbAsrImm: // Rd = Rm ASR #imm5 (#0 = 32, same as ARM)
        inst.opcode = Opcode::Mov;
        inst.dp.rs = 0xFF;
        inst.dp.shift_type = ShiftType::ASR;
        return true;
    case Opcode::ThumbLslReg: // Rd = Rd LSL Rs (Rs decoded into dp.rm)
        inst.opcode = Opcode::Mov;
        inst.dp.rs = inst.dp.rm;
        inst.dp.rm = inst.rd;
        inst.dp.shift_type = ShiftType::LSL;
        return true;
    case Opcode::ThumbLsrReg:
        inst.opcode = Opcode::Mov;
        inst.dp.rs = inst.dp.rm;
        inst.dp.rm = inst.rd;
        inst.dp.shift_type = ShiftType::LSR;
        return true;
    case Opcode::ThumbAsrReg:
        inst.opcode = Opcode::Mov;
        inst.dp.rs = inst.dp.rm;
        inst.dp.rm = inst.rd;
        inst.dp.shift_type = ShiftType::ASR;
        return true;
    case Opcode::ThumbRor:
        inst.opcode = Opcode::Mov;
        inst.dp.rs = inst.dp.rm;
        inst.dp.rm = inst.rd;
        inst.dp.shift_type = ShiftType::ROR;
        return true;

    case Opcode::ThumbMul: // Rd = Rm * Rd (dp.rm and mul.rm share the same byte)
        inst.opcode = Opcode::Mul;
        inst.mul.rs = inst.rd;
        return true;

    default:
        return false;
    }
}

u32 EliminateDeadFlags(BasicBlock* block) {
    u32 eliminated = 0;
    u8 live = FLAG_NZCV; // Successor block unknown: all flags live at exit
    for (int i = static_cast<int>(block->inst_count) - 1; i >= 0; --i) {
        DecodedInst& inst = block->instructions[i];
        FlagEffects fx = ClassifyFlagEffects(inst);
        if (!fx.barrier && fx.writes != 0 && (live & fx.writes) == 0 && TryDowngrade(inst)) {
            eliminated++;
            fx = ClassifyFlagEffects(inst);
        }
        if (fx.barrier) {
            live = FLAG_NZCV;
        } else {
            // Conditional instructions may not execute: no kill credit
            const u8 kills = (inst.cond == 0xE) ? fx.writes : u8{0};
            live = static_cast<u8>((live & ~kills) | fx.reads);
        }
        live |= CondReadMask(inst.cond);
    }
    return eliminated;
}

} // namespace

// ============================================================================
// PC-source elimination
// ============================================================================
// The dispatch loop does not keep regs[15] current, so decode must guarantee
// that no handler reads regs[15] as a source operand. ReadsPcAsSource() is the
// authoritative classifier (it mirrors exactly which union fields each handler
// indexes regs[] with); FoldPcSources() rewrites the common PC-relative forms
// into constants; anything left gets a PcSetup prefix inserted by
// DecodeBlockInto/Step, which materializes the pipeline-adjusted PC into
// regs[15] right before the instruction executes.

namespace {

// ARM data-processing opcodes (dp union, operand 2 via GetOp2/GetOp2Fast)
bool IsDpAluOpcode(Opcode op) {
    switch (op) {
    case Opcode::And:
    case Opcode::Eor:
    case Opcode::Sub:
    case Opcode::Rsb:
    case Opcode::Add:
    case Opcode::Adc:
    case Opcode::Sbc:
    case Opcode::Rsc:
    case Opcode::Tst:
    case Opcode::Teq:
    case Opcode::Cmp:
    case Opcode::Cmn:
    case Opcode::Orr:
    case Opcode::Mov:
    case Opcode::Bic:
    case Opcode::Mvn:
    case Opcode::AndS:
    case Opcode::EorS:
    case Opcode::SubS:
    case Opcode::RsbS:
    case Opcode::AddS:
    case Opcode::AdcS:
    case Opcode::SbcS:
    case Opcode::RscS:
    case Opcode::OrrS:
    case Opcode::MovS:
    case Opcode::BicS:
    case Opcode::MvnS:
        return true;
    default:
        return false;
    }
}

// dp opcodes whose handlers read regs[dp.rn] (MOV/MVN forms do not)
bool DpReadsRn(Opcode op) {
    switch (op) {
    case Opcode::Mov:
    case Opcode::Mvn:
    case Opcode::MovS:
    case Opcode::MvnS:
        return false;
    default:
        return true; // callers only ask for dp opcodes
    }
}

// dp opcodes whose handlers use the carry-aware GetOp2 (shifter carry feeds
// the C flag); the rest use GetOp2Fast, where op2's shifter carry is ignored
bool DpUsesShifterCarry(Opcode op) {
    switch (op) {
    case Opcode::AndS:
    case Opcode::EorS:
    case Opcode::OrrS:
    case Opcode::BicS:
    case Opcode::MovS:
    case Opcode::MvnS:
    case Opcode::Tst:
    case Opcode::Teq:
        return true;
    default:
        return false;
    }
}

// Decode-time barrel shifter for an immediate shift of a known value.
// Mirrors ARM_FastInterp::ShiftImm for LSL>0, LSR, ASR, ROR>0; the caller
// handles LSL #0 (pass-through) and ROR #0 (RRX, needs the runtime C flag).
u32 DecodeShiftImm(u32 value, ShiftType type, u8 amount, bool& carry_out) {
    switch (type) {
    case ShiftType::LSL:
        carry_out = (value >> (32 - amount)) & 1; // caller guarantees 1..31
        return value << amount;
    case ShiftType::LSR:
        if (amount == 0)
            amount = 32; // LSR #0 encodes LSR #32
        if (amount >= 32) {
            carry_out = (amount == 32) ? ((value >> 31) & 1) : false;
            return 0;
        }
        carry_out = (value >> (amount - 1)) & 1;
        return value >> amount;
    case ShiftType::ASR:
        if (amount == 0)
            amount = 32; // ASR #0 encodes ASR #32
        if (amount >= 32) {
            carry_out = (value >> 31) & 1;
            return static_cast<u32>(static_cast<s32>(value) >> 31);
        }
        carry_out = (value >> (amount - 1)) & 1;
        return static_cast<u32>(static_cast<s32>(value) >> amount);
    case ShiftType::ROR:
        amount &= 31; // caller guarantees != 0 before masking
        if (amount == 0) {
            carry_out = (value >> 31) & 1;
            return value;
        }
        carry_out = (value >> (amount - 1)) & 1;
        return (value >> amount) | (value << (32 - amount));
    default:
        carry_out = false;
        return value;
    }
}

} // namespace

bool ARM_FastInterp::ReadsPcAsSource(const DecodedInst& inst) {
    switch (inst.opcode) {
    // --- ARM data processing: rn (except MOV/MVN), op2 register, shift reg ---
    case Opcode::And:
    case Opcode::Eor:
    case Opcode::Sub:
    case Opcode::Rsb:
    case Opcode::Add:
    case Opcode::Adc:
    case Opcode::Sbc:
    case Opcode::Rsc:
    case Opcode::Tst:
    case Opcode::Teq:
    case Opcode::Cmp:
    case Opcode::Cmn:
    case Opcode::Orr:
    case Opcode::Mov:
    case Opcode::Bic:
    case Opcode::Mvn:
    case Opcode::AndS:
    case Opcode::EorS:
    case Opcode::SubS:
    case Opcode::RsbS:
    case Opcode::AddS:
    case Opcode::AdcS:
    case Opcode::SbcS:
    case Opcode::RscS:
    case Opcode::OrrS:
    case Opcode::MovS:
    case Opcode::BicS:
    case Opcode::MvnS:
        if (inst.dp.rm != 0xFF) {
            if (inst.dp.rm == 15)
                return true;
            if (inst.dp.rs != 0xFF && inst.dp.rs == 15)
                return true;
        }
        return DpReadsRn(inst.opcode) && inst.dp.rn == 15;

    // --- MSR register form reads dp.rm ---
    case Opcode::Msr:
        return inst.dp.rm != 0xFF && inst.dp.rm == 15;

    // --- MCR reads regs[rd] (Rt) ---
    case Opcode::Mcr:
        return inst.rd == 15;

    // --- single-source-register misc ops (dp.rm) ---
    case Opcode::Clz:
    case Opcode::Sxtb:
    case Opcode::Sxth:
    case Opcode::Uxtb:
    case Opcode::Uxth:
    case Opcode::Rev:
    case Opcode::Rev16:
    case Opcode::Revsh:
    case Opcode::Rbit:
    case Opcode::Ssat:
    case Opcode::Usat:
        return inst.dp.rm == 15;

    // --- read-modify-write of rd (and rn for BFI) ---
    case Opcode::Bfc:
    case Opcode::Movt:
        return inst.rd == 15;
    case Opcode::Bfi:
        return inst.rd == 15 || inst.dp.rn == 15;
    case Opcode::Sbfx:
    case Opcode::Ubfx:
        return inst.dp.rn == 15;

    // --- extend-with-add (decode maps rn==15 to the no-add opcode for
    //     Sxtab/Sxtah/Uxtab/Uxtah; for the 16-variants rn==15 is the no-add
    //     sentinel handled in the handler, not a register read) ---
    case Opcode::Sxtab:
    case Opcode::Sxtah:
    case Opcode::Uxtab:
    case Opcode::Uxtah:
    case Opcode::Sxtab16:
    case Opcode::Uxtab16:
        return inst.dp.rm == 15;

    // --- two-source-register misc ops (dp.rn, dp.rm) ---
    case Opcode::Qadd:
    case Opcode::Qsub:
    case Opcode::Qdadd:
    case Opcode::Qdsub:
    case Opcode::Sadd16:
    case Opcode::Ssub16:
    case Opcode::Sadd8:
    case Opcode::Ssub8:
    case Opcode::Uadd16:
    case Opcode::Usub16:
    case Opcode::Uadd8:
    case Opcode::Usub8:
    case Opcode::Uqadd16:
    case Opcode::Uqsub16:
    case Opcode::Uqadd8:
    case Opcode::Uqsub8:
    case Opcode::Qadd16:
    case Opcode::Qsub16:
    case Opcode::Qadd8:
    case Opcode::Qsub8:
    case Opcode::Shadd16:
    case Opcode::Shsub16:
    case Opcode::Shadd8:
    case Opcode::Shsub8:
    case Opcode::Uhadd16:
    case Opcode::Uhsub16:
    case Opcode::Uhadd8:
    case Opcode::Uhsub8:
    case Opcode::Sel:
    case Opcode::Pkhbt:
    case Opcode::Pkhtb:
    case Opcode::Usad8:
    case Opcode::Usada8: // accumulator (dp.imm) can never be 15 (decode)
        return inst.dp.rn == 15 || inst.dp.rm == 15;

    // --- multiplies (mul union) ---
    case Opcode::Mul:
    case Opcode::MulS:
    case Opcode::Umull:
    case Opcode::UmullS:
    case Opcode::Smull:
    case Opcode::SmullS:
    case Opcode::Smulxy:
    case Opcode::Smulwy:
        return inst.mul.rm == 15 || inst.mul.rs == 15;
    case Opcode::Mla:
    case Opcode::MlaS:
    case Opcode::Smlaxy:
    case Opcode::Smlawy:
        return inst.mul.rm == 15 || inst.mul.rs == 15 || inst.mul.rn == 15;
    case Opcode::Umlal:
    case Opcode::UmlalS:
    case Opcode::Smlal:
    case Opcode::SmlalS:
    case Opcode::Smlalxy: // accumulator read from rd (RdLo) and rdhi
        return inst.mul.rm == 15 || inst.mul.rs == 15 || inst.rd == 15 || inst.mul.rdhi == 15;

    // --- single loads/stores (ls union) ---
    case Opcode::Ldr:
    case Opcode::LdrB:
    case Opcode::LdrH:
    case Opcode::LdrSB:
    case Opcode::LdrSH:
        return inst.ls.rn == 15 || (inst.ls.rm != 0xFF && inst.ls.rm == 15);
    case Opcode::Str:
    case Opcode::StrB:
    case Opcode::StrH:
        return inst.ls.rn == 15 || (inst.ls.rm != 0xFF && inst.ls.rm == 15) || inst.rd == 15;
    case Opcode::Ldrd:
        return inst.ls.rn == 15 || (inst.ls.rm != 0xFF && inst.ls.rm == 15);
    case Opcode::Strd: // stores rd and rd+1
        return inst.ls.rn == 15 || (inst.ls.rm != 0xFF && inst.ls.rm == 15) || inst.rd >= 14;

    // --- load/store multiple (lsm union) ---
    case Opcode::Ldm:
        return inst.lsm.rn == 15;
    case Opcode::Stm: // STM with PC in the list stores regs[15]
        return inst.lsm.rn == 15 || (inst.lsm.reg_list & 0x8000) != 0;

    // --- register branches ---
    case Opcode::Bx:
    case Opcode::Blx:
        return inst.branch.rm == 15;

    // --- exclusives (dp union: rn = address, rm = store value) ---
    case Opcode::Ldrex:
    case Opcode::Ldrexb:
    case Opcode::Ldrexh:
    case Opcode::Ldrexd:
        return inst.dp.rn == 15;
    case Opcode::Strex:
    case Opcode::Strexb:
    case Opcode::Strexh:
        return inst.dp.rn == 15 || inst.dp.rm == 15;
    case Opcode::Strexd: // stores rm and rm+1
        return inst.dp.rn == 15 || inst.dp.rm >= 14;

    // --- VFP (vfp union: rn = base register, rt/rt2 = ARM source regs) ---
    case Opcode::VldrS:
    case Opcode::VstrS:
    case Opcode::VldrD:
    case Opcode::VstrD:
    case Opcode::VldmS:
    case Opcode::VstmS:
    case Opcode::VldmD:
    case Opcode::VstmD:
        return inst.vfp.rn == 15;
    case Opcode::VmovArmToS:
        return inst.vfp.rt == 15;
    case Opcode::VmovArmToD:
        return inst.vfp.rt == 15 || inst.vfp.rt2 == 15;
    case Opcode::Vmsr:
        return inst.vfp.rt == 15;

    // --- Thumb hi-register forms (decode folds rm==15 where the value is a
    //     constant; the remaining cases read regs[15] directly) ---
    case Opcode::ThumbAddHi: // ADD PC, Rm reads regs[rd] (rd==15)
    case Opcode::ThumbCmpHi:
        return inst.rd == 15 || inst.dp.rm == 15;
    case Opcode::ThumbMovHi: // rd==15 only writes (reads back its own write)
        return inst.dp.rm == 15;
    case Opcode::ThumbBx:
    case Opcode::ThumbBlxReg:
        return inst.dp.rm == 15;

    // Fused/unrolled opcodes are only created from instructions that passed
    // this classifier (fusion is skipped for prefixed instructions), and all
    // remaining opcodes either read no guest registers, cannot encode R15
    // (Thumb 3-bit fields, SP-relative), or only write regs[15].
    default:
        return false;
    }
}

void ARM_FastInterp::FoldPcSources(DecodedInst& inst, u32 pipeline_pc) {
    // LDR literal (imm offset, no writeback): absolute address is a constant
    if (inst.opcode == Opcode::Ldr && inst.ls.rn == 15 && inst.ls.rm == 0xFF &&
        inst.ls.mode == AddrMode::Offset) {
        inst.opcode = Opcode::LdrLit;
        inst.ls.offset = static_cast<s32>(pipeline_pc + static_cast<u32>(inst.ls.offset));
        return;
    }

    if (!IsDpAluOpcode(inst.opcode))
        return;

    // Op2 = PC shifted by an immediate amount: fold the shifted value into an
    // immediate operand. RRX (ROR #0) consumes the runtime C flag for its
    // value and cannot be folded.
    if (inst.dp.rm == 15 && inst.dp.rs == 0xFF &&
        !(inst.dp.shift_type == ShiftType::ROR && inst.dp.shift_imm == 0)) {
        const bool lsl0 = (inst.dp.shift_type == ShiftType::LSL && inst.dp.shift_imm == 0);
        bool carry = false;
        const u32 val =
            lsl0 ? pipeline_pc
                 : DecodeShiftImm(pipeline_pc, inst.dp.shift_type, inst.dp.shift_imm, carry);
        if (!DpUsesShifterCarry(inst.opcode)) {
            // Arithmetic / non-S ops ignore the shifter carry entirely
            inst.dp.rm = 0xFF;
            inst.dp.imm = val;
            inst.dp.flags = 0;
        } else if (lsl0) {
            // LSL #0 passes the old C through - identical to an immediate
            // operand with flags=0 in GetOp2
            inst.dp.rm = 0xFF;
            inst.dp.imm = val;
            inst.dp.flags = 0;
        } else if (carry == (((val >> 31) & 1) != 0)) {
            // GetOp2's rotated-immediate convention (carry = imm bit 31)
            // reproduces this shifter carry exactly: safe to fold
            inst.dp.rm = 0xFF;
            inst.dp.imm = val;
            inst.dp.flags = 1;
        }
        // else: shifter carry not representable as an immediate - PcSetup
    }

    // Rn = PC with an immediate op2 and a pure operation (no flags, no
    // carry-in): the whole result is a decode-time constant -> MOV #imm
    if (inst.dp.rn == 15 && inst.dp.rm == 0xFF && DpReadsRn(inst.opcode)) {
        u32 val;
        switch (inst.opcode) {
        case Opcode::And:
            val = pipeline_pc & inst.dp.imm;
            break;
        case Opcode::Eor:
            val = pipeline_pc ^ inst.dp.imm;
            break;
        case Opcode::Sub:
            val = pipeline_pc - inst.dp.imm;
            break;
        case Opcode::Rsb:
            val = inst.dp.imm - pipeline_pc;
            break;
        case Opcode::Add:
            val = pipeline_pc + inst.dp.imm;
            break;
        case Opcode::Orr:
            val = pipeline_pc | inst.dp.imm;
            break;
        case Opcode::Bic:
            val = pipeline_pc & ~inst.dp.imm;
            break;
        default:
            return; // S-forms, compares, ADC/SBC/RSC: PcSetup prefix
        }
        inst.opcode = Opcode::Mov;
        inst.dp.rn = 0;
        inst.dp.rm = 0xFF;
        inst.dp.imm = val;
        inst.dp.flags = 0;
    }
}

// ============================================================================
// Instruction Fusion
// ============================================================================
// A compare/test followed by a conditional branch executes as one fused opcode.
// Each row maps a source opcode to its immediate- and register-operand fused
// forms and the per-pattern stats counter.

namespace {

struct ArmFusePattern {
    Opcode src;
    Opcode fused_imm;
    Opcode fused_reg;
    bool imm_shifter_carry; // fused.rm = 0xFE encodes "C = imm bit 31" (TST/TEQ)
    u64 FusionStats::* counter;
};

constexpr ArmFusePattern arm_fuse_patterns[] = {
    {Opcode::SubS, Opcode::FusedSubsBcc, Opcode::FusedSubsRegBcc, false,
     &FusionStats::subs_bcc_fusions},
    {Opcode::Cmp, Opcode::FusedCmpBcc, Opcode::FusedCmpRegBcc, false,
     &FusionStats::cmp_bcc_fusions},
    {Opcode::Tst, Opcode::FusedTstBcc, Opcode::FusedTstRegBcc, true, &FusionStats::tst_bcc_fusions},
    {Opcode::Teq, Opcode::FusedTeqBcc, Opcode::FusedTeqRegBcc, true, &FusionStats::teq_bcc_fusions},
};

struct ThumbFusePattern {
    Opcode src;
    Opcode fused;
    bool imm_form; // operand is imm8 (dp.imm); otherwise a register (dp.rm)
    u64 FusionStats::* counter;
};

constexpr ThumbFusePattern thumb_fuse_patterns[] = {
    {Opcode::ThumbSubImm8, Opcode::ThumbFusedSubsBcc, true, nullptr},
    {Opcode::ThumbCmpImm8, Opcode::ThumbFusedCmpBcc, true, nullptr},
    {Opcode::ThumbCmpReg, Opcode::ThumbFusedCmpRegBcc, false, nullptr},
    {Opcode::ThumbTst, Opcode::ThumbFusedTstBcc, false, &FusionStats::tst_bcc_fusions},
};

} // namespace

bool ARM_FastInterp::TryFuseArm(DecodedInst& inst, u32 pc) {
    // Compare/test + Bcc
    for (const ArmFusePattern& p : arm_fuse_patterns) {
        if (inst.opcode != p.src || inst.cond != 0xE) {
            continue;
        }
        if (p.src == Opcode::SubS && inst.rd == 15) {
            return false;
        }
        fusion_stats_.fusions_attempted++;
        const u32 next_inst = ReadMemory32(pc + 4);
        if ((next_inst & 0x0F000000) != 0x0A000000) {
            return false;
        }
        const u8 branch_cond = (next_inst >> 28) & 0xF;
        if (branch_cond >= 0xE) {
            return false;
        }
        // The fused fields alias the dp fields in the union: capture the dp
        // operands into locals before writing anything
        Opcode fused_op;
        u8 fused_rm;
        u32 fused_imm;
        if (inst.dp.rm == 0xFF) {
            fused_op = p.fused_imm;
            fused_rm = (p.imm_shifter_carry && (inst.dp.flags & 1)) ? 0xFE : 0xFF;
            fused_imm = inst.dp.imm;
        } else if (inst.dp.rs == 0xFF && inst.dp.shift_imm == 0 &&
                   inst.dp.shift_type == ShiftType::LSL) {
            fused_op = p.fused_reg;
            fused_rm = inst.dp.rm;
            fused_imm = 0;
        } else {
            return false; // shifted register: don't fuse
        }
        const u8 rn = inst.dp.rn;
        const s32 offset = static_cast<s32>((next_inst & 0x00FFFFFF) << 8) >> 6;
        inst.opcode = fused_op;
        inst.fused.rn = rn;
        inst.fused.rm = fused_rm;
        inst.fused.branch_cond = branch_cond;
        inst.fused.imm = fused_imm;
        inst.fused.branch_target = pc + 12 + offset;
        inst.fused.combined_size = 8;
        inst.inst_size = 8;
        fusion_stats_.fusions_applied++;
        fusion_stats_.*p.counter += 1;
        return true;
    }

    // ADD Rd, Rn, #imm; CMP Rd, Rm; B<cond> - three-instruction iterator loop
    if (inst.opcode == Opcode::Add && inst.cond == 0xE && inst.rd != 15 && inst.dp.rm == 0xFF) {
        fusion_stats_.fusions_attempted++;
        const u32 next_inst = ReadMemory32(pc + 4);
        // Next must be CMP Rd, Rm (unconditional) comparing the ADD result:
        // cccc 00I1 0101 nnnn 0000 oooo oooo oooo
        if ((next_inst & 0x0FF0F000) != 0x01500000 || ((next_inst >> 28) & 0xF) != 0xE ||
            ((next_inst >> 16) & 0xF) != inst.rd) {
            return false;
        }
        const u32 third_inst = ReadMemory32(pc + 8);
        if ((third_inst & 0x0F000000) != 0x0A000000) {
            return false;
        }
        const u8 branch_cond = (third_inst >> 28) & 0xF;
        if (branch_cond >= 0xE) {
            return false;
        }
        // CMP operand must be a plain register other than PC (the fused
        // handler reads regs[fused.rm])
        const bool cmp_is_imm = (next_inst >> 25) & 1;
        if (cmp_is_imm || ((next_inst >> 4) & 0xFF) != 0 || (next_inst & 0xF) == 15) {
            return false;
        }
        const u8 cmp_rm = next_inst & 0xF;
        const u8 rn = inst.dp.rn;
        const u32 add_imm = inst.dp.imm;
        const s32 offset = static_cast<s32>((third_inst & 0x00FFFFFF) << 8) >> 6;
        inst.opcode = Opcode::FusedAddCmpBcc;
        inst.fused.rn = rn;
        inst.fused.rm = cmp_rm;
        inst.fused.branch_cond = branch_cond;
        inst.fused.imm = add_imm;
        inst.fused.branch_target = pc + 16 + offset;
        inst.fused.combined_size = 12;
        inst.inst_size = 12;
        fusion_stats_.fusions_applied++;
        fusion_stats_.add_cmp_bcc_fusions++;
        return true;
    }

    return false;
}

bool ARM_FastInterp::TryFuseThumb(DecodedInst& inst, u32 pc) {
    for (const ThumbFusePattern& p : thumb_fuse_patterns) {
        if (inst.opcode != p.src) {
            continue;
        }
        fusion_stats_.fusions_attempted++;
        const u16 next_inst = ReadMemory16(pc + 2);
        const u8 branch_cond = (next_inst >> 8) & 0xF;
        if ((next_inst & 0xF000) != 0xD000 || branch_cond >= 0xE) {
            return false;
        }
        // rd doubles as the source register for all four patterns; capture the
        // dp operands before writing the aliasing fused fields
        const u8 rn = inst.rd;
        const u8 rm = p.imm_form ? u8{0xFF} : inst.dp.rm;
        const u32 imm = p.imm_form ? (inst.dp.imm & 0xFF) : 0;
        const s32 offset = static_cast<s8>(next_inst & 0xFF) << 1;
        inst.opcode = p.fused;
        inst.fused.rn = rn;
        inst.fused.rm = rm;
        inst.fused.branch_cond = branch_cond;
        inst.fused.imm = imm;
        inst.fused.branch_target = pc + 6 + offset;
        inst.fused.combined_size = 4;
        inst.inst_size = 4;
        fusion_stats_.fusions_applied++;
        fusion_stats_.thumb_fusions++;
        if (p.counter) {
            fusion_stats_.*p.counter += 1;
        }
        return true;
    }
    return false;
}

void ARM_FastInterp::DecodeBlockInto(BasicBlock* block, u32 pc) {
    block->SetThumb(state_.thumb_mode);
    block->inst_count = 0; // Reset before decoding (critical for re-decode on mode change)
    block->chain_target = nullptr;
    fusion_stats_.blocks_decoded++;

    u32 current_pc = pc;
    bool end_block = false;

    // The just-decoded instruction at instructions[block->inst_count] may
    // read regs[15] as a source. Fold it to a constant where possible;
    // otherwise shift it one slot down and insert a PcSetup prefix that
    // materializes the pipeline-adjusted PC. Returns false when there is no
    // room for the extra slot (caller ends the block before the instruction).
    // A prefixed slot holds Opcode::PcSetup, so the fusion pattern checks
    // below never match (fusion of PC-reading instructions is thereby skipped).
    auto resolve_pc_sources = [&block](DecodedInst& inst, u32 pipeline_pc) -> bool {
        if (!ReadsPcAsSource(inst))
            return true;
        FoldPcSources(inst, pipeline_pc);
        if (!ReadsPcAsSource(inst))
            return true;
        if (block->inst_count + 2 > MAX_BLOCK_SIZE - 1)
            return false;
        block->instructions[block->inst_count + 1] = inst;
        std::memset(&inst, 0, sizeof(inst));
        inst.opcode = Opcode::PcSetup;
        inst.cond = 0xE;
        inst.imm32 = pipeline_pc;
        block->inst_count++; // prefix slot; the shared increment below counts the real one
        return true;
    };

    // Cap real decoding at MAX_BLOCK_SIZE - 1: the last slot is reserved for the
    // EndBlock terminator appended below (DISPATCH has no bounds check).
    while (!end_block && block->inst_count < MAX_BLOCK_SIZE - 1) {
        DecodedInst& inst = block->instructions[block->inst_count];

        if (state_.thumb_mode) {
            u16 thumb_inst = ReadMemory16(current_pc);
            // Check for 32-bit Thumb instruction
            if ((thumb_inst & 0xE000) == 0xE000 && (thumb_inst & 0x1800) != 0) {
                u16 thumb_inst2 = ReadMemory16(current_pc + 2);
                u32 full_inst = (static_cast<u32>(thumb_inst) << 16) | thumb_inst2;
                DecodeThumb32(full_inst, current_pc, inst);
                if (!resolve_pc_sources(inst, current_pc + 4))
                    break; // no room for the PcSetup prefix: end block here
                current_pc += 4;
            } else {
                DecodeThumb16(thumb_inst, current_pc, inst);
                if (!resolve_pc_sources(inst, current_pc + 4))
                    break; // no room for the PcSetup prefix: end block here

#if !FASTINTERP_DISABLE_FUSION
                if (TryFuseThumb(inst, current_pc)) {
                    current_pc += 4; // both fused instructions
                    block->inst_count++;
                    end_block = true;
                    break;
                }
#endif
                current_pc += 2;
            }
        } else {
            u32 arm_inst = ReadMemory32(current_pc);
            DecodeARM(arm_inst, current_pc, inst);
            if (!resolve_pc_sources(inst, current_pc + 8))
                break; // no room for the PcSetup prefix: end block here

#if !FASTINTERP_DISABLE_FUSION
            if (TryFuseArm(inst, current_pc)) {
                current_pc += inst.inst_size; // 8, or 12 for ADD+CMP+Bcc
                block->inst_count++;
                end_block = true;
                break;
            }
#endif
            current_pc += 4;
        }

        block->inst_count++;

        // Check if this instruction ends the block. When a PcSetup prefix was
        // inserted, the real instruction is the last counted slot (the prefix
        // increment above plus the shared increment cover both).
        const DecodedInst& last = block->instructions[block->inst_count - 1];
        switch (last.opcode) {
        case Opcode::B:
        case Opcode::Bl:
        case Opcode::Bx:
        case Opcode::Blx:
        case Opcode::BlxImm:
        case Opcode::Swi:
        case Opcode::Invalid:
        // Thumb branches also end blocks
        case Opcode::ThumbB:
        case Opcode::ThumbBCond:
        case Opcode::ThumbBx:
        case Opcode::ThumbBlxReg:
        case Opcode::ThumbBl:
        case Opcode::ThumbBlxImm:
        case Opcode::ThumbSwi:
            // (fused opcodes never reach this switch: the TryFuse* paths exit
            // the decode loop directly)
            end_block = true;
            break;
        default:
            // Also end block if we modify PC directly
            if (last.rd == 15) {
                end_block = true;
            }
            break;
        }
    }

    block->end_pc = current_pc;

    // Compute tick costs. Delegate every real (non-fused) instruction to the canonical
    // Core::TicksForInstruction (the per-instruction model the Dynarmic JIT charges) so
    // FastInterp's guest timing matches Dynarmic's - thread scheduling is tick-driven, and
    // an approximate model desyncs it. Fused/unrolled opcodes represent several guest
    // instructions and can't be described by one raw word, so ComputeTicks handles those.
    bool is_thumb = state_.thumb_mode;
    u32 tick_addr = pc;
    for (u16 i = 0; i < block->inst_count; i++) {
        DecodedInst& inst = block->instructions[i];
        if (inst.opcode == Opcode::PcSetup) {
            inst.ticks = 0; // PcSetup prefix: zero guest instructions (the real
                            // instruction in the next slot carries the cost)
        } else if (IsFusedOrUnrolled(inst.opcode)) {
            inst.ticks = ComputeTicks(inst, is_thumb);
        } else if (is_thumb) {
            inst.ticks = 1; // canonical: every Thumb instruction is 1 tick
        } else {
            inst.ticks = static_cast<u8>(Core::TicksForInstruction(false, ReadMemory32(tick_addr)));
        }
        tick_addr += inst.inst_size;
    }

    fusion_stats_.dead_flags_eliminated += EliminateDeadFlags(block);

    // Append the EndBlock terminator. Every block gets one - conditional
    // block-enders (Bcc, fused not-taken paths, conditional Bx, ...) DISPATCH on
    // their not-taken path, and DISPATCH has no bounds check. The terminator
    // is not counted in inst_count and does not affect end_pc, so the tick loop
    // and EliminateDeadFlags above never see it (its inst_size and ticks are 0).
    // The decode loop capped real instructions at MAX_BLOCK_SIZE - 1, so this
    // slot always exists.
    DecodedInst& term = block->instructions[block->inst_count];
    std::memset(&term, 0, sizeof(term));
    term.opcode = Opcode::EndBlock;
    term.cond = 0xE;
}

// ============================================================================
// Hot Block Reoptimization
// ============================================================================
// When a block becomes hot (frequently executed), we apply additional
// optimizations like loop unrolling that are too expensive for cold blocks.

void ARM_FastInterp::ReoptimizeHotBlock(BasicBlock* block) {
    // Skip if already reoptimized
    if (block->IsReoptimized()) {
        return;
    }

    block->SetReoptimized();

    // Check if this is a single-instruction block with a tight loop pattern
    if (block->inst_count != 1) {
        return; // Only optimize single-instruction (fused) blocks for now
    }

    DecodedInst& inst = block->instructions[0];

    // =========================================================================
    // Tight Loop Detection and Unrolling
    // =========================================================================
    // A fused compare+branch whose taken-branch target is the block's own start
    // is a tight self-loop: swap in the unrolled handler.

    // Thumb: SUBS Rd, #imm8; BNE self
    if (inst.opcode == Opcode::ThumbFusedSubsBcc && inst.fused.branch_cond == 1 && // BNE
        inst.fused.branch_target == block->start_pc) {
        inst.opcode = Opcode::ThumbUnrolledSubsLoop;
        fusion_stats_.hot_blocks_reoptimized++;
        return;
    }

    // Thumb: CMP Rn, Rm; BNE self (spin/wait loop)
    if (inst.opcode == Opcode::ThumbFusedCmpRegBcc && inst.fused.branch_cond == 1 && // BNE
        inst.fused.branch_target == block->start_pc) {
        inst.opcode = Opcode::ThumbUnrolledCmpLoop;
        fusion_stats_.hot_blocks_reoptimized++;
        return;
    }

    // ARM: SUBS Rd, Rn, #imm; BNE self
    if (inst.opcode == Opcode::FusedSubsBcc && inst.fused.branch_cond == 1 && // BNE
        inst.fused.branch_target == block->start_pc) {
        inst.opcode = Opcode::ArmUnrolledSubsLoop;
        fusion_stats_.hot_blocks_reoptimized++;
        return;
    }

    // Only single-instruction (ic==1) fused self-loops are unrolled here; other
    // loop shapes (ARM CMP/ADD loops, multi-instruction bodies) are not handled.
}

// ============================================================================
// ARM Instruction Decoding
// ============================================================================

// One helper per large encoding space of bits[27:25], mirroring the ARM
// encoding table. All are pure: raw instruction word in, DecodedInst out.

// Data processing register, multiply, halfword/dual/exclusive load/store,
// and the miscellaneous space (op1 = 0b000)
static void DecodeArmDataProcRegOrMisc(u32 inst, DecodedInst& out) {
    // Data processing register, multiply, halfword LS, and misc instructions
    u32 op2 = (inst >> 4) & 0xF;

    // Check miscellaneous instructions first (encoded in DP space)
    // BX: 0x012FFF10
    if ((inst & 0x0FFFFFF0) == 0x012FFF10) {
        out.branch.rm = inst & 0xF;
        out.branch.target = 0;
        out.rd = 0;
        out.opcode = Opcode::Bx;
        return;
    }
    // BLX register: 0x012FFF30
    if ((inst & 0x0FFFFFF0) == 0x012FFF30) {
        out.branch.rm = inst & 0xF;
        out.branch.target = 0;
        out.rd = 0;
        out.opcode = Opcode::Blx;
        return;
    }
    // CLZ: 0x016F0F10
    if ((inst & 0x0FFF0FF0) == 0x016F0F10) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.opcode = Opcode::Clz;
        return;
    }
    // MRS: 0x010F0000
    if ((inst & 0x0FBF0FFF) == 0x010F0000) {
        out.rd = (inst >> 12) & 0xF;
        out.opcode = Opcode::Mrs;
        return;
    }
    // MSR register: 0x0120F000
    if ((inst & 0x0FB0FFF0) == 0x0120F000) {
        out.dp.rm = inst & 0xF;
        out.dp.flags = (inst >> 16) & 0xF;
        out.rd = 0;
        out.opcode = Opcode::Msr;
        return;
    }
    // QADD/QSUB/QDADD/QDSUB: 0x01000050
    if ((inst & 0x0F9000F0) == 0x01000050) {
        u32 op = (inst >> 21) & 0x3;
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = (inst >> 16) & 0xF;
        out.dp.rm = inst & 0xF;
        switch (op) {
        case 0:
            out.opcode = Opcode::Qadd;
            break;
        case 1:
            out.opcode = Opcode::Qsub;
            break;
        case 2:
            out.opcode = Opcode::Qdadd;
            break;
        case 3:
            out.opcode = Opcode::Qdsub;
            break;
        }
        return;
    }

    // Halfword multiplies (misc space): bits[27:23]=00010, S=0, bit7=1, bit4=0
    // op (bits[22:21]): 0=SMLA<x><y>, 1=SMLAW<y>/SMULW<y>, 2=SMLAL<x><y>, 3=SMUL<x><y>
    // x = bit5 (Rm half), y = bit6 (Rs half). For op=1, bit5 instead picks
    // SMULW (1) vs SMLAW (0).
    if ((inst & 0x0F900090) == 0x01000080) {
        const u32 op = (inst >> 21) & 0x3;
        const bool x = (inst >> 5) & 1;
        const bool y = (inst >> 6) & 1;
        out.rd = (inst >> 16) & 0xF;
        out.mul.rn = (inst >> 12) & 0xF;
        out.mul.rs = (inst >> 8) & 0xF;
        out.mul.rm = inst & 0xF;
        out.mul.rdhi = (inst >> 16) & 0xF;
        out.mul.flags = (x ? 1 : 0) | (y ? 2 : 0);
        switch (op) {
        case 0:
            out.opcode = Opcode::Smlaxy;
            break;
        case 1:
            out.opcode = x ? Opcode::Smulwy : Opcode::Smlawy;
            break;
        case 2:
            // SMLAL<x><y>: RdLo = bits[15:12] (RdHi already = bits[19:16] above)
            out.rd = (inst >> 12) & 0xF;
            out.opcode = Opcode::Smlalxy;
            break;
        case 3:
            out.opcode = Opcode::Smulxy;
            break;
        }
        return;
    }

    // Exclusive load/store: bits[27:23]=00011, bits[7:4]=1001
    // op1 range 0x18-0x1F covers all variants
    if (op2 == 0x9 && ((inst >> 20) & 0xF8) == 0x18) {
        u32 op1 = (inst >> 20) & 0xFF;
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = (inst >> 16) & 0xF;
        out.dp.rm = inst & 0xF;
        bool load = op1 & 1;          // bit[20]
        u32 variant = (op1 >> 1) & 3; // bits[22:21]: 0=word, 1=dword, 2=byte, 3=half
        if (load) {
            switch (variant) {
            case 0:
                out.opcode = Opcode::Ldrex;
                break;
            case 1:
                out.opcode = Opcode::Ldrexd;
                break;
            case 2:
                out.opcode = Opcode::Ldrexb;
                break;
            case 3:
                out.opcode = Opcode::Ldrexh;
                break;
            }
        } else {
            switch (variant) {
            case 0:
                out.opcode = Opcode::Strex;
                break;
            case 1:
                out.opcode = Opcode::Strexd;
                break;
            case 2:
                out.opcode = Opcode::Strexb;
                break;
            case 3:
                out.opcode = Opcode::Strexh;
                break;
            }
        }
        // ARMv6K LDREXD/STREXD transfer the pair Rt/Rt+1: Rt odd or
        // Rt==14 is UNPREDICTABLE - and the handlers would index regs[16]
        // for Rt==15. Reject at decode.
        if ((out.opcode == Opcode::Ldrexd && ((out.rd & 1) || out.rd >= 14)) ||
            (out.opcode == Opcode::Strexd && ((out.dp.rm & 1) || out.dp.rm >= 14))) {
            out.opcode = Opcode::Invalid;
        }
        return;
    }

    // Multiply (bits[7:4]=1001)
    if (op2 == 0x9) {
        u32 mul_op = (inst >> 21) & 0xF;
        bool s_flag = (inst >> 20) & 1;
        out.rd = (inst >> 16) & 0xF;
        out.mul.rn = (inst >> 12) & 0xF;
        out.mul.rs = (inst >> 8) & 0xF;
        out.mul.rm = inst & 0xF;
        out.mul.rdhi = out.mul.rn;

        switch (mul_op) {
        case 0x0:
            out.opcode = s_flag ? Opcode::MulS : Opcode::Mul;
            break;
        case 0x1:
            out.opcode = s_flag ? Opcode::MlaS : Opcode::Mla;
            break;
        case 0x4:
            out.opcode = s_flag ? Opcode::UmullS : Opcode::Umull;
            out.mul.rdhi = (inst >> 16) & 0xF;
            out.rd = (inst >> 12) & 0xF;
            break;
        case 0x5:
            out.opcode = s_flag ? Opcode::UmlalS : Opcode::Umlal;
            out.mul.rdhi = (inst >> 16) & 0xF;
            out.rd = (inst >> 12) & 0xF;
            break;
        case 0x6:
            out.opcode = s_flag ? Opcode::SmullS : Opcode::Smull;
            out.mul.rdhi = (inst >> 16) & 0xF;
            out.rd = (inst >> 12) & 0xF;
            break;
        case 0x7:
            out.opcode = s_flag ? Opcode::SmlalS : Opcode::Smlal;
            out.mul.rdhi = (inst >> 16) & 0xF;
            out.rd = (inst >> 12) & 0xF;
            break;
        default:
            out.opcode = Opcode::Invalid;
            break;
        }
        return;
    }

    // Halfword/signed byte load/store (bits[7]=1, bits[4]=1, bits[6:5]!=00)
    if ((op2 & 0x9) == 0x9 && (op2 & 0x6) != 0) {
        bool load = (inst >> 20) & 1;
        bool imm = (inst >> 22) & 1;
        u32 sh = (inst >> 5) & 0x3;

        out.rd = (inst >> 12) & 0xF;
        out.ls.rn = (inst >> 16) & 0xF;

        if (imm) {
            u32 imm_hi = (inst >> 8) & 0xF;
            u32 imm_lo = inst & 0xF;
            out.ls.offset = (imm_hi << 4) | imm_lo;
            out.ls.rm = 0xFF;
        } else {
            out.ls.rm = inst & 0xF;
            out.ls.offset = 0;
        }

        bool u = (inst >> 23) & 1;
        bool p = (inst >> 24) & 1;
        bool w = (inst >> 21) & 1;

        if (!u)
            out.ls.offset = -out.ls.offset;

        if (p && !w)
            out.ls.mode = AddrMode::Offset;
        else if (p && w)
            out.ls.mode = AddrMode::PreIndex;
        else
            out.ls.mode = AddrMode::PostIndex;

        out.ls.shift_type = ShiftType::LSL;
        out.ls.shift_imm = 0;
        out.ls.flags = u ? 0x01 : 0; // preserve U for register-offset subtract

        if (load) {
            switch (sh) {
            case 1:
                out.opcode = Opcode::LdrH;
                break;
            case 2:
                out.opcode = Opcode::LdrSB;
                break;
            case 3:
                out.opcode = Opcode::LdrSH;
                break;
            default:
                out.opcode = Opcode::Invalid;
                break;
            }
        } else {
            switch (sh) {
            case 1:
                out.opcode = Opcode::StrH;
                break;
            case 2:
                out.opcode = Opcode::Ldrd;
                break;
            case 3:
                out.opcode = Opcode::Strd;
                break;
            default:
                out.opcode = Opcode::Invalid;
                break;
            }
        }
        // ARM LDRD/STRD transfer the pair Rt/Rt+1: Rt odd is UNDEFINED and
        // Rt==14 is UNPREDICTABLE (Rt2 would be PC) - and the handlers
        // would index regs[16] for Rt==15. Reject at decode.
        if (out.opcode == Opcode::Ldrd || out.opcode == Opcode::Strd) {
            if ((out.rd & 1) || out.rd >= 14) {
                out.opcode = Opcode::Invalid;
                return;
            }
            out.ls.rt2 = out.rd + 1;
        }

        // Decode-time specialization: plain imm-offset halfword with
        // rd/rn != 15 (signed loads stay generic)
        if (imm && out.ls.mode == AddrMode::Offset && out.rd != 15 && out.ls.rn != 15) {
            if (out.opcode == Opcode::LdrH) {
                out.opcode = Opcode::LdrHImm;
            } else if (out.opcode == Opcode::StrH) {
                out.opcode = Opcode::StrHImm;
            }
        }
        return;
    }

    // Data processing register (bits[7:4] != 1001 and not halfword LS)
    if ((op2 & 0x9) != 0x9) {
        u32 opcode_field = (inst >> 21) & 0xF;
        bool s_flag = (inst >> 20) & 1;

        // Compare opcodes (10xx) without S are not data processing - they are the
        // miscellaneous space (BX, CLZ, MRS/MSR, QADD, halfword multiplies, BKPT...).
        // Everything legitimate was matched above; decoding the rest as TST/TEQ/CMP/CMN
        // would silently corrupt flags, so reject it.
        if (!s_flag && (opcode_field & 0xC) == 0x8) {
            out.opcode = Opcode::Invalid;
            return;
        }

        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = (inst >> 16) & 0xF;
        out.dp.rm = inst & 0xF;

        out.dp.shift_type = static_cast<ShiftType>((inst >> 5) & 0x3);
        if ((inst >> 4) & 1) {
            out.dp.rs = (inst >> 8) & 0xF;
            out.dp.shift_imm = 0;
        } else {
            out.dp.rs = 0xFF;
            out.dp.shift_imm = (inst >> 7) & 0x1F;
        }
        out.dp.flags = 0;

        switch (opcode_field) {
        case 0x0:
            out.opcode = s_flag ? Opcode::AndS : Opcode::And;
            break;
        case 0x1:
            out.opcode = s_flag ? Opcode::EorS : Opcode::Eor;
            break;
        case 0x2:
            out.opcode = s_flag ? Opcode::SubS : Opcode::Sub;
            break;
        case 0x3:
            out.opcode = s_flag ? Opcode::RsbS : Opcode::Rsb;
            break;
        case 0x4:
            out.opcode = s_flag ? Opcode::AddS : Opcode::Add;
            break;
        case 0x5:
            out.opcode = s_flag ? Opcode::AdcS : Opcode::Adc;
            break;
        case 0x6:
            out.opcode = s_flag ? Opcode::SbcS : Opcode::Sbc;
            break;
        case 0x7:
            out.opcode = s_flag ? Opcode::RscS : Opcode::Rsc;
            break;
        case 0x8:
            out.opcode = Opcode::Tst;
            break;
        case 0x9:
            out.opcode = Opcode::Teq;
            break;
        case 0xA:
            out.opcode = Opcode::Cmp;
            break;
        case 0xB:
            out.opcode = Opcode::Cmn;
            break;
        case 0xC:
            out.opcode = s_flag ? Opcode::OrrS : Opcode::Orr;
            break;
        case 0xD:
            out.opcode = s_flag ? Opcode::MovS : Opcode::Mov;
            break;
        case 0xE:
            out.opcode = s_flag ? Opcode::BicS : Opcode::Bic;
            break;
        case 0xF:
            out.opcode = s_flag ? Opcode::MvnS : Opcode::Mvn;
            break;
        }
        return;
    }

    out.opcode = Opcode::Invalid;
    return;
}

// Data processing immediate, MSR immediate, MOVW/MOVT (op1 = 0b001)
static void DecodeArmDataProcImm(u32 inst, DecodedInst& out) {
    // Data processing immediate - check MSR immediate first, then DP
    // MSR immediate: bits[27:20] = 0x32, bits[15:12] = 0xF
    if ((inst & 0x0FB0F000) == 0x0320F000) {
        u32 imm = inst & 0xFF;
        u32 rotate = ((inst >> 8) & 0xF) * 2;
        if (rotate != 0) {
            out.dp.imm = (imm >> rotate) | (imm << (32 - rotate));
        } else {
            out.dp.imm = imm;
        }
        out.dp.rm = 0xFF;
        out.dp.flags = (inst >> 16) & 0xF;
        out.rd = 0;
        out.opcode = Opcode::Msr;
        return;
    }

    // Standard DP immediate
    u32 opcode_field = (inst >> 21) & 0xF;
    bool s_flag = (inst >> 20) & 1;
    out.rd = (inst >> 12) & 0xF;
    out.dp.rn = (inst >> 16) & 0xF;
    out.dp.rm = 0xFF;
    out.dp.rs = 0;

    // MOVW/MOVT: opcodes 0x8/0xA with S=0 are move-halfword, not TST/CMP
    if (!s_flag && (opcode_field == 0x8 || opcode_field == 0xA)) {
        u32 imm4 = (inst >> 16) & 0xF;
        u32 imm12 = inst & 0xFFF;
        u32 imm16 = (imm4 << 12) | imm12;
        out.dp.imm = imm16;
        out.dp.shift_type = ShiftType::LSL;
        out.dp.shift_imm = 0;
        if (opcode_field == 0x8) {
            // MOVW Rd, #imm16
            out.opcode = Opcode::Mov;
            out.dp.flags = 2; // Flag 2 = MOVW (16-bit immediate)
        } else {
            // MOVT Rd, #imm16
            out.opcode = Opcode::Movt;
            out.dp.flags = 0;
        }
        return;
    }

    // TEQ/CMN immediate without S is the unmatched MSR-immediate space (e.g. MSR
    // SPSR_x, #imm) - decoding it as a compare would silently corrupt flags.
    if (!s_flag && (opcode_field == 0x9 || opcode_field == 0xB)) {
        out.opcode = Opcode::Invalid;
        return;
    }

    u32 imm = inst & 0xFF;
    u32 rotate = ((inst >> 8) & 0xF) * 2;
    if (rotate != 0) {
        out.dp.imm = (imm >> rotate) | (imm << (32 - rotate));
        out.dp.flags = 1;
    } else {
        out.dp.imm = imm;
        out.dp.flags = 0;
    }
    out.dp.shift_type = ShiftType::LSL;
    out.dp.shift_imm = 0;

    switch (opcode_field) {
    case 0x0:
        out.opcode = s_flag ? Opcode::AndS : Opcode::And;
        break;
    case 0x1:
        out.opcode = s_flag ? Opcode::EorS : Opcode::Eor;
        break;
    case 0x2:
        out.opcode = s_flag ? Opcode::SubS : Opcode::Sub;
        break;
    case 0x3:
        out.opcode = s_flag ? Opcode::RsbS : Opcode::Rsb;
        break;
    case 0x4:
        out.opcode = s_flag ? Opcode::AddS : Opcode::Add;
        break;
    case 0x5:
        out.opcode = s_flag ? Opcode::AdcS : Opcode::Adc;
        break;
    case 0x6:
        out.opcode = s_flag ? Opcode::SbcS : Opcode::Sbc;
        break;
    case 0x7:
        out.opcode = s_flag ? Opcode::RscS : Opcode::Rsc;
        break;
    case 0x8:
        out.opcode = Opcode::Tst;
        break;
    case 0x9:
        out.opcode = Opcode::Teq;
        break;
    case 0xA:
        out.opcode = Opcode::Cmp;
        break;
    case 0xB:
        out.opcode = Opcode::Cmn;
        break;
    case 0xC:
        out.opcode = s_flag ? Opcode::OrrS : Opcode::Orr;
        break;
    case 0xD:
        out.opcode = s_flag ? Opcode::MovS : Opcode::Mov;
        break;
    case 0xE:
        out.opcode = s_flag ? Opcode::BicS : Opcode::Bic;
        break;
    case 0xF:
        out.opcode = s_flag ? Opcode::MvnS : Opcode::Mvn;
        break;
    }
    return;
}

// Media instructions and register-offset load/store (op1 = 0b011)
static void DecodeArmMediaOrLoadStoreReg(u32 inst, DecodedInst& out) {
    // Load/Store register OR special media instructions
    u32 op2 = (inst >> 4) & 0xF;

    // Check for special media instructions first (encoded in LS register space)
    // REV: 0x06BF0F30
    if ((inst & 0x0FFF0FF0) == 0x06BF0F30) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.opcode = Opcode::Rev;
        return;
    }
    // REV16: 0x06BF0FB0
    if ((inst & 0x0FFF0FF0) == 0x06BF0FB0) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.opcode = Opcode::Rev16;
        return;
    }
    // REVSH: 0x06FF0FB0
    if ((inst & 0x0FFF0FF0) == 0x06FF0FB0) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.opcode = Opcode::Revsh;
        return;
    }
    // RBIT: cond 0110 1111 1111 Rd 1111 0011 Rm
    if ((inst & 0x0FFF0FF0) == 0x06FF0F30) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.opcode = Opcode::Rbit;
        return;
    }

    // Extension instructions (with and without add)
    // SXTB/SXTAB: bits[27:20]=0x6A, bits[9:4]=000111
    if ((inst & 0x0FF003F0) == 0x06A00070) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = ((inst >> 10) & 0x3) * 8;
        u32 rn = (inst >> 16) & 0xF;
        if (rn == 0xF) {
            out.opcode = Opcode::Sxtb;
        } else {
            out.dp.rn = rn;
            out.opcode = Opcode::Sxtab;
        }
        return;
    }
    // SXTH/SXTAH: bits[27:20]=0x6B, bits[9:4]=000111
    if ((inst & 0x0FF003F0) == 0x06B00070) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = ((inst >> 10) & 0x3) * 8;
        u32 rn = (inst >> 16) & 0xF;
        if (rn == 0xF) {
            out.opcode = Opcode::Sxth;
        } else {
            out.dp.rn = rn;
            out.opcode = Opcode::Sxtah;
        }
        return;
    }
    // UXTB/UXTAB: bits[27:20]=0x6E, bits[9:4]=000111
    if ((inst & 0x0FF003F0) == 0x06E00070) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = ((inst >> 10) & 0x3) * 8;
        u32 rn = (inst >> 16) & 0xF;
        if (rn == 0xF) {
            out.opcode = Opcode::Uxtb;
        } else {
            out.dp.rn = rn;
            out.opcode = Opcode::Uxtab;
        }
        return;
    }
    // UXTH/UXTAH: bits[27:20]=0x6F, bits[9:4]=000111
    if ((inst & 0x0FF003F0) == 0x06F00070) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = ((inst >> 10) & 0x3) * 8;
        u32 rn = (inst >> 16) & 0xF;
        if (rn == 0xF) {
            out.opcode = Opcode::Uxth;
        } else {
            out.dp.rn = rn;
            out.opcode = Opcode::Uxtah;
        }
        return;
    }
    // SEL: cond 0110 1000 Rn Rd 1111 1011 Rm (0x06800FB0)
    if ((inst & 0x0FF00FF0) == 0x06800FB0) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = (inst >> 16) & 0xF;
        out.dp.rm = inst & 0xF;
        out.opcode = Opcode::Sel;
        return;
    }
    // SXTB16/SXTAB16: bits[27:20]=0x68, bits[9:4]=000111
    if ((inst & 0x0FF003F0) == 0x06800070) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = ((inst >> 10) & 0x3) * 8;
        u32 rn = (inst >> 16) & 0xF;
        if (rn == 0xF) {
            // SXTB16 - treat as SXTAB16 with Rn=0 (effectively same)
            out.dp.rn = 0xF;
            out.opcode = Opcode::Sxtab16;
        } else {
            out.dp.rn = rn;
            out.opcode = Opcode::Sxtab16;
        }
        return;
    }
    // UXTB16/UXTAB16: bits[27:20]=0x6C, bits[9:4]=000111
    if ((inst & 0x0FF003F0) == 0x06C00070) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = ((inst >> 10) & 0x3) * 8;
        u32 rn = (inst >> 16) & 0xF;
        if (rn == 0xF) {
            // UXTB16 - treat as UXTAB16 with Rn=0 (effectively same)
            out.dp.rn = 0xF;
            out.opcode = Opcode::Uxtab16;
        } else {
            out.dp.rn = rn;
            out.opcode = Opcode::Uxtab16;
        }
        return;
    }

    // SSAT: 0x06A00010
    if ((inst & 0x0FE00030) == 0x06A00010) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.imm = ((inst >> 16) & 0x1F) + 1;
        out.dp.shift_imm = (inst >> 7) & 0x1F;
        out.dp.shift_type = ((inst >> 6) & 1) ? ShiftType::ASR : ShiftType::LSL;
        out.opcode = Opcode::Ssat;
        return;
    }
    // USAT: 0x06E00010
    if ((inst & 0x0FE00030) == 0x06E00010) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.imm = (inst >> 16) & 0x1F;
        out.dp.shift_imm = (inst >> 7) & 0x1F;
        out.dp.shift_type = ((inst >> 6) & 1) ? ShiftType::ASR : ShiftType::LSL;
        out.opcode = Opcode::Usat;
        return;
    }
    // PKHBT: 0x06800010
    if ((inst & 0x0FF00070) == 0x06800010) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = (inst >> 16) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = (inst >> 7) & 0x1F;
        out.opcode = Opcode::Pkhbt;
        return;
    }
    // PKHTB: 0x06800050
    if ((inst & 0x0FF00070) == 0x06800050) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = (inst >> 16) & 0xF;
        out.dp.rm = inst & 0xF;
        out.dp.shift_imm = (inst >> 7) & 0x1F;
        out.opcode = Opcode::Pkhtb;
        return;
    }

    // Parallel add/sub: cond 0110 type Rn Rd 1111 op1 Rm
    // bits[27:24]=0110, bits[11:8]=1111, bit[4]=1
    if ((inst & 0x0F000F10) == 0x06000F10) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = (inst >> 16) & 0xF;
        out.dp.rm = inst & 0xF;
        u32 type = (inst >> 20) & 0xF;
        u32 op = (inst >> 5) & 7;
        switch (type) {
        case 1: // Signed normal
            switch (op) {
            case 0:
                out.opcode = Opcode::Sadd16;
                return;
            case 3:
                out.opcode = Opcode::Ssub16;
                return;
            case 4:
                out.opcode = Opcode::Sadd8;
                return;
            case 7:
                out.opcode = Opcode::Ssub8;
                return;
            }
            break;
        case 2: // Signed saturating (Q)
            switch (op) {
            case 0:
                out.opcode = Opcode::Qadd16;
                return;
            case 3:
                out.opcode = Opcode::Qsub16;
                return;
            case 4:
                out.opcode = Opcode::Qadd8;
                return;
            case 7:
                out.opcode = Opcode::Qsub8;
                return;
            }
            break;
        case 3: // Signed halving (SH)
            switch (op) {
            case 0:
                out.opcode = Opcode::Shadd16;
                return;
            case 3:
                out.opcode = Opcode::Shsub16;
                return;
            case 4:
                out.opcode = Opcode::Shadd8;
                return;
            case 7:
                out.opcode = Opcode::Shsub8;
                return;
            }
            break;
        case 5: // Unsigned normal
            switch (op) {
            case 0:
                out.opcode = Opcode::Uadd16;
                return;
            case 3:
                out.opcode = Opcode::Usub16;
                return;
            case 4:
                out.opcode = Opcode::Uadd8;
                return;
            case 7:
                out.opcode = Opcode::Usub8;
                return;
            }
            break;
        case 6: // Unsigned saturating (UQ)
            switch (op) {
            case 0:
                out.opcode = Opcode::Uqadd16;
                return;
            case 3:
                out.opcode = Opcode::Uqsub16;
                return;
            case 4:
                out.opcode = Opcode::Uqadd8;
                return;
            case 7:
                out.opcode = Opcode::Uqsub8;
                return;
            }
            break;
        case 7: // Unsigned halving (UH)
            switch (op) {
            case 0:
                out.opcode = Opcode::Uhadd16;
                return;
            case 3:
                out.opcode = Opcode::Uhsub16;
                return;
            case 4:
                out.opcode = Opcode::Uhadd8;
                return;
            case 7:
                out.opcode = Opcode::Uhsub8;
                return;
            }
            break;
        }
        // ASX/SAX variants (op 1,2,5,6) not yet implemented
    }

    // Bit field operations: bits[27:25]=011, bits[24]=1
    // BFC/BFI: cond 0111 110 msb Rd lsb 001 Rn
    if ((inst & 0x0FE00070) == 0x07C00010) {
        out.rd = (inst >> 12) & 0xF;
        u32 msb = (inst >> 16) & 0x1F;
        u32 lsb = (inst >> 7) & 0x1F;
        out.dp.imm = lsb;
        out.dp.shift_imm = msb - lsb + 1;
        u32 rn = inst & 0xF;
        if (rn == 0xF) {
            out.opcode = Opcode::Bfc;
        } else {
            out.dp.rn = rn;
            out.opcode = Opcode::Bfi;
        }
        return;
    }
    // SBFX: cond 0111 101 width-1 Rd lsb 101 Rn
    if ((inst & 0x0FE00070) == 0x07A00050) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = inst & 0xF;
        out.dp.imm = (inst >> 7) & 0x1F;              // lsb
        out.dp.shift_imm = ((inst >> 16) & 0x1F) + 1; // width
        out.opcode = Opcode::Sbfx;
        return;
    }
    // UBFX: cond 0111 111 width-1 Rd lsb 101 Rn
    if ((inst & 0x0FE00070) == 0x07E00050) {
        out.rd = (inst >> 12) & 0xF;
        out.dp.rn = inst & 0xF;
        out.dp.imm = (inst >> 7) & 0x1F;              // lsb
        out.dp.shift_imm = ((inst >> 16) & 0x1F) + 1; // width
        out.opcode = Opcode::Ubfx;
        return;
    }
    // USAD8/USADA8: cond 0111 1000 dddd aaaa mmmm 0001 nnnn
    // Rd=[19:16], Ra=[15:12], Rm=[11:8], Rn=[3:0]
    if ((inst & 0x0FF000F0) == 0x07800010) {
        out.rd = (inst >> 16) & 0xF;
        out.dp.rn = inst & 0xF;        // Rn (source 1)
        out.dp.rm = (inst >> 8) & 0xF; // Rm (source 2)
        u32 ra = (inst >> 12) & 0xF;
        if (ra == 0xF) {
            out.opcode = Opcode::Usad8;
        } else {
            out.dp.imm = ra; // Ra (accumulate register)
            out.opcode = Opcode::Usada8;
        }
        return;
    }

    // Standard Load/Store register (bit 4 must be 0)
    if (!(op2 & 1)) {
        bool load = (inst >> 20) & 1;
        bool byte = (inst >> 22) & 1;
        out.rd = (inst >> 12) & 0xF;
        out.ls.rn = (inst >> 16) & 0xF;
        out.ls.rm = inst & 0xF;
        out.ls.offset = 0;

        out.ls.shift_type = static_cast<ShiftType>((inst >> 5) & 0x3);
        out.ls.shift_imm = (inst >> 7) & 0x1F;

        bool u = (inst >> 23) & 1;
        bool p = (inst >> 24) & 1;
        bool w = (inst >> 21) & 1;

        out.ls.flags = u ? 0x01 : 0;

        if (p && !w)
            out.ls.mode = AddrMode::Offset;
        else if (p && w)
            out.ls.mode = AddrMode::PreIndex;
        else
            out.ls.mode = AddrMode::PostIndex;

        if (load) {
            out.opcode = byte ? Opcode::LdrB : Opcode::Ldr;
        } else {
            out.opcode = byte ? Opcode::StrB : Opcode::Str;
        }
        return;
    }

    out.opcode = Opcode::Invalid;
    return;
}

// Coprocessor space (op1 = 0b110/0b111): VFP transfers, load/store, data
// processing, and CP15
static void DecodeArmCoprocessor(u32 inst, DecodedInst& out) {
    // ARM<->VFP 64-bit register transfer (MCRR/MRRC to cp10/cp11): bits[27:21] = 1100010.
    // e.g. VMOV Dm,Rt,Rt2 (0xEC4..) and VMOV Rt,Rt2,Dm (0xEC5..). This encoding overlaps
    // the coprocessor load/store mask below, so it must be decoded first; otherwise it is
    // mis-decoded as VLDM/VSTM and performs a spurious memory access instead of the transfer.
    if ((inst & 0x0FE00000) == 0x0C400000) {
        const u32 cp = (inst >> 8) & 0xF;
        if (cp == 0xB) { // cp11: double-precision transfer (the common case)
            const bool from_coproc = (inst >> 20) & 1; // L bit: 1 = D->ARM, 0 = ARM->D
            out.vfp.rt = (inst >> 12) & 0xF;
            out.vfp.rt2 = (inst >> 16) & 0xF;
            out.vfp.sd = (((inst >> 5) & 1) << 4) | (inst & 0xF); // Dm = M:Vm
            out.opcode = from_coproc ? Opcode::VmovDToArm : Opcode::VmovArmToD;
            return;
        }
        // cp10 (two single-precision registers) is rare on 3DS; fall through unchanged.
    }

    // VFP Load/Store: bits[27:25] = 110, coprocessor 10 or 11
    if ((inst & 0x0E000E00) == 0x0C000A00) {
        bool P = (inst >> 24) & 1;
        bool W = (inst >> 21) & 1;

        if (P && !W) {
            // VLDR/VSTR (single transfer): P=1, W=0
            bool load = (inst >> 20) & 1;
            bool U = (inst >> 23) & 1;
            bool D = (inst >> 22) & 1;
            u32 Vd = ((inst >> 12) & 0xF);
            u32 Rn = (inst >> 16) & 0xF;
            s32 imm8 = inst & 0xFF;
            bool is_double = (inst & 0x100) != 0;

            if (is_double) {
                out.vfp.sd = (D << 4) | Vd;
            } else {
                out.vfp.sd = (Vd << 1) | D;
            }
            out.vfp.rn = Rn;
            out.vfp.offset = static_cast<s16>(U ? (imm8 * 4) : -(imm8 * 4));

            if (is_double) {
                out.opcode = load ? Opcode::VldrD : Opcode::VstrD;
            } else {
                out.opcode = load ? Opcode::VldrS : Opcode::VstrS;
            }
            return;
        }

        // VLDM/VSTM (multiple transfer): P=0,U=1 (IA) or P=1,U=0,W=1 (DB)
        {
            bool load = (inst >> 20) & 1;
            bool U = (inst >> 23) & 1;
            bool D = (inst >> 22) & 1;
            u32 Vd = ((inst >> 12) & 0xF);
            u32 Rn = (inst >> 16) & 0xF;
            u32 imm8 = inst & 0xFF;
            bool is_double = (inst & 0x100) != 0;

            if (is_double) {
                out.vfp.sd = (D << 4) | Vd;
                out.vfp.sn = imm8 / 2; // Number of double registers
            } else {
                out.vfp.sd = (Vd << 1) | D;
                out.vfp.sn = imm8; // Number of single registers
            }
            out.vfp.rn = Rn;
            // flags: bit 0 = W (writeback), bit 1 = DB mode (U=0)
            out.vfp.flags = (W ? 1 : 0) | ((!U) ? 2 : 0);

            if (is_double) {
                out.opcode = load ? Opcode::VldmD : Opcode::VstmD;
            } else {
                out.opcode = load ? Opcode::VldmS : Opcode::VstmS;
            }
            return;
        }
    }

    // VFP Data Processing and Register Transfer: bits[27:24] = 1110
    if ((inst & 0x0F000000) == 0x0E000000) {
        // Check coprocessor number (bits[11:8])
        u32 cp_num = (inst >> 8) & 0xF;
        if (cp_num == 0xF) {
            // CP15 system control coprocessor (MRC/MCR p15)
            bool bit4 = (inst >> 4) & 1;
            if (bit4) {
                bool L = (inst >> 20) & 1;
                out.opcode = L ? Opcode::Mrc : Opcode::Mcr;
                out.rd = (inst >> 12) & 0xF;         // Rt
                out.dp.rn = (inst >> 16) & 0xF;      // CRn
                out.dp.rm = inst & 0xF;              // CRm
                out.dp.shift_imm = (inst >> 21) & 7; // opc1
                out.dp.imm = (inst >> 5) & 7;        // opc2
            } else {
                out.opcode = Opcode::Invalid;
            }
            return;
        }
        if (cp_num != 0xA && cp_num != 0xB) {
            // Unknown coprocessor
            out.opcode = Opcode::Invalid;
            return;
        }

        bool is_double = (cp_num == 0xB);
        bool bit4 = (inst >> 4) & 1;

        // VMOV/VMRS/VMSR: Register transfer (bit 4 = 1, and specific patterns)
        if (bit4) {
            u32 opc1 = (inst >> 21) & 0x7;
            u32 L = (inst >> 20) & 1;

            // VMRS: cond 1110 1111 0001 Rt 1010 0001 0000 (L=1, opc1=7)
            if (opc1 == 0x7 && L == 1) {
                out.vfp.rt = (inst >> 12) & 0xF;
                out.opcode = Opcode::Vmrs;
                return;
            }

            // VMSR: cond 1110 1110 0001 Rt 1010 0001 0000 (L=0, opc1=7)
            if (opc1 == 0x7 && L == 0) {
                out.vfp.rt = (inst >> 12) & 0xF;
                out.opcode = Opcode::Vmsr;
                return;
            }

            // VMOV ARM to/from single VFP: opc1=0, specific encoding
            if (opc1 == 0 && !is_double) {
                u32 Vn = (inst >> 16) & 0xF;
                u32 N = (inst >> 7) & 1;
                out.vfp.sd = (Vn << 1) | N;
                out.vfp.rt = (inst >> 12) & 0xF;

                if (L) {
                    out.opcode = Opcode::VmovSToArm;
                } else {
                    out.opcode = Opcode::VmovArmToS;
                }
                return;
            }
        }

        // VFP Data Processing (bit 4 = 0 or specific bit4=1 patterns for unary ops)
        u32 opc1 = (inst >> 20) & 0xF;
        u32 opc2 = (inst >> 6) & 0x3;
        u32 opc3 = (inst >> 16) & 0xF;

        // Extract VFP register fields
        u32 D = (inst >> 22) & 1;
        u32 Vn = (inst >> 16) & 0xF;
        u32 N = (inst >> 7) & 1;
        u32 Vd = (inst >> 12) & 0xF;
        u32 M = (inst >> 5) & 1;
        u32 Vm = inst & 0xF;

        if (is_double) {
            out.vfp.sd = (D << 4) | Vd;
            out.vfp.sn = (N << 4) | Vn;
            out.vfp.sm = (M << 4) | Vm;
        } else {
            out.vfp.sd = (Vd << 1) | D;
            out.vfp.sn = (Vn << 1) | N;
            out.vfp.sm = (Vm << 1) | M;
        }

        // Decode based on opc1 (bits[23:20])
        switch (opc1 & 0xB) { // Mask out bit 2 (the 'N' bit position sometimes)
        case 0x0:             // VMLA, VMLS
            if (is_double) {
                out.opcode = (opc2 & 1) ? Opcode::VmlsD : Opcode::VmlaD;
            } else {
                out.opcode = (opc2 & 1) ? Opcode::VmlsS : Opcode::VmlaS;
            }
            return;
        case 0x1: // VNMLA, VNMLS
            if (is_double) {
                out.opcode = (opc2 & 1) ? Opcode::VnmlaD : Opcode::VnmlsD;
            } else {
                out.opcode = (opc2 & 1) ? Opcode::VnmlaS : Opcode::VnmlsS;
            }
            return;
        case 0x2: // VMUL, VNMUL
            if (is_double) {
                out.opcode = (opc2 & 1) ? Opcode::VnmulD : Opcode::VmulD;
            } else {
                out.opcode = (opc2 & 1) ? Opcode::VnmulS : Opcode::VmulS;
            }
            return;
        case 0x3: // VADD, VSUB
            if (is_double) {
                out.opcode = (opc2 & 1) ? Opcode::VsubD : Opcode::VaddD;
            } else {
                out.opcode = (opc2 & 1) ? Opcode::VsubS : Opcode::VaddS;
            }
            return;
        case 0x8: // VDIV
            if (is_double) {
                out.opcode = Opcode::VdivD;
            } else {
                out.opcode = Opcode::VdivS;
            }
            return;
        case 0xB: // VMOV, VABS, VNEG, VSQRT, VCMP, VCVT and other extension ops
            // These use opc3 (bits[19:16]) and opc2 (bits[7:6]) to differentiate
            if (opc2 == 0x0) {
                // VMOV immediate: cond 1110 1D11 imm4H Vd 101p 0000 imm4L
                // opc2=00 uniquely identifies this in the extension block
                // Expand the modified immediate (imm8 = abcdefgh) at decode:
                //   F32 = a:NOT(b):bbbbb:cdefgh:zeros(19)
                //   F64 = a:NOT(b):b(x8):cdefgh:zeros(48)
                const u32 imm8 = (opc3 << 4) | (inst & 0xF);
                const u32 a = (imm8 >> 7) & 1;
                const u32 b = (imm8 >> 6) & 1;
                const u32 cdefgh = imm8 & 0x3F;
                out.vimm.sd = out.vfp.sd; // Same byte slot; kept for clarity
                if (is_double) {
                    const u64 bits =
                        (static_cast<u64>(a) << 63) | (static_cast<u64>(b ? 0u : 1u) << 62) |
                        (static_cast<u64>(b ? 0xFFu : 0u) << 54) | (static_cast<u64>(cdefgh) << 48);
                    out.vimm.lo = static_cast<u32>(bits);
                    out.vimm.hi = static_cast<u32>(bits >> 32);
                    out.opcode = Opcode::VmovImmD;
                } else {
                    out.vimm.lo = (a << 31) | ((b ? 0u : 1u) << 30) | ((b ? 0x1Fu : 0u) << 25) |
                                  (cdefgh << 19);
                    out.vimm.hi = 0;
                    out.opcode = Opcode::VmovImmS;
                }
                return;
            }
            if (opc3 == 0x0 && opc2 == 0x1) {
                // VMOV register
                if (is_double) {
                    out.opcode = Opcode::VmovD;
                } else {
                    out.opcode = Opcode::VmovS;
                }
                return;
            }
            if (opc3 == 0x0 && opc2 == 0x3) {
                // VABS
                if (is_double) {
                    out.opcode = Opcode::VabsD;
                } else {
                    out.opcode = Opcode::VabsS;
                }
                return;
            }
            if (opc3 == 0x1 && opc2 == 0x1) {
                // VNEG
                if (is_double) {
                    out.opcode = Opcode::VnegD;
                } else {
                    out.opcode = Opcode::VnegS;
                }
                return;
            }
            if (opc3 == 0x1 && opc2 == 0x3) {
                // VSQRT
                if (is_double) {
                    out.opcode = Opcode::VsqrtD;
                } else {
                    out.opcode = Opcode::VsqrtS;
                }
                return;
            }
            if (opc3 == 0x4 && (opc2 & 1)) {
                // VCMP
                if (is_double) {
                    out.opcode = Opcode::VcmpD;
                } else {
                    out.opcode = Opcode::VcmpS;
                }
                return;
            }
            if (opc3 == 0x5 && (opc2 & 1)) {
                // VCMPZ (compare with zero)
                if (is_double) {
                    out.opcode = Opcode::VcmpzD;
                } else {
                    out.opcode = Opcode::VcmpzS;
                }
                return;
            }
            // VCVT between float and int
            // ARM encoding: bit[7]=0 -> unsigned, bit[7]=1 -> signed
            if (opc3 == 0x8) {
                // VCVT int to float. The integer operand always lives in a
                // single-precision register; only the float dest may be double,
                // so the source (sm) must use single numbering regardless.
                out.vfp.flags = (opc2 & 2) ? 0 : 1; // bit[7]=1->signed(0), bit[7]=0->unsigned(1)
                out.vfp.sm = (Vm << 1) | M;         // Sm (int, always single)
                if (is_double) {
                    out.vfp.sd = (D << 4) | Vd; // Dd (double)
                    out.opcode = Opcode::VcvtIntToD;
                } else {
                    out.opcode = Opcode::VcvtIntToS; // both single (generic decode ok)
                }
                return;
            }
            if (opc3 == 0xC || opc3 == 0xD) {
                // VCVT float to int. The integer result always lands in a
                // single-precision register; only the float source may be double,
                // so the dest (sd) must use single numbering regardless.
                // opc3[0] (bit[16]): 0=unsigned(0xC), 1=signed(0xD)
                // bit[7] is rounding mode, not signed/unsigned
                out.vfp.flags = ((opc3 & 1) ? 0 : 1) | 2; // bit 0 = 1 if unsigned, bit 1 = to_int
                out.vfp.sd = (Vd << 1) | D;               // Sd (int, always single)
                if (is_double) {
                    out.vfp.sm = (M << 4) | Vm; // Dm (double)
                    out.opcode = Opcode::VcvtDToInt;
                } else {
                    out.opcode = Opcode::VcvtSToInt; // both single (generic decode ok)
                }
                return;
            }
            // VCVT between single and double precision. Mixed-precision op: the cp
            // field (bits[11:8]) gives the source float precision. 0xB (is_double) is
            // VCVT.F32.F64 (double->single), 0xA is VCVT.F64.F32 (single->double). The
            // generic decode above assumed uniform precision, so recompute both operand
            // registers with the correct per-operand numbering here.
            if (opc3 == 0x7 && opc2 == 0x3) {
                if (is_double) {
                    // VCVT.F32.F64 Sd, Dm: dest single, source double
                    out.vfp.sd = (Vd << 1) | D;
                    out.vfp.sm = (M << 4) | Vm;
                    out.opcode = Opcode::VcvtDToS;
                } else {
                    // VCVT.F64.F32 Dd, Sm: dest double, source single
                    out.vfp.sd = (D << 4) | Vd;
                    out.vfp.sm = (Vm << 1) | M;
                    out.opcode = Opcode::VcvtSToD;
                }
                return;
            }
            break;
        }
    }

    // Default: unrecognized instruction
    out.opcode = Opcode::Invalid;
}

void ARM_FastInterp::DecodeARM(u32 inst, u32 pc, DecodedInst& out) {
    out.cond = (inst >> 28) & 0xF;
    out.inst_size = 4;

    // Unconditional (cond=0xF) space. Only BLX immediate (handled in the branch
    // case below) and the hint/sync group decode here; nothing else may fall
    // through to the conditional decoders, since CondPassed treats cond=0xF as
    // always-pass (e.g. PLD would execute as an LDRB with Rd=PC).
    if (out.cond == 0xF && ((inst >> 25) & 0x7) != 5) {
        out.cond = 0xE;
        out.rd = 0;
        if ((inst & 0x0FFFFF00) == 0x057FF000) {
            // CLREX (0xF57FF01F) clears the monitor; DSB/DMB/ISB
            // (0xF57FF04x/5x/6x) have nothing to barrier here
            out.opcode = (((inst >> 4) & 0xF) == 1) ? Opcode::Clrex : Opcode::Nop;
            return;
        }
        if ((inst & 0x0D70F000) == 0x0550F000) {
            out.opcode = Opcode::Nop; // PLD (immediate or register): a hint
            return;
        }
        out.opcode = Opcode::Invalid;
        return;
    }

    // Dispatch on op1 (bits[27:25]), cases in encoding order
    switch ((inst >> 25) & 0x7) {

    case 0:
        DecodeArmDataProcRegOrMisc(inst, out);
        return;

    case 1:
        DecodeArmDataProcImm(inst, out);
        return;

    case 2: {
        // Load/Store word/byte immediate - no special cases, direct decode
        bool load = (inst >> 20) & 1;
        bool byte = (inst >> 22) & 1;
        out.rd = (inst >> 12) & 0xF;
        out.ls.rn = (inst >> 16) & 0xF;
        out.ls.rm = 0xFF;
        out.ls.offset = inst & 0xFFF;

        bool u = (inst >> 23) & 1;
        bool p = (inst >> 24) & 1;
        bool w = (inst >> 21) & 1;

        if (!u)
            out.ls.offset = -out.ls.offset;

        if (p && !w)
            out.ls.mode = AddrMode::Offset;
        else if (p && w)
            out.ls.mode = AddrMode::PreIndex;
        else
            out.ls.mode = AddrMode::PostIndex;

        out.ls.shift_type = ShiftType::LSL;
        out.ls.shift_imm = 0;
        out.ls.flags = 0;

        // Decode-time specialization: plain imm-offset (no writeback) with
        // rd/rn != 15 takes dedicated handlers with no runtime mode checks
        if (out.ls.mode == AddrMode::Offset && out.rd != 15 && out.ls.rn != 15) {
            if (load) {
                out.opcode = byte ? Opcode::LdrBImm : Opcode::LdrImm;
            } else {
                out.opcode = byte ? Opcode::StrBImm : Opcode::StrImm;
            }
            return;
        }

        if (load) {
            out.opcode = byte ? Opcode::LdrB : Opcode::Ldr;
        } else {
            out.opcode = byte ? Opcode::StrB : Opcode::Str;
        }
        return;
    }

    case 3:
        DecodeArmMediaOrLoadStoreReg(inst, out);
        return;

    case 4: {
        // Load/Store Multiple - no special cases, direct decode
        bool load = (inst >> 20) & 1;
        out.lsm.rn = (inst >> 16) & 0xF;
        out.lsm.reg_list = inst & 0xFFFF;
        out.lsm.flags = ((inst >> 21) & 0x1F);
        // Precompute count, first-access offset, and writeback delta so the
        // handlers skip popcount and the increment/before flag tests
        {
            const u32 count = static_cast<u32>(std::popcount(out.lsm.reg_list));
            const bool increment = (out.lsm.flags & 0x04) != 0; // U bit
            const bool before = (out.lsm.flags & 0x08) != 0;    // P bit
            s32 base_off = increment ? 0 : -static_cast<s32>(count * 4);
            // Add 4 when (increment == before): STMIB/LDMIB, STMDA/LDMDA
            if (increment == before)
                base_off += 4;
            out.lsm.count = static_cast<u8>(count);
            out.lsm.base_off = static_cast<s8>(base_off);
            out.lsm.wb_delta = static_cast<s16>(increment ? static_cast<s32>(count * 4)
                                                          : -static_cast<s32>(count * 4));
        }
        out.rd = 0;
        out.opcode = load ? Opcode::Ldm : Opcode::Stm;
        return;
    }

    case 5: {
        // Branch (B/BL/BLX immediate)
        // Targets and LR are decode-time constants (instruction PC is fixed)
        // When cond=0xF (unconditional), this is BLX immediate (ARM->Thumb)
        if (out.cond == 0xF) {
            // BLX immediate: 1111 101H offset24
            // H bit (bit 24) adds 2 to the offset for halfword alignment
            s32 offset = static_cast<s32>(inst << 8) >> 6;
            if ((inst >> 24) & 1)
                offset |= 2; // H bit
            out.branch.target = pc + 8 + offset;
            out.branch.lr = pc + 4;
            out.branch.rm = 1; // Destination mode: Thumb
            out.rd = 0;
            out.opcode = Opcode::BlxImm;
            out.cond = 0xE; // Make unconditional (AL) for CHECK_COND
            return;
        }
        bool link = (inst >> 24) & 1;
        s32 offset = static_cast<s32>(inst << 8) >> 6;
        out.branch.target = pc + 8 + offset;
        out.branch.lr = pc + 4;
        out.branch.rm = 0xFF;
        out.rd = 0;
        out.opcode = link ? Opcode::Bl : Opcode::B;
        return;
    }

    case 6:
        DecodeArmCoprocessor(inst, out);
        return;

    case 7:
        // SWI/SVC (bits[27:24] = 0b1111), otherwise coprocessor
        if ((inst >> 24) & 1) {
            out.opcode = Opcode::Swi;
            out.imm32 = inst & 0x00FFFFFF;
            return;
        }
        DecodeArmCoprocessor(inst, out);
        return;
    }
}

void ARM_FastInterp::DecodeThumb16(u16 inst, u32 pc, DecodedInst& out) {
    out.inst_size = 2;
    out.cond = 0xE; // Most Thumb instructions are unconditional

    // ========================================================================
    // Shift by immediate (format: 000 op imm5 Rm Rd)
    // Add/Sub register (format: 000110x Rm Rn Rd)
    // Add/Sub 3-bit immediate (format: 000111x imm3 Rn Rd)
    // ========================================================================
    if ((inst & 0xE000) == 0x0000) {
        if ((inst & 0x1800) == 0x0000) {
            out.opcode = Opcode::ThumbLslImm;
            out.rd = inst & 7;
            out.dp.rm = (inst >> 3) & 7;
            out.dp.shift_imm = (inst >> 6) & 0x1F;
        } else if ((inst & 0x1800) == 0x0800) {
            out.opcode = Opcode::ThumbLsrImm;
            out.rd = inst & 7;
            out.dp.rm = (inst >> 3) & 7;
            out.dp.shift_imm = (inst >> 6) & 0x1F;
        } else if ((inst & 0x1800) == 0x1000) {
            out.opcode = Opcode::ThumbAsrImm;
            out.rd = inst & 7;
            out.dp.rm = (inst >> 3) & 7;
            out.dp.shift_imm = (inst >> 6) & 0x1F;
        } else if ((inst & 0x1E00) == 0x1800) {
            out.opcode = Opcode::ThumbAddReg3;
            out.rd = inst & 7;
            out.dp.rn = (inst >> 3) & 7;
            out.dp.rm = (inst >> 6) & 7;
        } else if ((inst & 0x1E00) == 0x1A00) {
            out.opcode = Opcode::ThumbSubReg3;
            out.rd = inst & 7;
            out.dp.rn = (inst >> 3) & 7;
            out.dp.rm = (inst >> 6) & 7;
        } else if ((inst & 0x1E00) == 0x1C00) {
            out.opcode = Opcode::ThumbAddImm3;
            out.rd = inst & 7;
            out.dp.rn = (inst >> 3) & 7;
            out.dp.imm = (inst >> 6) & 7;
        } else if ((inst & 0x1E00) == 0x1E00) {
            out.opcode = Opcode::ThumbSubImm3;
            out.rd = inst & 7;
            out.dp.rn = (inst >> 3) & 7;
            out.dp.imm = (inst >> 6) & 7;
        } else {
            out.opcode = Opcode::Invalid;
        }
        return;
    }

    // ========================================================================
    // MOV/CMP/ADD/SUB with 8-bit immediate (format: 001 op Rd imm8)
    // ========================================================================
    if ((inst & 0xE000) == 0x2000) {
        out.rd = (inst >> 8) & 7;
        out.dp.imm = inst & 0xFF;
        const u8 op = (inst >> 11) & 3;
        switch (op) {
        case 0:
            out.opcode = Opcode::ThumbMovImm8;
            break;
        case 1:
            out.opcode = Opcode::ThumbCmpImm8;
            break;
        case 2:
            out.opcode = Opcode::ThumbAddImm8;
            break;
        case 3:
            out.opcode = Opcode::ThumbSubImm8;
            break;
        }
        return;
    }

    // ========================================================================
    // Data processing register (format: 010000 op Rm Rd)
    // ========================================================================
    if ((inst & 0xFC00) == 0x4000) {
        out.rd = inst & 7;
        out.dp.rm = (inst >> 3) & 7;
        const u8 op = (inst >> 6) & 0xF;
        static constexpr Opcode dp_table[16] = {
            Opcode::ThumbAnd,    Opcode::ThumbEor, Opcode::ThumbLslReg, Opcode::ThumbLsrReg,
            Opcode::ThumbAsrReg, Opcode::ThumbAdc, Opcode::ThumbSbc,    Opcode::ThumbRor,
            Opcode::ThumbTst,    Opcode::ThumbNeg, Opcode::ThumbCmpReg, Opcode::ThumbCmn,
            Opcode::ThumbOrr,    Opcode::ThumbMul, Opcode::ThumbBic,    Opcode::ThumbMvn,
        };
        out.opcode = dp_table[op];
        return;
    }

    // ========================================================================
    // Special data processing (high registers, BX, BLX)
    // ========================================================================
    if ((inst & 0xFC00) == 0x4400) {
        out.rd = (inst & 7) | ((inst >> 4) & 8);
        out.dp.rm = (inst >> 3) & 0xF;
        const u8 op = (inst >> 8) & 3;
        switch (op) {
        case 0:
            out.opcode = Opcode::ThumbAddHi;
            // ADD Rd, PC (rd != 15): the PC value (PC+4) is a constant - fold
            // to the ARM Add handler as Rd = Rd + #imm
            if (out.dp.rm == 15 && out.rd != 15) {
                out.opcode = Opcode::Add;
                out.dp.rn = out.rd;
                out.dp.rm = 0xFF;
                out.dp.rs = 0xFF;
                out.dp.imm = pc + 4;
                out.dp.flags = 0;
            }
            break;
        case 1:
            out.opcode = Opcode::ThumbCmpHi;
            // CMP Rn, PC: fold to the ARM Cmp handler as CMP Rn, #(PC+4)
            if (out.dp.rm == 15 && out.rd != 15) {
                out.opcode = Opcode::Cmp;
                out.dp.rn = out.rd;
                out.dp.rm = 0xFF;
                out.dp.rs = 0xFF;
                out.dp.imm = pc + 4;
                out.dp.flags = 0;
            }
            break;
        case 2:
            out.opcode = Opcode::ThumbMovHi;
            // MOV Rd, PC (rd != 15): fold to the ARM Mov handler, #(PC+4)
            // (rd==15 must stay ThumbMovHi: the ARM rd==15 path would switch
            // to ARM mode, but Thumb MOV PC, Rm stays in Thumb)
            if (out.dp.rm == 15 && out.rd != 15) {
                out.opcode = Opcode::Mov;
                out.dp.rn = 0;
                out.dp.rm = 0xFF;
                out.dp.rs = 0xFF;
                out.dp.imm = pc + 4;
                out.dp.flags = 0;
            }
            break;
        case 3:
            out.opcode = (inst & 0x80) ? Opcode::ThumbBlxReg : Opcode::ThumbBx;
            break;
        }
        return;
    }

    // ========================================================================
    // Load from literal pool (LDR Rd, [PC, #imm8*4])
    // ========================================================================
    if ((inst & 0xF800) == 0x4800) {
        out.opcode = Opcode::ThumbLdrPc;
        out.rd = (inst >> 8) & 7;
        // Absolute literal-pool address: Align(PC+4, 4) + imm8*4 (constant)
        out.dp.imm = ((pc + 4) & ~3u) + (inst & 0xFF) * 4;
        return;
    }

    // ========================================================================
    // Load/Store register offset (format: 0101 opc Rm Rn Rd)
    // ========================================================================
    if ((inst & 0xF000) == 0x5000) {
        out.rd = inst & 7;
        out.dp.rn = (inst >> 3) & 7;
        out.dp.rm = (inst >> 6) & 7;
        const u8 opc = (inst >> 9) & 7;
        static constexpr Opcode ls_reg_table[8] = {
            Opcode::ThumbStrReg, Opcode::ThumbStrhReg, Opcode::ThumbStrbReg, Opcode::ThumbLdrsb,
            Opcode::ThumbLdrReg, Opcode::ThumbLdrhReg, Opcode::ThumbLdrbReg, Opcode::ThumbLdrsh,
        };
        out.opcode = ls_reg_table[opc];
        return;
    }

    // ========================================================================
    // Load/Store word/byte immediate offset (format: 011 B L imm5 Rn Rd)
    // ========================================================================
    if ((inst & 0xE000) == 0x6000) {
        out.rd = inst & 7;
        out.dp.rn = (inst >> 3) & 7;
        const bool is_byte = (inst & 0x1000) != 0;
        const bool is_load = (inst & 0x0800) != 0;
        // Word offset is imm5*4, byte offset is imm5
        out.dp.imm = is_byte ? ((inst >> 6) & 0x1F) : (((inst >> 6) & 0x1F) * 4);
        if (is_load) {
            out.opcode = is_byte ? Opcode::ThumbLdrbImm5 : Opcode::ThumbLdrImm5;
        } else {
            out.opcode = is_byte ? Opcode::ThumbStrbImm5 : Opcode::ThumbStrImm5;
        }
        return;
    }

    // ========================================================================
    // Load/Store halfword immediate offset (format: 1000 L imm5 Rn Rd)
    // ========================================================================
    if ((inst & 0xF000) == 0x8000) {
        out.rd = inst & 7;
        out.dp.rn = (inst >> 3) & 7;
        out.dp.imm = ((inst >> 6) & 0x1F) * 2;
        out.opcode = (inst & 0x0800) ? Opcode::ThumbLdrhImm5 : Opcode::ThumbStrhImm5;
        return;
    }

    // ========================================================================
    // Load/Store SP-relative (format: 1001 L Rd imm8)
    // ========================================================================
    if ((inst & 0xF000) == 0x9000) {
        out.rd = (inst >> 8) & 7;
        out.dp.imm = (inst & 0xFF) * 4;
        out.opcode = (inst & 0x0800) ? Opcode::ThumbLdrSp : Opcode::ThumbStrSp;
        return;
    }

    // ========================================================================
    // Add to PC or SP (format: 1010 S Rd imm8)
    // ========================================================================
    if ((inst & 0xF000) == 0xA000) {
        out.rd = (inst >> 8) & 7;
        if (inst & 0x0800) {
            out.dp.imm = (inst & 0xFF) * 4;
            out.opcode = Opcode::ThumbAddSpImm;
        } else {
            // ADR: the value Align(PC+4, 4) + imm8*4 is a constant
            out.dp.imm = ((pc + 4) & ~3u) + (inst & 0xFF) * 4;
            out.opcode = Opcode::ThumbAddPcImm;
        }
        return;
    }

    // ========================================================================
    // Miscellaneous (PUSH, POP, adjust SP, sign/zero extend, etc.)
    // ========================================================================
    if ((inst & 0xF000) == 0xB000) {
        if ((inst & 0xFF80) == 0xB000) {
            out.opcode = Opcode::ThumbAddSpImm7;
            out.dp.imm = (inst & 0x7F) * 4;
            return;
        }
        if ((inst & 0xFF80) == 0xB080) {
            out.opcode = Opcode::ThumbSubSpImm7;
            out.dp.imm = (inst & 0x7F) * 4;
            return;
        }
        if ((inst & 0xFE00) == 0xB400) {
            out.opcode = Opcode::ThumbPush;
            out.lsm.reg_list = (inst & 0xFF) | ((inst & 0x100) ? (1 << 14) : 0);
            const u32 count = static_cast<u32>(std::popcount(out.lsm.reg_list));
            out.lsm.count = static_cast<u8>(count);
            out.lsm.base_off = static_cast<s8>(-static_cast<s32>(count * 4));
            out.lsm.wb_delta = static_cast<s16>(-static_cast<s32>(count * 4));
            return;
        }
        if ((inst & 0xFE00) == 0xBC00) {
            out.opcode = Opcode::ThumbPop;
            out.lsm.reg_list = (inst & 0xFF) | ((inst & 0x100) ? (1 << 15) : 0);
            const u32 count = static_cast<u32>(std::popcount(out.lsm.reg_list));
            out.lsm.count = static_cast<u8>(count);
            out.lsm.base_off = 0;
            out.lsm.wb_delta = static_cast<s16>(count * 4);
            return;
        }
        // Sign/Zero extend
        if ((inst & 0xFF00) == 0xB200) {
            out.rd = inst & 7;
            out.dp.rm = (inst >> 3) & 7;
            const u8 op = (inst >> 6) & 3;
            static constexpr Opcode ext_table[4] = {
                Opcode::ThumbSxth,
                Opcode::ThumbSxtb,
                Opcode::ThumbUxth,
                Opcode::ThumbUxtb,
            };
            out.opcode = ext_table[op];
            return;
        }
        // Byte reversal
        if ((inst & 0xFF00) == 0xBA00) {
            out.rd = inst & 7;
            out.dp.rm = (inst >> 3) & 7;
            const u8 op = (inst >> 6) & 3;
            if (op == 0)
                out.opcode = Opcode::ThumbRev;
            else if (op == 1)
                out.opcode = Opcode::ThumbRev16;
            else if (op == 3)
                out.opcode = Opcode::ThumbRevsh;
            else
                out.opcode = Opcode::Invalid;
            return;
        }
        out.opcode = Opcode::Invalid;
        return;
    }

    // ========================================================================
    // Load/Store Multiple (format: 1100 L Rn reglist)
    // ========================================================================
    if ((inst & 0xF000) == 0xC000) {
        out.lsm.rn = (inst >> 8) & 7;
        out.lsm.reg_list = inst & 0xFF;
        const u32 count = static_cast<u32>(std::popcount(out.lsm.reg_list));
        out.lsm.count = static_cast<u8>(count);
        out.lsm.base_off = 0;
        out.lsm.wb_delta = static_cast<s16>(count * 4);
        out.opcode = (inst & 0x0800) ? Opcode::ThumbLdmia : Opcode::ThumbStmia;
        return;
    }

    // ========================================================================
    // Conditional branch (format: 1101 cond imm8)
    // ========================================================================
    if ((inst & 0xF000) == 0xD000) {
        const u8 cond = (inst >> 8) & 0xF;
        if (cond == 0xE) {
            out.opcode = Opcode::Invalid; // UDF
            return;
        }
        if (cond == 0xF) {
            out.opcode = Opcode::ThumbSwi;
            out.imm32 = inst & 0xFF;
            return;
        }
        out.opcode = Opcode::ThumbBCond;
        out.cond = cond;
        s8 imm8 = static_cast<s8>(inst & 0xFF);
        out.branch.target = pc + 4 + imm8 * 2; // Absolute target (constant)
        return;
    }

    // ========================================================================
    // Unconditional branch (format: 11100 imm11)
    // ========================================================================
    if ((inst & 0xF800) == 0xE000) {
        out.opcode = Opcode::ThumbB;
        s32 imm11 = inst & 0x7FF;
        if (imm11 & 0x400)
            imm11 |= 0xFFFFF800;                // Sign extend
        out.branch.target = pc + 4 + imm11 * 2; // Absolute target (constant)
        return;
    }

    out.opcode = Opcode::Invalid;
}

// Thumb32 data processing, modified immediate (hw1 = 11110 i 0 oooo S Rn,
// hw2 = 0 imm3 Rd imm8). The branches sharing this hw1 space (hw2 bit15=1)
// are decoded before this is tried.
static bool TryDecodeThumb32DataProcModImm(u16 hw1, u16 hw2, DecodedInst& out) {
    if ((hw1 & 0xFA00) != 0xF000 || (hw2 & 0x8000) != 0) {
        return false;
    }
    // Thumb32 modified immediate data processing
    // hw1: 1111 0 i 0 oooo S Rn
    // hw2: 0 imm3 Rd imm8
    const u8 op_raw = (hw1 >> 5) & 0xF; // bits [8:5]
    const bool S = (hw1 >> 4) & 1;
    const u8 Rn = hw1 & 0xF;
    const u8 Rd = (hw2 >> 8) & 0xF;
    const bool i = (hw1 >> 10) & 1;
    const u8 imm3 = (hw2 >> 12) & 0x7;
    const u8 imm8 = hw2 & 0xFF;

    // Reconstruct the 12-bit modified immediate: i:imm3:imm8
    const u16 thumb_imm12 = (i << 11) | (imm3 << 8) | imm8;

    // Thumb-2 modified immediate uses a different encoding than ARM rotated immediate.
    // Thumb ThumbExpandImm: if imm12[11:8] == 0, result = imm12[7:0]
    //   else if imm12[11:8] == 1, result = imm8:00000000:imm8:00000000
    //   else ... (ROR-based encoding)

    // Compute ThumbExpandImm value
    u32 imm_val;
    bool carry_out_valid = false;
    const u8 top4 = (thumb_imm12 >> 8) & 0xF;
    if (top4 == 0) {
        imm_val = imm8;
    } else if (top4 == 1) {
        imm_val = (imm8 << 16) | imm8;
    } else if (top4 == 2) {
        imm_val = (imm8 << 24) | (imm8 << 8);
    } else if (top4 == 3) {
        imm_val = (imm8 << 24) | (imm8 << 16) | (imm8 << 8) | imm8;
    } else {
        // ROR-based encoding: value = ROR(1:imm12[6:0], imm12[11:7])
        u32 unrot = 0x80 | (thumb_imm12 & 0x7F);
        u32 ror_amount = (thumb_imm12 >> 7) & 0x1F;
        imm_val = (unrot >> ror_amount) | (unrot << (32 - ror_amount));
        carry_out_valid = true;
    }

    // Map Thumb32 opcode to ARM opcode
    // Thumb32 opcodes [8:5] map to ARM data processing opcodes
    // 0000=AND, 0001=BIC, 0010=ORR/MOV, 0011=ORN/MVN
    // 0100=EOR/TEQ, 1000=ADD/CMN, 1010=ADC, 1011=SBC
    // 1101=SUB/CMP, 1110=RSB
    static const u8 thumb_to_arm_op[] = {
        0x0, // 0000: AND -> AND (0x0)
        0xE, // 0001: BIC -> BIC (0xE)
        0xC, // 0010: ORR (0xC); Rn==15 is special-cased to MOV (0xD) below
        0xF, // 0011: ORN/MVN -> MVN (0xF) for Rn=15, else need special handling
        0x1, // 0100: EOR/TEQ -> EOR (0x1)
        0x0, // 0101: undefined
        0x0, // 0110: undefined (PKHBT/PKHTB in shifted reg)
        0x0, // 0111: undefined
        0x4, // 1000: ADD/CMN -> ADD (0x4)
        0x0, // 1001: undefined
        0x5, // 1010: ADC -> ADC (0x5)
        0x6, // 1011: SBC -> SBC (0x6)
        0x0, // 1100: undefined
        0x2, // 1101: SUB/CMP -> SUB (0x2)
        0x3, // 1110: RSB -> RSB (0x3)
        0x0, // 1111: undefined
    };

    u8 arm_op = thumb_to_arm_op[op_raw];

    // Special handling for ORR with Rn=15 -> MOV
    if (op_raw == 0x2 && Rn == 0xF) {
        arm_op = 0xD; // MOV
    }
    // ORN with Rn=15 -> MVN
    if (op_raw == 0x3 && Rn == 0xF) {
        arm_op = 0xF; // MVN
    }
    // ORN (Rn != 15) - ARM has no ORN, compute NOT(imm) and use ORR.
    // The carry-out still comes from the pre-inversion ThumbExpandImm_C.
    bool carry_inverted = false;
    if (op_raw == 0x3 && Rn != 0xF) {
        imm_val = ~imm_val;
        arm_op = 0xC;
        carry_inverted = carry_out_valid;
    }

    // Now use our existing DP decoder directly instead of building ARM instruction
    // (since ThumbExpandImm doesn't map cleanly to ARM rotate_imm:imm8)
    out.opcode = Opcode::Invalid; // Will be set by logic below
    out.rd = Rd;
    out.cond = 0xE;
    out.dp.rn = Rn;
    out.dp.rm = 0xFF; // Immediate
    out.dp.rs = 0xFF;
    out.dp.shift_type = ShiftType::LSL;
    out.dp.shift_imm = 0;
    out.dp.imm = imm_val;
    // bit 0: carry-out = imm bit 31; bit 1: complemented (ORN)
    out.dp.flags = (carry_out_valid ? 1 : 0) | (carry_inverted ? 2 : 0);

    // Map to opcode
    switch (arm_op) {
    case 0x0:
        out.opcode = S ? Opcode::AndS : Opcode::And;
        break;
    case 0x1:
        out.opcode = S ? Opcode::EorS : Opcode::Eor;
        break;
    case 0x2:
        out.opcode = S ? Opcode::SubS : Opcode::Sub;
        break;
    case 0x3:
        out.opcode = S ? Opcode::RsbS : Opcode::Rsb;
        break;
    case 0x4:
        out.opcode = S ? Opcode::AddS : Opcode::Add;
        break;
    case 0x5:
        out.opcode = S ? Opcode::AdcS : Opcode::Adc;
        break;
    case 0x6:
        out.opcode = S ? Opcode::SbcS : Opcode::Sbc;
        break;
    case 0x8:
        out.opcode = Opcode::Tst;
        break; // TST = ANDS with Rd=0xF
    case 0x9:
        out.opcode = Opcode::Teq;
        break;
    case 0xA:
        out.opcode = Opcode::Cmp;
        break;
    case 0xB:
        out.opcode = Opcode::Cmn;
        break;
    case 0xC:
        out.opcode = S ? Opcode::OrrS : Opcode::Orr;
        break;
    case 0xD:
        out.opcode = S ? Opcode::MovS : Opcode::Mov;
        break;
    case 0xE:
        out.opcode = S ? Opcode::BicS : Opcode::Bic;
        break;
    case 0xF:
        out.opcode = S ? Opcode::MvnS : Opcode::Mvn;
        break;
    }

    // Handle test/compare opcodes (S=1, Rd=0xF -> flag-only)
    if (S && Rd == 0xF) {
        switch (arm_op) {
        case 0x0:
            out.opcode = Opcode::Tst;
            break;
        case 0x1:
            out.opcode = Opcode::Teq;
            break;
        case 0x2:
            out.opcode = Opcode::Cmp;
            break;
        case 0x4:
            out.opcode = Opcode::Cmn;
            break;
        }
    }

    return true;
}

// Thumb32 data processing, plain binary immediate: ADDW, SUBW, MOVW, MOVT
static bool TryDecodeThumb32PlainBinaryImm(u16 hw1, u16 hw2, DecodedInst& out) {
    if ((hw1 & 0xFA00) != 0xF200) {
        return false;
    }
    const u8 op_raw = (hw1 >> 5) & 0xF;
    const u8 Rn = hw1 & 0xF;
    const u8 Rd = (hw2 >> 8) & 0xF;
    const bool i = (hw1 >> 10) & 1;
    const u8 imm3 = (hw2 >> 12) & 0x7;
    const u8 imm8 = hw2 & 0xFF;

    if ((hw1 & 0xFBF0) == 0xF200) {
        // ADDW Rd, Rn, #imm12
        u32 imm12 = (i << 11) | (imm3 << 8) | imm8;
        out.opcode = Opcode::Add;
        out.rd = Rd;
        out.cond = 0xE;
        out.dp.rn = Rn;
        out.dp.rm = 0xFF;
        out.dp.rs = 0xFF;
        out.dp.imm = imm12;
        out.dp.shift_imm = 0;
        out.dp.shift_type = ShiftType::LSL;
        out.dp.flags = 0;
        return true;
    }
    if ((hw1 & 0xFBF0) == 0xF2A0) {
        // SUBW Rd, Rn, #imm12
        u32 imm12 = (i << 11) | (imm3 << 8) | imm8;
        out.opcode = Opcode::Sub;
        out.rd = Rd;
        out.cond = 0xE;
        out.dp.rn = Rn;
        out.dp.rm = 0xFF;
        out.dp.rs = 0xFF;
        out.dp.imm = imm12;
        out.dp.shift_imm = 0;
        out.dp.shift_type = ShiftType::LSL;
        out.dp.flags = 0;
        return true;
    }
    if ((hw1 & 0xFBF0) == 0xF240) {
        // MOVW Rd, #imm16
        u32 imm4 = hw1 & 0xF;
        u32 imm16 = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
        out.opcode = Opcode::Mov;
        out.rd = Rd;
        out.cond = 0xE;
        out.dp.rn = 0;
        out.dp.rm = 0xFF;
        out.dp.rs = 0xFF;
        out.dp.imm = imm16;
        out.dp.shift_imm = 0;
        out.dp.shift_type = ShiftType::LSL;
        out.dp.flags = 2; // Flag 2 = MOVW (16-bit immediate, don't use rotate)
        return true;
    }
    if ((hw1 & 0xFBF0) == 0xF2C0) {
        // MOVT Rd, #imm16 - set upper 16 bits, preserve lower 16
        u32 imm4 = hw1 & 0xF;
        u32 imm16 = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
        out.opcode = Opcode::Movt;
        out.rd = Rd;
        out.cond = 0xE;
        out.dp.rn = 0;
        out.dp.rm = 0xFF;
        out.dp.rs = 0xFF;
        out.dp.imm = imm16;
        out.dp.shift_imm = 0;
        out.dp.shift_type = ShiftType::LSL;
        out.dp.flags = 0;
        return true;
    }
    return false;
}

// Thumb32 single data transfer: LDR.W/STR.W and the byte/half/signed forms
static bool TryDecodeThumb32SingleTransfer(u16 hw1, u16 hw2, u32 pc, DecodedInst& out) {
    if ((hw1 & 0xFE00) != 0xF800) {
        return false;
    }
    const u8 Rn = hw1 & 0xF;
    const u8 Rt = (hw2 >> 12) & 0xF;
    const bool is_load = (hw1 >> 4) & 1;
    const u8 size_bits = (hw1 >> 5) & 0x3; // bits[6:5]: 00=byte, 01=half, 10=word
    const bool is_signed = (hw1 >> 8) & 1; // bit 8
    const bool is_imm12 = (hw1 >> 7) & 1;  // bit 7: 1=imm12 (T3), 0=T4

    u8 size = (size_bits == 0) ? 1 : (size_bits == 1) ? 2 : 4;

    // Helper lambda to select opcode
    auto select_opcode = [&]() {
        if (size == 4) {
            out.opcode = is_load ? Opcode::Ldr : Opcode::Str;
        } else if (size == 2) {
            if (is_load && is_signed)
                out.opcode = Opcode::LdrSH;
            else if (is_load)
                out.opcode = Opcode::LdrH;
            else
                out.opcode = Opcode::StrH;
        } else {
            if (is_load && is_signed)
                out.opcode = Opcode::LdrSB;
            else if (is_load)
                out.opcode = Opcode::LdrB;
            else
                out.opcode = Opcode::StrB;
        }
    };

    // PC-relative (literal) load: Rn = 0xF
    if (Rn == 0xF) {
        s32 offset = hw2 & 0xFFF;
        bool U = (hw1 >> 7) & 1;
        select_opcode();
        out.rd = Rt;
        out.cond = 0xE;
        // Thumb literal base: Align(PC+4, 4); the target address is a
        // decode-time constant
        u32 base = (pc + 4) & ~3u;
        u32 target = U ? (base + offset) : (base - offset);
        if (out.opcode == Opcode::Ldr) {
            // Word literal: absolute-address opcode
            out.opcode = Opcode::LdrLit;
            out.ls.rn = 0xFF;
            out.ls.rm = 0xFF;
            out.ls.offset = static_cast<s32>(target);
        } else {
            // Byte/half literals (rare): generic opcode behind a PcSetup
            // prefix; regs[15] holds the Thumb pipeline PC (PC+4), so the
            // offset is stored relative to it (signed - the generic
            // handlers apply immediate offsets without consulting flags)
            out.ls.rn = 15;
            out.ls.rm = 0xFF;
            out.ls.offset = static_cast<s32>(target) - static_cast<s32>(pc + 4);
        }
        out.ls.mode = AddrMode::Offset;
        out.ls.shift_type = ShiftType::LSL;
        out.ls.shift_imm = 0;
        out.ls.flags = 1;
        return true;
    }

    if (is_imm12) {
        // T3 encoding: positive imm12 offset
        s32 offset = hw2 & 0xFFF;
        select_opcode();
        out.rd = Rt;
        out.cond = 0xE;
        out.ls.rn = Rn;
        out.ls.rm = 0xFF;
        out.ls.offset = offset;
        out.ls.mode = AddrMode::Offset;
        out.ls.shift_type = ShiftType::LSL;
        out.ls.shift_imm = 0;
        out.ls.flags = 1; // U=1 (add)
        return true;
    }

    // T4 encoding (bit 7 = 0): imm8 with P,U,W or register offset
    if ((hw2 & 0x0800) == 0x0800) {
        // imm8 with pre/post-index: hw2 = 1PUW imm8
        s32 offset = hw2 & 0xFF;
        bool P = (hw2 >> 10) & 1;
        bool U = (hw2 >> 9) & 1;
        bool W = (hw2 >> 8) & 1;
        select_opcode();
        out.rd = Rt;
        out.cond = 0xE;
        out.ls.rn = Rn;
        out.ls.rm = 0xFF;
        out.ls.offset = U ? offset : -offset;
        out.ls.shift_type = ShiftType::LSL;
        out.ls.shift_imm = 0;
        out.ls.flags = U ? 1 : 0;
        if (P && !W)
            out.ls.mode = AddrMode::Offset;
        else if (P && W)
            out.ls.mode = AddrMode::PreIndex;
        else // !P
            out.ls.mode = AddrMode::PostIndex;
        return true;
    }

    if ((hw2 & 0x0FC0) == 0x0000) {
        // Register offset: hw2 = 000000 imm2 Rm
        u8 Rm = hw2 & 0xF;
        u8 shift = (hw2 >> 4) & 0x3;
        select_opcode();
        out.rd = Rt;
        out.cond = 0xE;
        out.ls.rn = Rn;
        out.ls.rm = Rm;
        out.ls.offset = 0;
        out.ls.mode = AddrMode::Offset;
        out.ls.shift_type = ShiftType::LSL;
        out.ls.shift_imm = shift;
        out.ls.flags = 1; // U=1 (add)
        return true;
    }
    return false;
}

void ARM_FastInterp::DecodeThumb32(u32 inst, u32 pc, DecodedInst& out) {
    out.inst_size = 4;
    out.cond = 0xE;

    // The 3DS ARM11 is ARMv6K: its Thumb-2 support is only the BL/BLX pairs,
    // so real game code never reaches the rest of this decoder. The full
    // coverage below is kept for homebrew/patched code and is exercised by
    // the test suite against Dynarmic.

    // Thumb32 instructions are stored with first halfword in upper 16 bits
    const u16 hw1 = (inst >> 16) & 0xFFFF;
    const u16 hw2 = inst & 0xFFFF;

    // ========================================================================
    // BL/BLX immediate (Branch with Link) - use native Thumb opcodes
    // ========================================================================
    if ((hw1 & 0xF800) == 0xF000 && (hw2 & 0xC000) == 0xC000) {
        const bool is_blx = (hw2 & 0x1000) == 0;
        const bool S = (hw1 >> 10) & 1;
        const bool J1 = (hw2 >> 13) & 1;
        const bool J2 = (hw2 >> 11) & 1;
        const u32 imm10 = hw1 & 0x3FF;
        const u32 imm11 = hw2 & 0x7FF;

        const bool I1 = !(J1 ^ S);
        const bool I2 = !(J2 ^ S);

        s32 offset;
        if (is_blx) {
            const u32 imm10L = (hw2 >> 1) & 0x3FF;
            offset = (S ? 0xFF000000 : 0) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm10L << 2);
        } else {
            offset = (S ? 0xFF000000 : 0) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);
        }

        // Target and LR are decode-time constants
        if (is_blx) {
            out.opcode = Opcode::ThumbBlxImm;
            out.branch.target = (pc + 4 + offset) & ~3u; // BLX target is word-aligned
        } else {
            out.opcode = Opcode::ThumbBl;
            out.branch.target = pc + 4 + offset;
        }
        out.branch.lr = (pc + 4) | 1;
        return;
    }

    // ========================================================================
    // Unconditional branch (B.W) - 32-bit wide branch
    // ========================================================================
    // 11110 S cond imm6 10 J1 0 J2 imm11 (conditional)
    // 11110 S imm10 10 J1 1 J2 imm11 (unconditional)
    // The conditional form exists only for cond < 0b1110: hw1 cond=111x is the
    // MSR/MRS/misc-control space, decoded further down.
    if ((hw1 & 0xF800) == 0xF000 && (hw2 & 0xC000) == 0x8000 &&
        ((hw2 & 0x1000) != 0 || ((hw1 >> 6) & 0xF) < 0xE)) {
        const bool is_uncond = (hw2 & 0x1000) != 0;

        if (is_uncond) {
            // Unconditional B.W
            const bool S = (hw1 >> 10) & 1;
            const bool J1 = (hw2 >> 13) & 1;
            const bool J2 = (hw2 >> 11) & 1;
            const u32 imm10 = hw1 & 0x3FF;
            const u32 imm11 = hw2 & 0x7FF;

            const bool I1 = !(J1 ^ S);
            const bool I2 = !(J2 ^ S);

            s32 offset =
                (S ? 0xFF000000 : 0) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);

            out.opcode = Opcode::B;
            out.cond = 0xE; // AL
            // True Thumb32 target: PC+4+offset (stored absolute)
            out.branch.target = pc + 4 + offset;
            out.branch.rm = 0xFF;
        } else {
            // Conditional B.W (B<cond>.W)
            const u8 cond = (hw1 >> 6) & 0xF;
            const bool S = (hw1 >> 10) & 1;
            const bool J1 = (hw2 >> 13) & 1;
            const bool J2 = (hw2 >> 11) & 1;
            const u32 imm6 = hw1 & 0x3F;
            const u32 imm11 = hw2 & 0x7FF;

            s32 offset =
                (S ? 0xFFF00000 : 0) | (J2 << 19) | (J1 << 18) | (imm6 << 12) | (imm11 << 1);

            out.opcode = Opcode::B;
            out.cond = cond;
            // True Thumb32 target: PC+4+offset (stored absolute)
            out.branch.target = pc + 4 + offset;
            out.branch.rm = 0xFF;
        }
        return;
    }

    // ========================================================================
    // VFP/SIMD instructions (coprocessor 10 and 11)
    // ========================================================================
    // 1110 110x xxxx xxxx xxxx 101x xxxx xxxx (VFP load/store)
    // 1110 1110 xxxx xxxx xxxx 101x xxxx xxxx (VFP data processing)

    const u8 coproc = (hw2 >> 8) & 0xF;

    // Check for VFP (coprocessor 10 or 11)
    if ((coproc & 0xE) == 0xA) {
        // Reconstruct as ARM-style VFP instruction for reuse of ARM decoder
        // ARM VFP: cond 110x xxxx (load/store) or cond 1110 xxxx (data proc)
        u32 arm_inst = 0xE0000000; // AL condition

        const u8 op_hi4 = (hw1 >> 12) & 0xF;
        const u8 op_next4 = (hw1 >> 8) & 0xF;

        if (op_hi4 == 0xE && (op_next4 == 0xC || op_next4 == 0xD)) {
            // 1110 110x: VFP load/store (single or multiple)
            arm_inst |= 0x0C000000 | ((hw1 & 0x01FF) << 16) | hw2;
        } else if (op_hi4 == 0xE && op_next4 == 0xE) {
            // 1110 1110: VFP data processing or register transfer
            arm_inst |= 0x0E000000 | ((hw1 & 0x00FF) << 16) | hw2;
        } else if (op_hi4 == 0xE && op_next4 == 0xF) {
            // Advanced SIMD - not supported yet
            out.opcode = Opcode::Invalid;
            return;
        } else {
            out.opcode = Opcode::Invalid;
            return;
        }

        // Decode using ARM VFP decoder
        DecodeARM(arm_inst, pc, out);
        out.inst_size = 4; // Override - it's Thumb32
        // Thumb32 Rn==PC fixups: VLDR (literal) uses Align(PC+4,4) as its
        // base, but regs[15] arrives via a PcSetup prefix holding PC+4 (which
        // may be 2 mod 4 in Thumb) - bake the alignment into the offset.
        // Every other VFP load/store with Rn==PC is UNPREDICTABLE in Thumb.
        switch (out.opcode) {
        case Opcode::VldrS:
        case Opcode::VldrD:
            if (out.vfp.rn == 15) {
                out.vfp.offset = static_cast<s16>(out.vfp.offset - static_cast<s32>((pc + 4) & 3u));
            }
            break;
        case Opcode::VstrS:
        case Opcode::VstrD:
        case Opcode::VldmS:
        case Opcode::VldmD:
        case Opcode::VstmS:
        case Opcode::VstmD:
            if (out.vfp.rn == 15) {
                out.opcode = Opcode::Invalid;
            }
            break;
        default:
            break;
        }
        return;
    }

    // ========================================================================
    // Thumb32 -> ARM translation for data processing, load/store, etc.
    // ========================================================================
    // Strategy: reconstruct equivalent ARM instruction, decode with DecodeARM,
    // then fix up inst_size. This reuses all existing ARM handlers.

    const u8 op1_5 = (hw1 >> 11) & 0x1F; // bits [15:11] of hw1
    const u8 op1_hi = (hw1 >> 9) & 0x3;  // bits [10:9] of hw1
    const u8 op2_7 = (hw1 >> 4) & 0x7F;  // bits [10:4] of hw1

    // ---- 1110 100x: Load/Store Multiple, Load/Store Dual ----
    // hw1 bits[10:9] select the sub-space (00 = multiple/dual/exclusive,
    // 01 = DP shifted register, 1x = coprocessor); bit 8 is the P bit of the
    // dual encodings and must not be used for routing.
    if (op1_5 == 0x1D && (hw1 & 0x0600) == 0x0000) {
        // 1110 100x xxxx: LDM.W, STM.W, LDMDB, STMDB, LDRD, STRD, etc.
        const u8 op = (hw1 >> 7) & 0x3; // bits [8:7]
        const bool L = (hw1 >> 4) & 1;  // bit 4 (load vs store for dual)

        if ((hw1 & 0x0040) == 0) {
            // LDM.W / STM.W / LDMDB / STMDB (bit 6 = 0 means multiple)
            // Thumb32 LDM: 1110 100 PU1W L Rn rrrrrrrrrrrrrrrr
            // ARM LDM:     cond 100 PU1WL Rn rrrrrrrrrrrrrrrr
            u32 arm_inst = 0xE0000000;
            arm_inst |= 0x08000000;             // 100 base
            arm_inst |= ((hw1 & 0x03FF) << 16); // PU1WL + Rn from hw1 bits[9:0] -> ARM[25:16]
            arm_inst |= hw2;                    // register list
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        } else {
            // LDRD / STRD (immediate) - bit 6 = 1
            // Thumb32: 1110 100 PU1WL Rn Rt Rt2 imm8
            // ARM LDRD: cond 000 PU1W0 Rn Rd imm4hi 1101 imm4lo (encoding 3)
            // Different encoding - handle directly
            const u8 P = (hw1 >> 8) & 1;
            const u8 U = (hw1 >> 7) & 1;
            const u8 W = (hw1 >> 5) & 1;
            const bool is_load = (hw1 >> 4) & 1;
            const u8 Rn = hw1 & 0xF;
            const u8 Rt = (hw2 >> 12) & 0xF;
            const u8 Rt2 = (hw2 >> 8) & 0xF;
            const u8 imm8 = hw2 & 0xFF;

            // P==0 && W==0 is not load/store dual: it is the load/store
            // exclusive / table branch space (LDREX, STREXD, TBB, ...), which
            // has no handlers here - reject instead of misdecoding as LDRD.
            if (!P && !W) {
                out.opcode = Opcode::Invalid;
                return;
            }
            // Rt/Rt2 == 15 is UNPREDICTABLE (and the handlers would index
            // regs[16]); so is LDRD with Rt == Rt2.
            if (Rt == 15 || Rt2 == 15 || (is_load && Rt == Rt2)) {
                out.opcode = Opcode::Invalid;
                return;
            }
            // Only LDRD (literal) is valid with Rn==PC: STRD to a PC-relative
            // address and LDRD (literal) with writeback are UNPREDICTABLE.
            if (Rn == 15 && (!is_load || W)) {
                out.opcode = Opcode::Invalid;
                return;
            }

            // Decode directly rather than synthesizing an ARM LDRD/STRD and
            // reusing DecodeARM (as the other Thumb32 paths do): the Thumb offset
            // (imm8<<2, range 0-1020) has no equivalent in ARM's LDRD immediate
            // encoding (imm4H:imm4L, range 0-255).
            out.opcode = is_load ? Opcode::Ldrd : Opcode::Strd;
            out.ls.rn = Rn;
            out.ls.rm = 0xFF; // immediate offset
            out.rd = Rt;
            out.ls.rt2 = Rt2; // Thumb32 has an independent Rt2 field
            out.ls.offset = U ? (imm8 << 2) : -(imm8 << 2);
            if (Rn == 15) {
                // LDRD (literal): the architectural base is Align(PC+4,4), but
                // regs[15] arrives via a PcSetup prefix holding PC+4 (which may
                // be 2 mod 4 in Thumb) - bake the alignment into the offset.
                out.ls.offset -= static_cast<s32>((pc + 4) & 3u);
            }
            out.ls.shift_type = ShiftType::LSL;
            out.ls.shift_imm = 0;
            // Encode flags: bit 0 = U, bit 1 = W (writeback), bit 2 = P
            out.ls.flags = (U) | (W << 1) | (P << 2);
            // Encode the P/W addressing mode in ls.mode (Rt2 stored above)
            if (P && !W) {
                out.ls.mode = AddrMode::Offset; // Pre-indexed, no writeback
            } else if (P && W) {
                out.ls.mode = AddrMode::PreIndex;
            } else if (!P && W) {
                out.ls.mode = AddrMode::PostIndex;
            } else {
                out.ls.mode = AddrMode::Offset;
            }
            return;
        }
    }

    // ---- 1110 101x: Data Processing (shifted register) ----
    if (op1_5 == 0x1D && (hw1 & 0x0600) == 0x0200) {
        // 1110 101x xxxx: Data processing (shifted register)
        // Thumb32: 1110 101 0ooo S Rn  0iii Rd ii tt Rm
        // ARM:     cond 000 oooS Rn Rd iiiii tt0 Rm
        const u8 op_raw = (hw1 >> 5) & 0xF; // 4-bit opcode from hw1 bits[8:5]
        const bool S = (hw1 >> 4) & 1;
        const u8 Rn = hw1 & 0xF;
        const u8 Rd = (hw2 >> 8) & 0xF;
        const u8 Rm = hw2 & 0xF;
        const u8 imm3 = (hw2 >> 12) & 0x7;
        const u8 imm2 = (hw2 >> 6) & 0x3;
        const u8 type = (hw2 >> 4) & 0x3;
        const u8 shift_imm = (imm3 << 2) | imm2;

        // Build ARM data processing (register, immediate shift)
        // ARM: cond 000 oooS Rn Rd shift_imm tt 0 Rm
        u32 arm_inst = 0xE0000000;
        arm_inst |= (op_raw << 21) | (S << 20);
        arm_inst |= (Rn << 16) | (Rd << 12);
        arm_inst |= (shift_imm << 7) | (type << 5) | Rm;
        DecodeARM(arm_inst, pc, out);
        out.inst_size = 4;
        return;
    }

    // ---- 11110 0xxxxxx: Data Processing (modified immediate) ----
    // (the branches sharing this space, hw2 bit15=1, were handled above)
    if (TryDecodeThumb32DataProcModImm(hw1, hw2, out)) {
        return;
    }

    // ---- 11110 1xxxxxx: Data Processing (plain binary immediate) ----
    if (TryDecodeThumb32PlainBinaryImm(hw1, hw2, out)) {
        return;
    }

    // ---- 1111 100x xxxx: Single data transfer (LDR.W, STR.W, etc.) ----
    if (TryDecodeThumb32SingleTransfer(hw1, hw2, pc, out)) {
        return;
    }

    // ---- 11111 011xxxx: Multiply, divide, accumulate ----
    if ((hw1 & 0xFF00) == 0xFB00) {
        const u8 op1 = (hw1 >> 4) & 0x7;
        const u8 Rn = hw1 & 0xF;
        const u8 Rd = (hw2 >> 8) & 0xF;
        const u8 Rm = hw2 & 0xF;
        const u8 Ra = (hw2 >> 12) & 0xF;
        const u8 op2 = (hw2 >> 4) & 0x3;

        if (op1 == 0 && op2 == 0) {
            if (Ra == 0xF) {
                // MUL Rd, Rn, Rm
                u32 arm_inst = 0xE0000090;
                arm_inst |= (Rd << 16) | (Rm << 8) | Rn;
                DecodeARM(arm_inst, pc, out);
                out.inst_size = 4;
                return;
            } else {
                // MLA Rd, Rn, Rm, Ra
                u32 arm_inst = 0xE0200090;
                arm_inst |= (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn;
                DecodeARM(arm_inst, pc, out);
                out.inst_size = 4;
                return;
            }
        }
        if (op1 == 0 && op2 == 1) {
            // MLS Rd, Rn, Rm, Ra
            u32 arm_inst = 0xE0600090;
            arm_inst |= (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn;
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        }
        // SDIV / UDIV
        if (op1 == 1 && op2 == 0) {
            // SDIV Rd, Rn, Rm  (ARM: cond 0111 0001 Rd 1111 Rm 0001 Rn)
            u32 arm_inst = 0xE710F010;
            arm_inst |= (Rd << 16) | (Rm << 8) | Rn;
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        }
        if (op1 == 3 && op2 == 0) {
            // UDIV Rd, Rn, Rm  (ARM: cond 0111 0011 Rd 1111 Rm 0001 Rn)
            u32 arm_inst = 0xE730F010;
            arm_inst |= (Rd << 16) | (Rm << 8) | Rn;
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        }

        // Long multiply: SMULL, UMULL, SMLAL, UMLAL
        if ((hw1 & 0xFFF0) == 0xFB80) {
            // SMULL RdLo, RdHi, Rn, Rm
            u8 RdLo = (hw2 >> 12) & 0xF;
            u8 RdHi = (hw2 >> 8) & 0xF;
            u32 arm_inst = 0xE0C00090;
            arm_inst |= (RdHi << 16) | (RdLo << 12) | (Rm << 8) | Rn;
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        }
        if ((hw1 & 0xFFF0) == 0xFBA0) {
            // UMULL RdLo, RdHi, Rn, Rm
            u8 RdLo = (hw2 >> 12) & 0xF;
            u8 RdHi = (hw2 >> 8) & 0xF;
            u32 arm_inst = 0xE0800090;
            arm_inst |= (RdHi << 16) | (RdLo << 12) | (Rm << 8) | Rn;
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        }
        if ((hw1 & 0xFFF0) == 0xFBC0) {
            // SMLAL RdLo, RdHi, Rn, Rm
            u8 RdLo = (hw2 >> 12) & 0xF;
            u8 RdHi = (hw2 >> 8) & 0xF;
            u32 arm_inst = 0xE0E00090;
            arm_inst |= (RdHi << 16) | (RdLo << 12) | (Rm << 8) | Rn;
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        }
        if ((hw1 & 0xFFF0) == 0xFBE0) {
            // UMLAL RdLo, RdHi, Rn, Rm
            u8 RdLo = (hw2 >> 12) & 0xF;
            u8 RdHi = (hw2 >> 8) & 0xF;
            u32 arm_inst = 0xE0A00090;
            arm_inst |= (RdHi << 16) | (RdLo << 12) | (Rm << 8) | Rn;
            DecodeARM(arm_inst, pc, out);
            out.inst_size = 4;
            return;
        }
    }

    // ---- 11110 011xxxx: Bit field operations ----
    if ((hw1 & 0xFBE0) == 0xF360) {
        // BFC/BFI: 1111 0011 0110 Rn 0 imm3 Rd imm2 0 msb
        const u8 Rn = hw1 & 0xF;
        const u8 Rd = (hw2 >> 8) & 0xF;
        const u8 imm3 = (hw2 >> 12) & 0x7;
        const u8 imm2 = (hw2 >> 6) & 0x3;
        const u8 msb = hw2 & 0x1F;
        const u8 lsb = (imm3 << 2) | imm2;
        // ARM BFC/BFI: cond 0111 110 msb Rd lsb 001 Rn(or 1111)
        u32 arm_inst = 0xE7C00010;
        arm_inst |= (msb << 16) | (Rd << 12) | (lsb << 7) | Rn;
        DecodeARM(arm_inst, pc, out);
        out.inst_size = 4;
        return;
    }
    if ((hw1 & 0xFBE0) == 0xF340) {
        // SBFX: 1111 0011 0100 Rn 0 imm3 Rd imm2 0 widthm1
        const u8 Rn = hw1 & 0xF;
        const u8 Rd = (hw2 >> 8) & 0xF;
        const u8 imm3 = (hw2 >> 12) & 0x7;
        const u8 imm2 = (hw2 >> 6) & 0x3;
        const u8 widthm1 = hw2 & 0x1F;
        const u8 lsb = (imm3 << 2) | imm2;
        u32 arm_inst = 0xE7A00050;
        arm_inst |= (widthm1 << 16) | (Rd << 12) | (lsb << 7) | Rn;
        DecodeARM(arm_inst, pc, out);
        out.inst_size = 4;
        return;
    }
    if ((hw1 & 0xFBE0) == 0xF3C0) {
        // UBFX: 1111 0011 1100 Rn 0 imm3 Rd imm2 0 widthm1
        const u8 Rn = hw1 & 0xF;
        const u8 Rd = (hw2 >> 8) & 0xF;
        const u8 imm3 = (hw2 >> 12) & 0x7;
        const u8 imm2 = (hw2 >> 6) & 0x3;
        const u8 widthm1 = hw2 & 0x1F;
        const u8 lsb = (imm3 << 2) | imm2;
        u32 arm_inst = 0xE7E00050;
        arm_inst |= (widthm1 << 16) | (Rd << 12) | (lsb << 7) | Rn;
        DecodeARM(arm_inst, pc, out);
        out.inst_size = 4;
        return;
    }

    // ---- 11110 0111xxx: Miscellaneous control (DSB, DMB, ISB, MRS, MSR) ----
    if ((hw1 & 0xFFF0) == 0xF3B0) {
        // CLREX (hw2 op=0010) clears the monitor; DSB/DMB/ISB have nothing to
        // barrier here (the interpreter performs guest accesses in order)
        out.opcode = (((hw2 >> 4) & 0xF) == 2) ? Opcode::Clrex : Opcode::Nop;
        return;
    }
    if ((hw1 & 0xFFF0) == 0xF3E0) {
        // MRS Rd, CPSR: 1111 0011 1110 1111 1000 Rd 0000 0000
        const u8 Rd = (hw2 >> 8) & 0xF;
        u32 arm_inst = 0xE10F0000; // MRS Rd, CPSR
        arm_inst |= (Rd << 12);
        DecodeARM(arm_inst, pc, out);
        out.inst_size = 4;
        return;
    }
    if ((hw1 & 0xFFF0) == 0xF380 && (hw2 & 0xFF00) == 0x8800) {
        // MSR CPSR, Rn: 1111 0011 1000 Rn 1000 mask 0000 0000
        const u8 Rn = hw1 & 0xF;
        const u8 mask = (hw2 >> 8) & 0xF;
        // ARM MSR: cond 0001 0 R 10 mask 1111 0000 0000 Rm
        u32 arm_inst = 0xE120F000;
        arm_inst |= (mask << 16) | Rn;
        DecodeARM(arm_inst, pc, out);
        out.inst_size = 4;
        return;
    }

    // ---- 11101 0xxxxxx with bit 8 set: Table Branch (TBB/TBH) ----
    if ((hw1 & 0xFFF0) == 0xE8D0) {
        // TBB/TBH: 1110 1000 1101 Rn Rt 0000 000H Rm
        const u8 Rn = hw1 & 0xF;
        const u8 Rm = hw2 & 0xF;
        const bool H = (hw2 >> 4) & 1;
        // Table branch: PC = PC + [Rn + Rm * (H ? 2 : 1)] * 2
        // This doesn't have a direct ARM equivalent - needs custom handler
        // For now, mark as invalid (uncommon instruction)
        out.opcode = Opcode::Invalid;
        return;
    }

    // Unrecognized Thumb32 instruction
    out.opcode = Opcode::Invalid;
}

} // namespace Core::FastInterp
