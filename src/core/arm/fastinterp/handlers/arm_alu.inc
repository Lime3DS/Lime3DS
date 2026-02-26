// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// ARM ALU handlers, non-S-variant: And, Eor, Sub, Rsb, Add, Adc, Sbc, Rsc, Orr,
//     Mov, Bic, Mvn (no flag updates), plus the Tst, Teq, Cmp, Cmn compares
//     (which have no non-S form and always update flags)

// When an ALU instruction writes to PC (R15), treat as a branch
#define ALU_DISPATCH() do { \
    if (__builtin_expect(inst->rd == 15, 0)) { \
        u32 new_pc = state_.regs[15]; \
        state_.thumb_mode = new_pc & 1; \
        SET_PC(new_pc & ~1u); \
        executed += inst->ticks; \
        goto chain; \
    } \
    ADVANCE_AND_DISPATCH(); \
} while(0)

handle_And: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] & op2;
    ALU_DISPATCH();
}

handle_Eor: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] ^ op2;
    ALU_DISPATCH();
}

handle_Sub: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] - op2;
    ALU_DISPATCH();
}

handle_Rsb: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = op2 - state_.regs[inst->dp.rn];
    ALU_DISPATCH();
}

handle_Add: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] + op2;
    ALU_DISPATCH();
}

handle_Adc: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    u32 c = state_.lazy_flags.GetC() ? 1 : 0;
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] + op2 + c;
    ALU_DISPATCH();
}

handle_Sbc: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    u32 c = state_.lazy_flags.GetC() ? 1 : 0;
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] - op2 - (1 - c);
    ALU_DISPATCH();
}

handle_Rsc: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    u32 c = state_.lazy_flags.GetC() ? 1 : 0;
    state_.regs[inst->rd] = op2 - state_.regs[inst->dp.rn] - (1 - c);
    ALU_DISPATCH();
}

handle_Tst: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
    u32 op1 = state_.regs[inst->dp.rn];
#ifdef __aarch64__
    u32 nzcv;
    ARM64_TST(op1, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 & op2;
    state_.lazy_flags.op = FlagOp::And;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ADVANCE_AND_DISPATCH();
}

handle_Teq: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
    u32 op1 = state_.regs[inst->dp.rn];
#ifdef __aarch64__
    u32 nzcv;
    ARM64_TEQ(op1, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 ^ op2;
    state_.lazy_flags.op = FlagOp::Eor;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ADVANCE_AND_DISPATCH();
}

handle_Cmp: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
#ifdef __aarch64__
    u32 nzcv;
    ARM64_CMP(op1, op2, nzcv);
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2;
    state_.lazy_flags.op = FlagOp::Cmp;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
#endif
    ADVANCE_AND_DISPATCH();
}

handle_Cmn: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
#ifdef __aarch64__
    u32 nzcv;
    ARM64_CMN(op1, op2, nzcv);
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 + op2;
    state_.lazy_flags.op = FlagOp::Cmn;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
#endif
    ADVANCE_AND_DISPATCH();
}

handle_Orr: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] | op2;
    ALU_DISPATCH();
}

handle_Mov: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = op2;
    ALU_DISPATCH();
}

handle_Bic: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] & ~op2;
    ALU_DISPATCH();
}

handle_Mvn: {
    CHECK_COND();
    u32 op2 = GetOp2Fast();
    state_.regs[inst->rd] = ~op2;
    ALU_DISPATCH();
}
