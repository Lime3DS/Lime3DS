// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/common_types.h"

namespace Core::FastInterp {

// ============================================================================
// Lazy Flag Evaluation System
// ============================================================================
//
// Rather than compute NZCV on every ALU op, flag-setting handlers stash the
// operation type and operands in `lazy_flags` and defer the NZCV computation to
// the next reader (usually a conditional branch).
//
// The one non-obvious rule: logical and multiply ops preserve V (and C for
// multiply) from the previous flag state, which is what `nzcv` holds while a
// lazy op is pending. So a handler setting up new lazy flags must Materialize()
// the pending state first (directly, or via GetC()/GetPacked()) before it
// overwrites `op` - otherwise the pending op's C/V are silently dropped.
//
// Reading: Materialize() once then read `nzcv`, or GetPacked() which does both
// (GetC() is the single-bit shortcut the barrel shifter uses).
// ============================================================================

/// Types of operations that can produce flags
enum class FlagOp : u8 {
    None = 0, // No pending flag computation (flags are current)
    Add,      // Addition: N,Z from result; C,V from add overflow
    Sub,      // Subtraction: N,Z from result; C,V from sub overflow
    Rsb,      // Reverse subtract
    Adc,      // Add with carry
    Sbc,      // Subtract with carry
    Rsc,      // Reverse subtract with carry
    And,      // Logical AND: N,Z from result; C from shifter; V unchanged
    Orr,      // Logical OR
    Eor,      // Logical XOR
    Bic,      // Bit clear
    Mov,      // Move: N,Z from result; C from shifter; V unchanged
    Mvn,      // Move NOT
    Cmp,      // Compare (like SUB but no writeback)
    Cmn,      // Compare negative (like ADD but no writeback)
    Mul,      // Multiply: N,Z from result; C,V unchanged
};
// TST/TEQ handlers reuse And/Eor: the flag behavior is identical.

/// Lazy flag state - stores information to compute flags on demand
struct LazyFlags {
    FlagOp op{FlagOp::None}; // Operation that produced these flags
    u32 result{0};           // Result of the operation (for N, Z)
    u32 operand1{0};         // First operand (for C, V computation)
    u32 operand2{0};         // Second operand (for C, V computation)
    u32 shifter_carry{0};    // Carry from barrel shifter (for logical ops)

    // When op == FlagOp::None, the materialized flags: N=bit 31, Z=bit 30,
    // C=bit 29, V=bit 28; bits 27-0 always zero. While op != None, holds the
    // previous flags (source of preserved C/V for logical/multiply ops).
    u32 nzcv{0};

    /// Compute the packed NZCV value from lazy state. Idempotent. The hot
    /// already-materialized case inlines to a load+branch; the lazy computation
    /// is kept out of line so the many inline call sites inside ExecuteBlock
    /// stay small (its codegen is fragile).
    void Materialize() {
        if (op == FlagOp::None) {
            return;
        }
        MaterializeSlow();
    }

