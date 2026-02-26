// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstdint>
#include "common/common_types.h"
#include "core/arm/fastinterp/fastinterp_flags.h"

namespace Core::FastInterp {

// ============================================================================
// Instruction Opcodes
// ============================================================================
// Each opcode corresponds to a handler in the dispatch table.

enum class Opcode : u16 {
    // Special
    Invalid = 0,
    Nop,
    EndBlock, // End of basic block - return to dispatcher

    // Data Processing - ALU
    And,
    Eor,
    Sub,
    Rsb,
    Add,
    Adc,
    Sbc,
    Rsc,
    Tst,
    Teq,
    Cmp,
    Cmn,
    Orr,
    Mov,
    Bic,
    Mvn,

    // Data Processing with S flag (update flags)
    AndS,
    EorS,
    SubS,
    RsbS,
    AddS,
    AdcS,
    SbcS,
    RscS,
    // TstS, TeqS, CmpS, CmnS are always S variants
    OrrS,
    MovS,
    BicS,
    MvnS,

    // Multiply
    Mul,
    MulS,
    Mla,
    MlaS,
    Umull,
    UmullS,
    Umlal,
    UmlalS,
    Smull,
    SmullS,
    Smlal,
    SmlalS,

    // Load/Store
    Ldr,
    LdrB,
    LdrH,
    LdrSB,
    LdrSH,
    Str,
    StrB,
    StrH,

    // Load/Store Multiple
    Ldm,
    Stm,

    // Branch
    B,
    Bl,
    Bx,
    Blx,

    // Miscellaneous
    Clz,
    Swi,
    Mrs,
    Msr,

    // Extension
    Sxtb,
    Sxth,
    Uxtb,
    Uxth,

    // Byte reversal
    Rev,
    Rev16,
    Revsh,

    // Saturating arithmetic
    Qadd,
    Qsub,
    Qdadd,
    Qdsub,
    Ssat,
    Usat,

    // Parallel add/sub
    Sadd16,
    Ssub16,
    Sadd8,
    Ssub8,
    Uadd16,
    Usub16,
    Uadd8,
    Usub8,

    // Unsigned saturating parallel
    Uqadd16,
    Uqsub16,
    Uqadd8,
    Uqsub8,

    // Signed saturating parallel
    Qadd16,
    Qsub16,
    Qadd8,
    Qsub8,

    // Signed halving parallel
    Shadd16,
    Shsub16,
    Shadd8,
    Shsub8,

    // Unsigned halving parallel
    Uhadd16,
    Uhsub16,
    Uhadd8,
    Uhsub8,

    // Extend with Add
    Sxtab,
    Sxtab16,
    Sxtah,
    Uxtab,
    Uxtab16,
    Uxtah,

    // Bit field
    Bfc,
    Bfi,
    Sbfx,
    Ubfx,

    // Bit reversal
    Rbit,

    // Move top halfword
    Movt,

    // SIMD/Packed
    Pkhbt,
    Pkhtb,
    Sel,
    Usad8,
    Usada8,

    // VFP Register Transfer
    VmovArmToS, // VMOV Sn, Rt (ARM to single)
    VmovSToArm, // VMOV Rt, Sn (single to ARM)
    VmovArmToD, // VMOV Dn, Rt, Rt2 (ARM to double)
    VmovDToArm, // VMOV Rt, Rt2, Dn (double to ARM)
    Vmrs,       // VMRS Rt, FPSCR
    Vmsr,       // VMSR FPSCR, Rt

    // VFP Data Processing (single precision)
    VaddS,    // VADD.F32
    VsubS,    // VSUB.F32
    VmulS,    // VMUL.F32
    VdivS,    // VDIV.F32
    VmlaS,    // VMLA.F32 (multiply-accumulate)
    VmlsS,    // VMLS.F32 (multiply-subtract)
    VnmlaS,   // VNMLA.F32
    VnmlsS,   // VNMLS.F32
    VnmulS,   // VNMUL.F32
    VabsS,    // VABS.F32
    VnegS,    // VNEG.F32
    VsqrtS,   // VSQRT.F32
    VmovS,    // VMOV.F32 Sd, Sm (register to register)
    VmovImmS, // VMOV.F32 Sd, #imm

