// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Fused compare/test + branch handlers, plus the unrolled tight-loop handlers
// that ReoptimizeHotBlock generates. Branch conditions are checked with
// CondPassed() (COND_LUT, fastinterp_flags.h) against a packed NZCV word.
// No CHECK_COND: TryFuseArm/TryFuseThumb only fuse cond==AL instructions.

handle_FusedSubsBcc: {
    // SUBS Rd, Rn, #imm; B<cond> label
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = inst->fused.imm;
    u32 result = op1 - op2;
    state_.regs[inst->rd] = result;
    executed += inst->ticks;

    // Fast path for NE (most common loop condition) - avoid flag computation
    if (inst->fused.branch_cond == 1) {
        // Lazy flags for NE only need result stored
        state_.lazy_flags.op = FlagOp::Sub;
        state_.lazy_flags.result = result;
        state_.lazy_flags.operand1 = op1;
        state_.lazy_flags.operand2 = op2;
        if (result != 0) {
            SET_PC(inst->fused.branch_target);
            goto chain;
        }
        SET_PC(GET_PC() + inst->fused.combined_size);
        DISPATCH();
    }

    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedSubsRegBcc: {
    // SUBS Rd, Rn, Rm; B<cond> label
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = state_.regs[inst->fused.rm];
    u32 result = op1 - op2;
    state_.regs[inst->rd] = result;
    executed += inst->ticks;

    if (inst->fused.branch_cond == 1) {
        state_.lazy_flags.op = FlagOp::Sub;
        state_.lazy_flags.result = result;
        state_.lazy_flags.operand1 = op1;
        state_.lazy_flags.operand2 = op2;
        if (result != 0) {
            SET_PC(inst->fused.branch_target);
            goto chain;
        }
        SET_PC(GET_PC() + inst->fused.combined_size);
        DISPATCH();
    }

    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedCmpBcc: {
    // CMP Rn, #imm; B<cond> label
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = inst->fused.imm;

#ifdef __aarch64__
    // ARM64: native CMP + LUT-based condition check
    u32 take_branch, nzcv;
    ARM64_CMP_COND(op1, op2, inst->fused.branch_cond, take_branch, nzcv);
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2;

    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);

    const bool take_branch = CondPassed(inst->fused.branch_cond, nzcv);
#endif

    executed += inst->ticks;

    if (take_branch) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedCmpRegBcc: {
    // CMP Rn, Rm; B<cond> label
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = state_.regs[inst->fused.rm];

#ifdef __aarch64__
    // ARM64: native CMP + LUT-based condition check
    u32 take_branch, nzcv;
    ARM64_CMP_COND(op1, op2, inst->fused.branch_cond, take_branch, nzcv);
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2;

    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);

    const bool take_branch = CondPassed(inst->fused.branch_cond, nzcv);
#endif

    executed += inst->ticks;

    if (take_branch) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_ThumbFusedSubsBcc: {
    // Thumb: SUBS Rd, #imm8; B<cond> label
    u32 op1 = state_.regs[inst->rd];
    u32 op2 = inst->fused.imm;
    u32 result = op1 - op2;
    state_.regs[inst->rd] = result;
    executed += inst->ticks;

    if (inst->fused.branch_cond == 1) {
        state_.lazy_flags.op = FlagOp::Sub;
        state_.lazy_flags.result = result;
        state_.lazy_flags.operand1 = op1;
        state_.lazy_flags.operand2 = op2;
        if (result != 0) {
            SET_PC(inst->fused.branch_target);
            goto chain;
        }
        SET_PC(GET_PC() + inst->fused.combined_size);
        DISPATCH();
    }

    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_ThumbFusedCmpBcc: {
    // Thumb: CMP Rn, #imm8; B<cond> label
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = inst->fused.imm;

#ifdef __aarch64__
    // ARM64: native CMP + LUT-based condition check
    u32 take_branch, nzcv;
    ARM64_CMP_COND(op1, op2, inst->fused.branch_cond, take_branch, nzcv);
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2;

    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);

    const bool take_branch = CondPassed(inst->fused.branch_cond, nzcv);
#endif

    executed += inst->ticks;

    if (take_branch) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_ThumbFusedCmpRegBcc: {
    // Thumb: CMP Rn, Rm; B<cond> label (register-register compare)
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = state_.regs[inst->fused.rm];

#ifdef __aarch64__
    // ARM64: native CMP + LUT-based condition check
    u32 take_branch, nzcv;
    ARM64_CMP_COND(op1, op2, inst->fused.branch_cond, take_branch, nzcv);
    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;
#else
    u32 result = op1 - op2;

    // Fast path for NE (most common loop condition)
    if (inst->fused.branch_cond == 1) {
        state_.lazy_flags.op = FlagOp::Cmp;
        state_.lazy_flags.result = result;
        state_.lazy_flags.operand1 = op1;
        state_.lazy_flags.operand2 = op2;
        executed += inst->ticks;
        if (result != 0) {
            SET_PC(inst->fused.branch_target);
            goto chain;
        }
        SET_PC(GET_PC() + inst->fused.combined_size);
        DISPATCH();
    }

    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;

    const bool take_branch = CondPassed(inst->fused.branch_cond, nzcv);
#endif

    if (take_branch) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedTstBcc: {
    // ARM: TST Rn, #imm; B<cond> label (logical AND, sets N,Z,C)
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = inst->fused.imm;
    u32 result = op1 & op2;

    // TST sets N,Z from result, C from shifter, V unchanged
    // rm=0xFE means rotated immediate (C = bit 31 of imm)
    // rm=0xFF means no rotation (C preserved)
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    const u32 c = (inst->fused.rm == 0xFE) ? ((op2 >> 31) & 1) : ((old_nzcv >> 29) & 1);
    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (old_nzcv & (1u << 28));

    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedTstRegBcc: {
    // ARM: TST Rn, Rm; B<cond> label (register operand)
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = state_.regs[inst->fused.rm];
    u32 result = op1 & op2;

    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 nzcv = (n << 31) | (z << 30) | (old_nzcv & 0x30000000u);

    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedTeqBcc: {
    // ARM: TEQ Rn, #imm; B<cond> label (logical XOR, sets N,Z,C)
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = inst->fused.imm;
    u32 result = op1 ^ op2;

    // Same flag semantics and fused.rm carry convention as TST above
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    const u32 c = (inst->fused.rm == 0xFE) ? ((op2 >> 31) & 1) : ((old_nzcv >> 29) & 1);
    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (old_nzcv & (1u << 28));

    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedTeqRegBcc: {
    // ARM: TEQ Rn, Rm; B<cond> label (register operand)
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = state_.regs[inst->fused.rm];
    u32 result = op1 ^ op2;

    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 nzcv = (n << 31) | (z << 30) | (old_nzcv & 0x30000000u);

    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_ThumbFusedTstBcc: {
    // Thumb: TST Rn, Rm; B<cond> label (Thumb TST is register-only)
    u32 op1 = state_.regs[inst->fused.rn];
    u32 op2 = state_.regs[inst->fused.rm];
    u32 result = op1 & op2;

    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    const u32 n = result >> 31;
    const u32 z = (result == 0) ? 1u : 0u;
    const u32 nzcv = (n << 31) | (z << 30) | (old_nzcv & 0x30000000u);

    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;

    // Fast path for EQ (common for bit testing)
    if (inst->fused.branch_cond == 0) {
        if (z) {
            SET_PC(inst->fused.branch_target);
            goto chain;
        }
        SET_PC(GET_PC() + inst->fused.combined_size);
        DISPATCH();
    }

    if (inst->fused.branch_cond == 1) {
        if (!z) {
            SET_PC(inst->fused.branch_target);
            goto chain;
        }
        SET_PC(GET_PC() + inst->fused.combined_size);
        DISPATCH();
    }

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_FusedAddCmpBcc: {
    // ARM: ADD Rd, Rn, #imm; CMP Rd, Rm; B<cond> label (iterator loop pattern)
    // First: ADD Rd, Rn, #imm
    u32 add_result = state_.regs[inst->fused.rn] + inst->fused.imm;
    state_.regs[inst->rd] = add_result;

    // Second: CMP Rd, Rm (compare add result with rm)
    u32 op1 = add_result;
    u32 op2 = state_.regs[inst->fused.rm];
    u32 cmp_result = op1 - op2;

    // Fast path for LT (common in "i < limit" loops)
    if (inst->fused.branch_cond == 0xB) {
        bool n = (cmp_result >> 31) & 1;
        bool v = ((op1 ^ op2) & (op1 ^ cmp_result)) >> 31;
        state_.lazy_flags.op = FlagOp::Cmp;
        state_.lazy_flags.result = cmp_result;
        state_.lazy_flags.operand1 = op1;
        state_.lazy_flags.operand2 = op2;
        executed += inst->ticks;
        if (n != v) {
            SET_PC(inst->fused.branch_target);
            goto chain;
        }
        SET_PC(GET_PC() + inst->fused.combined_size);
        DISPATCH();
    }

    const u32 n = cmp_result >> 31;
    const u32 z = (cmp_result == 0) ? 1u : 0u;
    const u32 c = (op1 >= op2) ? 1u : 0u;
    const u32 v = ((op1 ^ op2) & (op1 ^ cmp_result)) >> 31;
    const u32 nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);

    state_.lazy_flags.SetPacked(nzcv);
    executed += inst->ticks;

    if (CondPassed(inst->fused.branch_cond, nzcv)) {
        SET_PC(inst->fused.branch_target);
        goto chain;
    }
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

// ============================================================================
// Unrolled Loop Handlers
// ============================================================================
// Generated by ReoptimizeHotBlock for hot blocks that are tight self-loops.

handle_ThumbUnrolledSubsLoop: {
    // Thumb: SUBS Rd, #imm8; BNE self - tight countdown loop
    // Execute up to 16 iterations inline before returning to dispatcher
    constexpr int UNROLL_COUNT = 16;

    u8 rd = inst->rd;
    u32 imm = inst->fused.imm;
    u32 value = state_.regs[rd];
    u32 last_operand1;

    // Execute iterations until result is 0 (BNE fails) or UNROLL_COUNT reached
    // Always do at least one iteration since original SUBS always executes
    int iterations = 0;
    if (imm == 1) {
        // Dominant case: closed form instead of a serial dependent-subtract
        // chain. Matches the loop exactly: result = final value, operand1 =
        // value before the last subtraction, operand2 = imm.
        iterations = (value >= 1 && value <= UNROLL_COUNT) ? static_cast<int>(value)
                                                           : UNROLL_COUNT;
        last_operand1 = value - (iterations - 1);
        value -= iterations;
    } else {
        while (iterations < UNROLL_COUNT) {
            last_operand1 = value;
            value -= imm;  // May wrap around
            iterations++;
            if (value == 0) break;  // BNE would not branch (Z flag set)
        }
    }

    state_.regs[rd] = value;
    executed += iterations * inst->ticks;  // inst->ticks = per-iteration tick cost

    state_.lazy_flags.op = FlagOp::Sub;
    state_.lazy_flags.result = value;
    state_.lazy_flags.operand1 = last_operand1;
    state_.lazy_flags.operand2 = imm;

    if (value != 0) {
        // Loop continues: re-enter this block via chain, skipping Run()
        goto chain;
    }

    // Loop finished - advance past the fused instruction
    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_ArmUnrolledSubsLoop: {
    // ARM: SUBS Rd, Rn, #imm; BNE self - tight countdown loop
    constexpr int UNROLL_COUNT = 16;

    u8 rd = inst->rd;
    u8 rn = inst->fused.rn;
    u32 imm = inst->fused.imm;
    u32 value = state_.regs[rn];
    u32 last_operand1;

    int iterations = 0;
    if (rd == rn) {
        // SUBS Rd, Rd, #imm - can unroll multiple iterations
        if (imm == 1) {
            // Dominant case: closed form instead of a serial dependent-subtract
            // chain. Matches the loop exactly: result = final value, operand1 =
            // value before the last subtraction, operand2 = imm.
            iterations = (value >= 1 && value <= UNROLL_COUNT) ? static_cast<int>(value)
                                                               : UNROLL_COUNT;
            last_operand1 = value - (iterations - 1);
            value -= iterations;
        } else {
            while (iterations < UNROLL_COUNT) {
                last_operand1 = value;
                value -= imm;  // May wrap around
                iterations++;
                if (value == 0) break;  // BNE would not branch
            }
        }
        state_.regs[rd] = value;
    } else {
        // Rd != Rn case - just do one iteration (can't chain)
        last_operand1 = value;
        value = state_.regs[rn] - imm;
        state_.regs[rd] = value;
        iterations = 1;
    }

    executed += iterations * inst->ticks;

    state_.lazy_flags.op = FlagOp::Sub;
    state_.lazy_flags.result = value;
    state_.lazy_flags.operand1 = last_operand1;
    state_.lazy_flags.operand2 = imm;

    if (value != 0) {
        // Loop continues: re-enter this block via chain, skipping Run()
        goto chain;
    }

    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}

handle_ThumbUnrolledCmpLoop: {
    // Thumb: CMP Rn, Rm; BNE self - spin/wait loop. Nothing to unroll
    // (the values only change externally); execute one iteration.

    u8 rn = inst->fused.rn;
    u8 rm = inst->fused.rm;
    u32 op1 = state_.regs[rn];
    u32 op2 = state_.regs[rm];
    u32 result = op1 - op2;

    state_.lazy_flags.op = FlagOp::Cmp;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;

    executed += inst->ticks;

    if (result != 0) {
        // Still not equal: re-enter this block via chain, skipping Run()
        goto chain;
    }

    SET_PC(GET_PC() + inst->fused.combined_size);
    DISPATCH();
}