    __attribute__((noinline)) void MaterializeSlow() {
        const u32 n = result >> 31;
        const u32 z = (result == 0) ? 1u : 0u;
        u32 c = 0;
        u32 v = 0;

        switch (op) {
        case FlagOp::Add:
        case FlagOp::Cmn:
            // C = carry out of bit 31
            c = (static_cast<u64>(operand1) + operand2) > 0xFFFFFFFFULL;
            // V = signed overflow
            v = ((operand1 ^ result) & (operand2 ^ result)) >> 31;
            break;

        case FlagOp::Sub:
        case FlagOp::Cmp:
            // C = NOT borrow (operand1 >= operand2)
            c = operand1 >= operand2;
            // V = signed overflow
            v = ((operand1 ^ operand2) & (operand1 ^ result)) >> 31;
            break;

        case FlagOp::Rsb:
            // C = NOT borrow (operand2 >= operand1)
            c = operand2 >= operand1;
            // V = signed overflow (reversed operands)
            v = ((operand2 ^ operand1) & (operand2 ^ result)) >> 31;
            break;

        case FlagOp::Adc: {
            u64 full = static_cast<u64>(operand1) + operand2 + shifter_carry;
            c = full > 0xFFFFFFFFULL;
            v = ((operand1 ^ result) & (operand2 ^ result)) >> 31;
            break;
        }

        case FlagOp::Sbc: {
            c = static_cast<u64>(operand1) >= static_cast<u64>(operand2) + (1 - shifter_carry);
            v = ((operand1 ^ operand2) & (operand1 ^ result)) >> 31;
            break;
        }

        case FlagOp::Rsc: {
            c = static_cast<u64>(operand2) >= static_cast<u64>(operand1) + (1 - shifter_carry);
            v = ((operand2 ^ operand1) & (operand2 ^ result)) >> 31;
            break;
        }

        case FlagOp::And:
        case FlagOp::Orr:
        case FlagOp::Eor:
        case FlagOp::Bic:
        case FlagOp::Mov:
        case FlagOp::Mvn:
            // Logical ops: C from shifter, V preserved from previous flags
            c = shifter_carry & 1u;
            v = (nzcv >> 28) & 1u;
            break;

        case FlagOp::Mul:
            // Multiply: C,V preserved from previous flags
            c = (nzcv >> 29) & 1u;
            v = (nzcv >> 28) & 1u;
            break;

        case FlagOp::None:
            // Unreachable (early-out above)
            break;
        }

        nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);
        op = FlagOp::None;
    }

    bool GetC() {
        Materialize();
        return (nzcv >> 29) & 1;
    }

    u32 GetPacked() {
        Materialize();
        return nzcv;
    }

    void SetPacked(u32 packed) {
        op = FlagOp::None;
        nzcv = packed & 0xF0000000u;
    }
};

// ============================================================================
// Condition-pass lookup table
// ============================================================================
// COND_LUT[cond] bit f is set iff ARM condition `cond` passes for the flag
// nibble f = (N<<3)|(Z<<2)|(C<<1)|V, i.e. f == nzcv >> 28.
// Conditions 0xE (AL) and 0xF (unconditional space) always pass.

constexpr std::array<u16, 16> MakeCondLUT() {
    std::array<u16, 16> lut{};
    for (u32 cond = 0; cond < 16; ++cond) {
        for (u32 f = 0; f < 16; ++f) {
            const bool n = (f >> 3) & 1;
            const bool z = (f >> 2) & 1;
            const bool c = (f >> 1) & 1;
            const bool v = f & 1;
            bool pass = false;
            switch (cond) {
            case 0x0:
                pass = z;
                break; // EQ
            case 0x1:
                pass = !z;
                break; // NE
            case 0x2:
                pass = c;
                break; // CS/HS
            case 0x3:
                pass = !c;
                break; // CC/LO
            case 0x4:
                pass = n;
                break; // MI
            case 0x5:
                pass = !n;
                break; // PL
            case 0x6:
                pass = v;
                break; // VS
            case 0x7:
                pass = !v;
                break; // VC
            case 0x8:
                pass = c && !z;
                break; // HI
            case 0x9:
                pass = !c || z;
                break; // LS
            case 0xA:
                pass = n == v;
                break; // GE
            case 0xB:
                pass = n != v;
                break; // LT
            case 0xC:
                pass = !z && n == v;
                break; // GT
            case 0xD:
                pass = z || n != v;
                break; // LE
            default:
                pass = true;
                break; // AL / unconditional
            }
            if (pass) {
                lut[cond] = static_cast<u16>(lut[cond] | (1u << f));
            }
        }
    }
    return lut;
}

inline constexpr std::array<u16, 16> COND_LUT = MakeCondLUT();

/// Check an ARM condition against a packed NZCV word (bits 31-28)
inline bool CondPassed(u8 cond, u32 nzcv) {
    return (COND_LUT[cond] >> (nzcv >> 28)) & 1;
}

} // namespace Core::FastInterp