    // VFP Data Processing (double precision)
    VaddD,    // VADD.F64
    VsubD,    // VSUB.F64
    VmulD,    // VMUL.F64
    VdivD,    // VDIV.F64
    VmlaD,    // VMLA.F64
    VmlsD,    // VMLS.F64
    VnmlaD,   // VNMLA.F64
    VnmlsD,   // VNMLS.F64
    VnmulD,   // VNMUL.F64
    VabsD,    // VABS.F64
    VnegD,    // VNEG.F64
    VsqrtD,   // VSQRT.F64
    VmovD,    // VMOV.F64 Dd, Dm
    VmovImmD, // VMOV.F64 Dd, #imm

    // VFP Comparison
    VcmpS,  // VCMP.F32
    VcmpD,  // VCMP.F64
    VcmpzS, // VCMP.F32 Sd, #0.0
    VcmpzD, // VCMP.F64 Dd, #0.0

    // VFP Conversion
    VcvtSToInt, // VCVT.S32.F32 or VCVT.U32.F32
    VcvtIntToS, // VCVT.F32.S32 or VCVT.F32.U32
    VcvtDToS,   // VCVT.F32.F64
    VcvtSToD,   // VCVT.F64.F32
    VcvtDToInt, // VCVT.S32.F64 or VCVT.U32.F64
    VcvtIntToD, // VCVT.F64.S32 or VCVT.F64.U32

    // VFP Load/Store
    VldrS, // VLDR Sd, [Rn, #imm]
    VstrS, // VSTR Sd, [Rn, #imm]
    VldrD, // VLDR Dd, [Rn, #imm]
    VstrD, // VSTR Dd, [Rn, #imm]

    // VFP Load/Store Multiple
    VldmS, // VLDM {Sd, Sd+1, ...}
    VstmS, // VSTM {Sd, Sd+1, ...}
    VldmD, // VLDM {Dd, Dd+1, ...}
    VstmD, // VSTM {Dd, Dd+1, ...}

    // =========================================================================
    // Native Thumb16 opcodes
    // =========================================================================
    // These handlers work directly with Thumb encoding, no ARM conversion.
    // Operands are pre-decoded into rd, dp.*, lsm.*, branch.* fields.

    // Shift by immediate (format: 000 op imm5 Rm Rd)
    ThumbLslImm, // LSL Rd, Rm, #imm5
    ThumbLsrImm, // LSR Rd, Rm, #imm5
    ThumbAsrImm, // ASR Rd, Rm, #imm5

    // Add/Sub register 3-operand (format: 000110x Rm Rn Rd)
    ThumbAddReg3, // ADD Rd, Rn, Rm
    ThumbSubReg3, // SUB Rd, Rn, Rm

    // Add/Sub 3-bit immediate (format: 000111x imm3 Rn Rd)
    ThumbAddImm3, // ADD Rd, Rn, #imm3
    ThumbSubImm3, // SUB Rd, Rn, #imm3

    // 8-bit immediate operations (format: 001 op Rd imm8)
    ThumbMovImm8, // MOV Rd, #imm8
    ThumbCmpImm8, // CMP Rn, #imm8
    ThumbAddImm8, // ADD Rd, #imm8
    ThumbSubImm8, // SUB Rd, #imm8

