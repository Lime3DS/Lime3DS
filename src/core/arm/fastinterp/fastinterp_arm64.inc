// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// ARM64-specific optimizations for FastInterp

#pragma once

#ifdef __aarch64__

// =============================================================================
// NZCV Flag Manipulation
// =============================================================================
// Harvest the ARM64 host flags into a packed NZCV word - the same
// N=31/Z=30/C=29/V=28 layout LazyFlags uses (see fastinterp_flags.h).

// Helpers to build the C/V bits (29-28) that the logical-op macros below OR
// into their computed N/Z. `old_nzcv` is the packed word from GetPacked().

// C from a freshly computed shifter carry, V preserved from packed flags
#define ARM64_CV_SHIFTER(carry, old_nzcv) \
    ((static_cast<u32>(carry) << 29) | ((old_nzcv) & (1u << 28)))

// C and V both preserved from packed flags
#define ARM64_CV_KEEP(old_nzcv) ((old_nzcv) & 0x30000000u)

// =============================================================================
// ALU Operations with Native Flag Handling
// =============================================================================
// Arithmetic macros (ADDS/SUBS/CMP/CMN) run the real op + `mrs` to capture true
// host flags. Logical macros (ANDS/ORRS/EORS/TST/TEQ/BICS/MVNS/MOVS) compute N/Z
// directly and OR in the caller's C/V via cv_bits (ARM64_CV_SHIFTER / ARM64_CV_KEEP).

#define ARM64_ADDS(result, op1, op2, flags) do { \
    u32 _r, _f; \
    asm( \
        "adds %w[res], %w[a], %w[b]\n\t" \
        "mrs %x[fl], nzcv" \
        : [res] "=r"(_r), [fl] "=r"(_f) \
        : [a] "r"((u32)(op1)), [b] "r"((u32)(op2)) \
        : "cc" \
    ); \
    (result) = _r; \
    (flags) = _f; \
} while(0)

#define ARM64_SUBS(result, op1, op2, flags) do { \
    u32 _r, _f; \
    asm( \
        "subs %w[res], %w[a], %w[b]\n\t" \
        "mrs %x[fl], nzcv" \
        : [res] "=r"(_r), [fl] "=r"(_f) \
        : [a] "r"((u32)(op1)), [b] "r"((u32)(op2)) \
        : "cc" \
    ); \
    (result) = _r; \
    (flags) = _f; \
} while(0)

#define ARM64_CMP(op1, op2, flags) do { \
    u32 _f; \
    asm( \
        "cmp %w[a], %w[b]\n\t" \
        "mrs %x[fl], nzcv" \
        : [fl] "=r"(_f) \
        : [a] "r"((u32)(op1)), [b] "r"((u32)(op2)) \
        : "cc" \
    ); \
    (flags) = _f; \
} while(0)

#define ARM64_CMN(op1, op2, flags) do { \
    u32 _f; \
    asm( \
        "cmn %w[a], %w[b]\n\t" \
        "mrs %x[fl], nzcv" \
        : [fl] "=r"(_f) \
        : [a] "r"((u32)(op1)), [b] "r"((u32)(op2)) \
        : "cc" \
    ); \
    (flags) = _f; \
} while(0)

#define ARM64_ANDS(result, op1, op2, flags, cv_bits) do { \
    u32 _r = (u32)(op1) & (u32)(op2); \
    (result) = _r; \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

#define ARM64_ORRS(result, op1, op2, flags, cv_bits) do { \
    u32 _r = (u32)(op1) | (u32)(op2); \
    (result) = _r; \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

#define ARM64_EORS(result, op1, op2, flags, cv_bits) do { \
    u32 _r = (u32)(op1) ^ (u32)(op2); \
    (result) = _r; \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

#define ARM64_TST(op1, op2, flags, cv_bits) do { \
    u32 _r = (u32)(op1) & (u32)(op2); \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

#define ARM64_TEQ(op1, op2, flags, cv_bits) do { \
    u32 _r = (u32)(op1) ^ (u32)(op2); \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

#define ARM64_BICS(result, op1, op2, flags, cv_bits) do { \
    u32 _r = (u32)(op1) & ~(u32)(op2); \
    (result) = _r; \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

#define ARM64_MVNS(result, op, flags, cv_bits) do { \
    u32 _r = ~(u32)(op); \
    (result) = _r; \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

#define ARM64_MOVS(result, op, flags, cv_bits) do { \
    u32 _r = (u32)(op); \
    (result) = _r; \
    u32 _n = (_r >> 31) & 1; \
    u32 _z = (_r == 0) ? 1 : 0; \
    (flags) = (_n << 31) | (_z << 30) | (cv_bits); \
} while(0)

// =============================================================================
// ADC/SBC with Carry
// =============================================================================

// ADCS: msr nzcv seeds the carry-in before adcs.
#define ARM64_ADCS(result, op1, op2, carry_in, flags) do { \
    u32 _r, _f; \
    u64 _nzcv_in = (carry_in) ? 0x20000000ULL : 0ULL; /* C is bit 29 */ \
    asm( \
        "msr nzcv, %[nz]\n\t" \
        "adcs %w[res], %w[a], %w[b]\n\t" \
        "mrs %x[fl], nzcv" \
        : [res] "=r"(_r), [fl] "=r"(_f) \
        : [a] "r"((u32)(op1)), [b] "r"((u32)(op2)), [nz] "r"(_nzcv_in) \
        : "cc" \
    ); \
    (result) = _r; \
    (flags) = _f; \
} while(0)

// SBCS: msr nzcv seeds the carry-in before sbcs.
#define ARM64_SBCS(result, op1, op2, carry_in, flags) do { \
    u32 _r, _f; \
    u64 _nzcv_in = (carry_in) ? 0x20000000ULL : 0ULL; /* C is bit 29 */ \
    asm( \
        "msr nzcv, %[nz]\n\t" \
        "sbcs %w[res], %w[a], %w[b]\n\t" \
        "mrs %x[fl], nzcv" \
        : [res] "=r"(_r), [fl] "=r"(_f) \
        : [a] "r"((u32)(op1)), [b] "r"((u32)(op2)), [nz] "r"(_nzcv_in) \
        : "cc" \
    ); \
    (result) = _r; \
    (flags) = _f; \
} while(0)

// RSC with flags: result = op2 - op1 - !C (reverse subtract with carry)
#define ARM64_RSCS(result, op1, op2, carry_in, flags) do { \
    ARM64_SBCS(result, op2, op1, carry_in, flags); \
} while(0)

// =============================================================================
// Fused Compare + Branch Condition Check
// =============================================================================
// cmp + mrs in one asm block (nothing clobbers the flags between them), then
// CondPassed() checks the condition against the NZCV word (fastinterp_flags.h).

#define ARM64_CMP_COND(op1, op2, cond, take_branch, out_nzcv) do { \
    u64 _nzcv64; \
    asm( \
        "cmp %w[a], %w[b]\n" \
        "mrs %[nzcv], nzcv" \
        : [nzcv] "=r"(_nzcv64) \
        : [a] "r"((u32)(op1)), [b] "r"((u32)(op2)) \
        : "cc" \
    ); \
    u32 _nzcv = (u32)_nzcv64; \
    (out_nzcv) = _nzcv; \
    (take_branch) = CondPassed((cond), _nzcv); \
} while(0)

#endif // __aarch64__
