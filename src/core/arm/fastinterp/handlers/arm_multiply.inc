// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// ARM Multiply handlers: Mul, MulS, Mla, MlaS, Umull, UmullS, Umlal, UmlalS,
//     Smull, SmullS, Smlal, SmlalS, and the halfword forms Smulxy, Smlaxy,
//     Smulwy, Smlawy, Smlalxy

handle_Mul: {
    CHECK_COND();
    u32 result = state_.regs[inst->mul.rm] * state_.regs[inst->mul.rs];
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_MulS: {
    CHECK_COND();
    u32 result = state_.regs[inst->mul.rm] * state_.regs[inst->mul.rs];
    state_.regs[inst->rd] = result;
    state_.lazy_flags.Materialize();
    state_.lazy_flags.op = FlagOp::Mul;
    state_.lazy_flags.result = result;
    ADVANCE_AND_DISPATCH();
}

handle_Mla: {
    CHECK_COND();
    u32 result = state_.regs[inst->mul.rm] * state_.regs[inst->mul.rs] + state_.regs[inst->mul.rn];
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_MlaS: {
    CHECK_COND();
    u32 result = state_.regs[inst->mul.rm] * state_.regs[inst->mul.rs] + state_.regs[inst->mul.rn];
    state_.regs[inst->rd] = result;
    state_.lazy_flags.Materialize();
    state_.lazy_flags.op = FlagOp::Mul;
    state_.lazy_flags.result = result;
    ADVANCE_AND_DISPATCH();
}

handle_Umull: {
    CHECK_COND();
    u64 result = static_cast<u64>(state_.regs[inst->mul.rm]) * state_.regs[inst->mul.rs];
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    ADVANCE_AND_DISPATCH();
}

handle_UmullS: {
    CHECK_COND();
    u64 result = static_cast<u64>(state_.regs[inst->mul.rm]) * state_.regs[inst->mul.rs];
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    state_.lazy_flags.Materialize();
    state_.lazy_flags.op = FlagOp::Mul;
    // N = bit 63; Z = the whole 64-bit result == 0. Folding "low word nonzero"
    // into bit 0 lets one u32 carry both (bit 31 keeps N, zero iff u64 was 0).
    state_.lazy_flags.result =
        static_cast<u32>(static_cast<u64>(result) >> 32) | (static_cast<u32>(result) ? 1u : 0u);
    ADVANCE_AND_DISPATCH();
}

handle_Umlal: {
    CHECK_COND();
    u64 accum = (static_cast<u64>(state_.regs[inst->mul.rdhi]) << 32) | state_.regs[inst->rd];
    u64 result = static_cast<u64>(state_.regs[inst->mul.rm]) * state_.regs[inst->mul.rs] + accum;
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    ADVANCE_AND_DISPATCH();
}

handle_UmlalS: {
    CHECK_COND();
    u64 accum = (static_cast<u64>(state_.regs[inst->mul.rdhi]) << 32) | state_.regs[inst->rd];
    u64 result = static_cast<u64>(state_.regs[inst->mul.rm]) * state_.regs[inst->mul.rs] + accum;
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    state_.lazy_flags.Materialize();
    state_.lazy_flags.op = FlagOp::Mul;
    // 64-bit N/Z packing, as in UmullS
    state_.lazy_flags.result =
        static_cast<u32>(static_cast<u64>(result) >> 32) | (static_cast<u32>(result) ? 1u : 0u);
    ADVANCE_AND_DISPATCH();
}

handle_Smull: {
    CHECK_COND();
    s64 result = static_cast<s64>(static_cast<s32>(state_.regs[inst->mul.rm])) *
                 static_cast<s32>(state_.regs[inst->mul.rs]);
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    ADVANCE_AND_DISPATCH();
}

handle_SmullS: {
    CHECK_COND();
    s64 result = static_cast<s64>(static_cast<s32>(state_.regs[inst->mul.rm])) *
                 static_cast<s32>(state_.regs[inst->mul.rs]);
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    state_.lazy_flags.Materialize();
    state_.lazy_flags.op = FlagOp::Mul;
    // 64-bit N/Z packing, as in UmullS
    state_.lazy_flags.result =
        static_cast<u32>(static_cast<u64>(result) >> 32) | (static_cast<u32>(result) ? 1u : 0u);
    ADVANCE_AND_DISPATCH();
}

handle_Smlal: {
    CHECK_COND();
    s64 accum = (static_cast<s64>(state_.regs[inst->mul.rdhi]) << 32) |
                static_cast<u64>(state_.regs[inst->rd]);
    s64 result = static_cast<s64>(static_cast<s32>(state_.regs[inst->mul.rm])) *
                 static_cast<s32>(state_.regs[inst->mul.rs]) + accum;
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    ADVANCE_AND_DISPATCH();
}

handle_SmlalS: {
    CHECK_COND();
    s64 accum = (static_cast<s64>(state_.regs[inst->mul.rdhi]) << 32) |
                static_cast<u64>(state_.regs[inst->rd]);
    s64 result = static_cast<s64>(static_cast<s32>(state_.regs[inst->mul.rm])) *
                 static_cast<s32>(state_.regs[inst->mul.rs]) + accum;
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    state_.lazy_flags.Materialize();
    state_.lazy_flags.op = FlagOp::Mul;
    // 64-bit N/Z packing, as in UmullS
    state_.lazy_flags.result =
        static_cast<u32>(static_cast<u64>(result) >> 32) | (static_cast<u32>(result) ? 1u : 0u);
    ADVANCE_AND_DISPATCH();
}

// Halfword multiplies: SMUL<x><y>, SMLA<x><y>, SMULW<y>, SMLAW<y>, SMLAL<x><y>
// mul.flags bit0 = x (Rm half), bit1 = y (Rs half). None of these touch NZCV.
// SMLA<x><y> and SMLAW<y> set Q (CPSR[27]) on signed overflow of the addition;
// SMLAL<x><y> accumulates into 64 bits and does not affect Q.

handle_Smulxy: {
    CHECK_COND();
    const u32 rm_val = state_.regs[inst->mul.rm];
    const u32 rs_val = state_.regs[inst->mul.rs];
    const s32 op1 = static_cast<s16>((inst->mul.flags & 1) ? (rm_val >> 16) : rm_val);
    const s32 op2 = static_cast<s16>((inst->mul.flags & 2) ? (rs_val >> 16) : rs_val);
    state_.regs[inst->rd] = static_cast<u32>(op1 * op2);
    ADVANCE_AND_DISPATCH();
}

handle_Smlaxy: {
    CHECK_COND();
    const u32 rm_val = state_.regs[inst->mul.rm];
    const u32 rs_val = state_.regs[inst->mul.rs];
    const s32 op1 = static_cast<s16>((inst->mul.flags & 1) ? (rm_val >> 16) : rm_val);
    const s32 op2 = static_cast<s16>((inst->mul.flags & 2) ? (rs_val >> 16) : rs_val);
    const u32 product = static_cast<u32>(op1 * op2);
    const u32 accum = state_.regs[inst->mul.rn];
    const u32 result = product + accum;
    if ((~(product ^ accum) & (product ^ result)) >> 31) {
        state_.cpsr_other |= (1 << 27); // Q flag on signed overflow
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_Smulwy: {
    CHECK_COND();
    const u32 rs_val = state_.regs[inst->mul.rs];
    const s32 op2 = static_cast<s16>((inst->mul.flags & 2) ? (rs_val >> 16) : rs_val);
    const s64 product = static_cast<s64>(static_cast<s32>(state_.regs[inst->mul.rm])) * op2;
    state_.regs[inst->rd] = static_cast<u32>(product >> 16);
    ADVANCE_AND_DISPATCH();
}

handle_Smlawy: {
    CHECK_COND();
    const u32 rs_val = state_.regs[inst->mul.rs];
    const s32 op2 = static_cast<s16>((inst->mul.flags & 2) ? (rs_val >> 16) : rs_val);
    const s64 product = static_cast<s64>(static_cast<s32>(state_.regs[inst->mul.rm])) * op2;
    const u32 product_hi = static_cast<u32>(product >> 16);
    const u32 accum = state_.regs[inst->mul.rn];
    const u32 result = product_hi + accum;
    if ((~(product_hi ^ accum) & (product_hi ^ result)) >> 31) {
        state_.cpsr_other |= (1 << 27); // Q flag on signed overflow
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_Smlalxy: {
    CHECK_COND();
    const u32 rm_val = state_.regs[inst->mul.rm];
    const u32 rs_val = state_.regs[inst->mul.rs];
    const s32 op1 = static_cast<s16>((inst->mul.flags & 1) ? (rm_val >> 16) : rm_val);
    const s32 op2 = static_cast<s16>((inst->mul.flags & 2) ? (rs_val >> 16) : rs_val);
    const u64 accum =
        (static_cast<u64>(state_.regs[inst->mul.rdhi]) << 32) | state_.regs[inst->rd];
    const u64 result = accum + static_cast<u64>(static_cast<s64>(op1 * op2));
    state_.regs[inst->rd] = static_cast<u32>(result);
    state_.regs[inst->mul.rdhi] = static_cast<u32>(result >> 32);
    ADVANCE_AND_DISPATCH();
}