    // Data processing register (format: 010000 op Rm Rd)
    ThumbAnd,    // AND Rd, Rm
    ThumbEor,    // EOR Rd, Rm
    ThumbLslReg, // LSL Rd, Rs
    ThumbLsrReg, // LSR Rd, Rs
    ThumbAsrReg, // ASR Rd, Rs
    ThumbAdc,    // ADC Rd, Rm
    ThumbSbc,    // SBC Rd, Rm
    ThumbRor,    // ROR Rd, Rs
    ThumbTst,    // TST Rn, Rm
    ThumbNeg,    // NEG Rd, Rm (RSB Rd, Rm, #0)
    ThumbCmpReg, // CMP Rn, Rm
    ThumbCmn,    // CMN Rn, Rm
    ThumbOrr,    // ORR Rd, Rm
    ThumbMul,    // MUL Rd, Rm
    ThumbBic,    // BIC Rd, Rm
    ThumbMvn,    // MVN Rd, Rm

    // High register operations (format: 010001 op Rm Rd)
    ThumbAddHi,  // ADD Rd, Rm (no flags, high regs)
    ThumbCmpHi,  // CMP Rn, Rm (high regs)
    ThumbMovHi,  // MOV Rd, Rm (no flags, high regs)
    ThumbBx,     // BX Rm
    ThumbBlxReg, // BLX Rm

    // Load/Store word (format: 0101xx0 Rm Rn Rd / 011xx imm5 Rn Rd)
    ThumbLdrReg,  // LDR Rd, [Rn, Rm]
    ThumbStrReg,  // STR Rd, [Rn, Rm]
    ThumbLdrImm5, // LDR Rd, [Rn, #imm5*4]
    ThumbStrImm5, // STR Rd, [Rn, #imm5*4]

    // Load/Store byte (format: 0101xx1 Rm Rn Rd / 011xx imm5 Rn Rd)
    ThumbLdrbReg,  // LDRB Rd, [Rn, Rm]
    ThumbStrbReg,  // STRB Rd, [Rn, Rm]
    ThumbLdrbImm5, // LDRB Rd, [Rn, #imm5]
    ThumbStrbImm5, // STRB Rd, [Rn, #imm5]

    // Load/Store halfword
    ThumbLdrhReg,  // LDRH Rd, [Rn, Rm]
    ThumbStrhReg,  // STRH Rd, [Rn, Rm]
    ThumbLdrhImm5, // LDRH Rd, [Rn, #imm5*2]
    ThumbStrhImm5, // STRH Rd, [Rn, #imm5*2]

    // Signed load
    ThumbLdrsb, // LDRSB Rd, [Rn, Rm]
    ThumbLdrsh, // LDRSH Rd, [Rn, Rm]

    // SP-relative load/store (format: 1001x Rd imm8)
    ThumbLdrSp, // LDR Rd, [SP, #imm8*4]
    ThumbStrSp, // STR Rd, [SP, #imm8*4]

    // PC-relative load (format: 01001 Rd imm8)
    ThumbLdrPc, // LDR Rd, [PC, #imm8*4]

    // Add to SP/PC (format: 1010x Rd imm8)
    ThumbAddPcImm, // ADD Rd, PC, #imm8*4 (ADR)
    ThumbAddSpImm, // ADD Rd, SP, #imm8*4

    // Adjust SP (format: 10110000x imm7)
    ThumbAddSpImm7, // ADD SP, #imm7*4
    ThumbSubSpImm7, // SUB SP, #imm7*4

    // Push/Pop (format: 1011x10x reg_list)
    ThumbPush, // PUSH {reg_list}
    ThumbPop,  // POP {reg_list}

    // Load/Store Multiple (format: 1100x Rn reg_list)
    ThumbLdmia, // LDMIA Rn!, {reg_list}
    ThumbStmia, // STMIA Rn!, {reg_list}

    // Conditional branch (format: 1101 cond imm8)
    ThumbBCond, // B<cond> label

    // Unconditional branch (format: 11100 imm11)
    ThumbB, // B label

    // Software interrupt
    ThumbSwi, // SWI imm8

    // Sign/Zero extend (format: 10110010xx Rm Rd)
    ThumbSxth, // SXTH Rd, Rm
    ThumbSxtb, // SXTB Rd, Rm
    ThumbUxth, // UXTH Rd, Rm
    ThumbUxtb, // UXTB Rd, Rm

    // Byte reversal (format: 10111010xx Rm Rd)
    ThumbRev,   // REV Rd, Rm
    ThumbRev16, // REV16 Rd, Rm
    ThumbRevsh, // REVSH Rd, Rm

    // =========================================================================
    // Thumb32 opcodes (BL, VFP, etc.)
    // =========================================================================
    ThumbBl,     // BL label (Thumb32)
    ThumbBlxImm, // BLX label (Thumb32)

    // =========================================================================
    // Fused instruction opcodes
    // =========================================================================
    // Common compare/test + branch sequences executed as a single dispatch.

    FusedSubsBcc,        // SUBS Rd, Rn, #imm; B<cond>
    FusedSubsRegBcc,     // SUBS Rd, Rn, Rm; B<cond>
    FusedCmpBcc,         // CMP Rn, #imm; B<cond>
    FusedCmpRegBcc,      // CMP Rn, Rm; B<cond>
    FusedTstBcc,         // TST Rn, #imm; B<cond>
    FusedTstRegBcc,      // TST Rn, Rm; B<cond>
    FusedTeqBcc,         // TEQ Rn, #imm; B<cond>
    FusedTeqRegBcc,      // TEQ Rn, Rm; B<cond>
    ThumbFusedSubsBcc,   // SUBS Rd, #imm8; B<cond>
    ThumbFusedCmpBcc,    // CMP Rn, #imm8; B<cond>
    ThumbFusedCmpRegBcc, // CMP Rn, Rm; B<cond>
    ThumbFusedTstBcc,    // TST Rn, Rm; B<cond> (Thumb TST is register-only)
    FusedAddCmpBcc,      // ADD Rd, Rn, #imm; CMP Rd, Rm; B<cond> (iterator loops)

    // =========================================================================
    // Unrolled loop opcodes
    // =========================================================================
    // These execute multiple loop iterations inline without dispatch overhead.
    // Only generated for hot blocks that are tight loops.

    // Thumb: SUBS Rd, #imm8; BNE self (countdown loop)
    ThumbUnrolledSubsLoop,

    // ARM: SUBS Rd, Rn, #imm; BNE self (countdown loop)
    ArmUnrolledSubsLoop,

    // Thumb: CMP Rn, Rm; BNE self (spin loop / wait pattern)
    ThumbUnrolledCmpLoop,

    // Coprocessor (CP15)
    Mrc, // MRC p15, ... (read CP15 register)
    Mcr, // MCR p15, ... (write CP15 register)

    // Exclusive load/store
    Ldrex,  // LDREX Rd, [Rn]
    Strex,  // STREX Rd, Rm, [Rn]
    Ldrexb, // LDREXB
    Strexb, // STREXB
    Ldrexh, // LDREXH
    Strexh, // STREXH
    Ldrexd, // LDREXD Rt, Rt+1, [Rn]
    Strexd, // STREXD Rd, Rm, Rm+1, [Rn]
    Clrex,  // CLREX

    // Doubleword load/store
    Ldrd, // LDRD Rd, Rd+1, [Rn, #offset]
    Strd, // STRD Rd, Rd+1, [Rn, #offset]

    // Halfword multiplies (misc space; x/y half selectors in mul.flags)
    Smulxy,  // SMUL<x><y> Rd, Rm, Rs
    Smlaxy,  // SMLA<x><y> Rd, Rm, Rs, Rn (sets Q on accumulate overflow)
    Smulwy,  // SMULW<y> Rd, Rm, Rs
    Smlawy,  // SMLAW<y> Rd, Rm, Rs, Rn (sets Q on accumulate overflow)
    Smlalxy, // SMLAL<x><y> RdLo, RdHi, Rm, Rs

    // =========================================================================
    // Decode-time specialized opcodes
    // =========================================================================
    // Imm-offset load/store: decode guarantees offset addressing mode,
    // immediate offset, rd != 15, rn != 15 (generic opcodes keep the rest)
    LdrImm,  // LDR Rd, [Rn, #imm]
    StrImm,  // STR Rd, [Rn, #imm]
    LdrBImm, // LDRB Rd, [Rn, #imm]
    StrBImm, // STRB Rd, [Rn, #imm]
    LdrHImm, // LDRH Rd, [Rn, #imm] (zero-extends, like LdrH)
    StrHImm, // STRH Rd, [Rn, #imm]

    // ARM BLX immediate (always switches ARM->Thumb); target, LR, and
    // destination mode are decode-time constants. Blx keeps the register form.
    BlxImm,

    // =========================================================================
    // PC-source elimination
    // =========================================================================
    // LDR literal: word load from an absolute address (ls.offset holds the
    // absolute literal-pool address, precomputed at decode). rd==15 branches.
    LdrLit,

    // PC-setup prefix: materializes the pipeline-adjusted PC (imm32) into
    // regs[15] and falls through to the next slot. Decode inserts it before the
    // rare instructions that read R15 as a source and cannot be folded to a
    // constant, so the dispatch loop never has to keep regs[15] current.
    // inst_size and ticks are 0 (the following real instruction carries both).
    PcSetup,

    OpcodeCount
};

// ============================================================================
// Pre-decoded Instruction Representation
// ============================================================================
// Operands are extracted at decode time so handlers never re-parse encodings.

/// Shift type for barrel shifter operations
enum class ShiftType : u8 {
    LSL = 0, // Logical shift left
    LSR = 1, // Logical shift right
    ASR = 2, // Arithmetic shift right
    ROR = 3, // Rotate right (includes RRX when amount is 0)
};

/// Addressing mode for load/store
enum class AddrMode : u8 {
    Offset,    // [Rn, #offset]
    PreIndex,  // [Rn, #offset]!
    PostIndex, // [Rn], #offset
};

/// Pre-decoded instruction
struct DecodedInst {
    Opcode opcode; // 2 bytes
    u8 cond;       // 1 byte: Condition code (0-14, 15=unconditional)
    u8 rd;         // 1 byte: Destination register

    union {
        // Data processing format
        struct {
            u8 rn;                // First source register
            u8 rm;                // Second source register (0xFF = immediate operand)
            u8 rs;                // Shift amount register (0xFF = immediate shift)
            ShiftType shift_type; // Type of shift
            u8 shift_imm;         // Immediate shift amount (0-31)
            u8 flags;             // Bit flags for variants
            u32 imm;              // Immediate value (already rotated)
        } dp;

        // Multiply format
        struct {
            u8 rn;    // Accumulator register (for MLA, etc.)
            u8 rm;    // First multiply operand
            u8 rs;    // Second multiply operand
            u8 rdhi;  // High destination (for long multiply)
            u8 flags; // Halfword multiplies: bit0 = x (Rm half), bit1 = y (Rs half)
        } mul;

        // Load/Store format
        struct {
            u8 rn;                // Base register
            u8 rm;                // Index register (0xFF = immediate offset)
            AddrMode mode;        // Addressing mode
            ShiftType shift_type; // Shift for register offset
            u8 shift_imm;         // Shift amount
            u8 flags;             // U (add/sub), W (writeback), etc.
            u8 rt2;               // Second transfer register (Ldrd/Strd only;
                                  // rd+1 in ARM, independent Rt2 in Thumb32)
            s32 offset;           // Immediate offset (signed, already computed)
        } ls;

        // Load/Store Multiple format
        struct {
            u8 rn;        // Base register
            u8 count;     // Popcount of reg_list (precomputed at decode)
            u16 reg_list; // Bitmask of registers
            u8 flags;     // P,U,S,W bits
            s8 base_off;  // Base-to-first-access offset (folds increment/before)
            s16 wb_delta; // Writeback delta: +count*4 or -count*4
        } lsm;

        // Branch format
        struct {
            u32 target; // Absolute branch target (precomputed at decode)
            u8 rm;      // Register for BX/BLX reg; BlxImm: destination T bit
            u32 lr;     // Link-register value (precomputed for Bl/BlxImm/
                        // ThumbBl/ThumbBlxImm)
        } branch;

        // VFP format
        struct {
            u8 sd;      // Destination S/D register (0-31 for S, 0-15 for D)
            u8 sn;      // First source register
            u8 sm;      // Second source register
            u8 rt;      // ARM register for transfers
            u8 rt2;     // Second ARM register (for double transfers)
            u8 rn;      // Base register for load/store
            s16 offset; // Load/store offset (in bytes)
            u8 flags;   // Bit 0: unsigned (for VCVT), Bit 1: to_integer, etc.
        } vfp;

        // VFP immediate format (VmovImmS/D): fully expanded modified-immediate
        // constant precomputed at decode
        struct {
            u8 sd;  // Destination register (same byte slot as vfp.sd)
            u32 lo; // F32 bits / low word of F64 bits
            u32 hi; // High word of F64 bits (VmovImmD only)
        } vimm;

        // Fused instruction format
        // Combines ALU operation + conditional branch into single handler
        struct {
            u8 rn; // First operand register (for SUBS/CMP)
            u8 rm; // Second operand register (0xFF=imm no rotation, 0xFE=imm with rotation)
            u8 branch_cond;    // Condition for the branch (0=EQ, 1=NE, etc.)
            u8 combined_size;  // Total size of fused instructions in bytes
            u32 imm;           // Immediate operand (for SUBS Rd,Rn,#imm or CMP Rn,#imm)
            u32 branch_target; // Absolute taken-branch target (precomputed at decode)
        } fused;

        // Generic 32-bit immediate
        u32 imm32;
    };

    // Size in bytes of original instruction (4 for ARM, 2 or 4 for Thumb)
    u8 inst_size;

    // Tick cost for this instruction (matches Dynarmic's variable tick model)
    u8 ticks;
};

static_assert(sizeof(DecodedInst) == 20, "DecodedInst should be 20 bytes");

// ============================================================================
// Basic Block Structure
// ============================================================================

/// Maximum instruction slots per block: 31 real instructions + 1 reserved for
/// the EndBlock terminator every block carries (DISPATCH has no bounds check).
/// Most game blocks are 5-15 instructions; 31 handles 95%+ of blocks
constexpr size_t MAX_BLOCK_SIZE = 32;

/// Block flags
constexpr u16 BLOCK_FLAG_THUMB = 0x01;       // Block contains Thumb code
constexpr u16 BLOCK_FLAG_HOT = 0x02;         // Block has been marked as hot
constexpr u16 BLOCK_FLAG_REOPTIMIZED = 0x04; // Block has been reoptimized

/// Threshold for considering a block "hot" (frequently executed)
constexpr u32 HOT_BLOCK_THRESHOLD = 1000;

/// Basic block of decoded instructions
struct BasicBlock {
    u32 start_pc;   // Starting PC of this block
    u32 end_pc;     // Ending PC (exclusive)
    u16 inst_count; // Number of real instructions (the EndBlock terminator at
                    // instructions[inst_count] is not counted)
    u16 flags;      // Block flags

    // Execution counter for hot path detection
    u32 exec_count;

    // Chain target for block chaining (kept in the header so the chain fast
    // path and the other hot header fields share a cache line)
    BasicBlock* chain_target; // Next block for chaining

    // Decoded instructions (inline for cache locality)
    std::array<DecodedInst, MAX_BLOCK_SIZE> instructions;

    bool IsThumb() const {
        return flags & BLOCK_FLAG_THUMB;
    }
    void SetThumb(bool thumb) {
        flags = (flags & ~BLOCK_FLAG_THUMB) | (thumb ? BLOCK_FLAG_THUMB : 0);
    }
    bool IsHot() const {
        return flags & BLOCK_FLAG_HOT;
    }
    void SetHot() {
        flags |= BLOCK_FLAG_HOT;
    }
    bool IsReoptimized() const {
        return flags & BLOCK_FLAG_REOPTIMIZED;
    }
    void SetReoptimized() {
        flags |= BLOCK_FLAG_REOPTIMIZED;
    }
};

// ============================================================================
// Fusion Statistics
// ============================================================================
// Tracks fusion opportunities and hit rates for performance analysis.

struct FusionStats {
    u64 blocks_decoded{0};         // Total blocks decoded
    u64 fusions_attempted{0};      // Fusion opportunities detected
    u64 fusions_applied{0};        // Fusions actually applied
    u64 hot_blocks_detected{0};    // Blocks that exceeded hot threshold
    u64 hot_blocks_reoptimized{0}; // Blocks that were reoptimized after becoming hot
    u64 dead_flags_eliminated{0};  // Flag-setters downgraded to non-flag-setting forms

    // Per-pattern fusion counts
    u64 subs_bcc_fusions{0};    // SUBS + Bcc fusions
    u64 cmp_bcc_fusions{0};     // CMP + Bcc fusions
    u64 tst_bcc_fusions{0};     // TST + Bcc fusions
    u64 teq_bcc_fusions{0};     // TEQ + Bcc fusions
    u64 add_cmp_bcc_fusions{0}; // ADD + CMP + Bcc fusions
    u64 thumb_fusions{0};       // All Thumb fusions

    void Reset() {
        blocks_decoded = 0;
        fusions_attempted = 0;
        fusions_applied = 0;
        hot_blocks_detected = 0;
        hot_blocks_reoptimized = 0;
        dead_flags_eliminated = 0;
        subs_bcc_fusions = 0;
        cmp_bcc_fusions = 0;
        tst_bcc_fusions = 0;
        teq_bcc_fusions = 0;
        add_cmp_bcc_fusions = 0;
        thumb_fusions = 0;
    }
};

// ============================================================================
// Interpreter State
// ============================================================================
// This structure holds all CPU state, optimized for cache locality.
// Hot fields (registers, PC, flags) are placed first.

/// Page table constants (duplicated here to avoid including memory.h)
constexpr int FASTINTERP_PAGE_BITS = 12;
constexpr u32 FASTINTERP_PAGE_MASK = (1 << FASTINTERP_PAGE_BITS) - 1;

struct InterpreterState {
    // Hot path: Guest registers (64 bytes - fits in one cache line)
    std::array<u32, 16> regs; // R0-R15 (R15 = PC)

    // Raw pointer array of the current PageTable, or the all-null
    // empty_page_table (fastinterp.cpp) when no table is set. Never null, so
    // the memory fast paths only test the per-page entry.
    u8** page_pointers{nullptr};

    // Lazy flags (separate from CPSR for fast access)
    LazyFlags lazy_flags;

    // CPSR (non-flag bits: mode, interrupt masks, etc.)
    u32 cpsr_other; // CPSR without NZCV flags

    // VFP registers (256 bytes)
    std::array<u32, 64> vfp_regs; // D0-D31 as 32-bit halves
    u32 fpscr;
    u32 fpexc;

    // CP15 registers
    u32 cp15_thread_uprw;
    u32 cp15_thread_uro;

    // Execution state
    bool thumb_mode;

    // Helper to get/set PC
    u32 PC() const {
        return regs[15];
    }
    void SetPC(u32 pc) {
        regs[15] = pc;
    }

    // Build full CPSR from lazy flags and other bits
    u32 GetCPSR() {
        u32 cpsr = lazy_flags.GetPacked() | (cpsr_other & 0x0FFFFFFF);
        // Sync T bit from thumb_mode (branch handlers update thumb_mode
        // but not cpsr_other, so we must merge here)
        cpsr = (cpsr & ~(1u << 5)) | (static_cast<u32>(thumb_mode) << 5);
        return cpsr;
    }

    // Set CPSR (splits into flags and other)
    void SetCPSR(u32 cpsr) {
        lazy_flags.SetPacked(cpsr);
        cpsr_other = cpsr & 0x0FFFFFFF;
    }
};

} // namespace Core::FastInterp
