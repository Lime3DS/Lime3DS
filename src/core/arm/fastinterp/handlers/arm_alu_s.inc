// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// ARM ALU S-variant handlers: AndS, EorS, SubS, RsbS, AddS, AdcS, SbcS,
//                             RscS, OrrS, MovS, BicS, MvnS

// Note: ALU_DISPATCH() macro defined in arm_alu.inc handles Rd=15 branch

handle_AndS: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
    u32 op1 = state_.regs[inst->dp.rn];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ANDS(result, op1, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 & op2;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::And;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ALU_DISPATCH();
}

handle_EorS: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
    u32 op1 = state_.regs[inst->dp.rn];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_EORS(result, op1, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 ^ op2;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Eor;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ALU_DISPATCH();
}

handle_SubS: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SUBS(result, op1, op2, nzcv);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Sub;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
#endif
    ALU_DISPATCH();
}

handle_RsbS: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SUBS(result, op2, op1, nzcv);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op2 - op1;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Rsb;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
#endif
    ALU_DISPATCH();
}

handle_AddS: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ADDS(result, op1, op2, nzcv);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 + op2;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Add;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
#endif
    ALU_DISPATCH();
}

handle_AdcS: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
    u32 c = state_.lazy_flags.GetC() ? 1 : 0;
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ADCS(result, op1, op2, c, nzcv);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 + op2 + c;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Adc;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
    state_.lazy_flags.shifter_carry = c;
#endif
    ALU_DISPATCH();
}

handle_SbcS: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
    u32 c = state_.lazy_flags.GetC() ? 1 : 0;
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SBCS(result, op1, op2, c, nzcv);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2 - (1 - c);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Sbc;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
    state_.lazy_flags.shifter_carry = c;
#endif
    ALU_DISPATCH();
}

handle_RscS: {
    CHECK_COND();
    u32 op1 = state_.regs[inst->dp.rn];
    u32 op2 = GetOp2Fast();
    u32 c = state_.lazy_flags.GetC() ? 1 : 0;
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_RSCS(result, op1, op2, c, nzcv);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op2 - op1 - (1 - c);
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Rsc;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
    state_.lazy_flags.shifter_carry = c;
#endif
    ALU_DISPATCH();
}

handle_OrrS: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
    u32 op1 = state_.regs[inst->dp.rn];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ORRS(result, op1, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 | op2;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Orr;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ALU_DISPATCH();
}

handle_MovS: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_MOVS(result, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    state_.regs[inst->rd] = op2;
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = op2;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ALU_DISPATCH();
}

handle_BicS: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
    u32 op1 = state_.regs[inst->dp.rn];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_BICS(result, op1, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 & ~op2;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Bic;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ALU_DISPATCH();
}

handle_MvnS: {
    CHECK_COND();
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
    bool carry = (old_nzcv >> 29) & 1;
    u32 op2 = GetOp2(carry);
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_MVNS(result, op2, nzcv, ARM64_CV_SHIFTER(carry, old_nzcv));
    state_.regs[inst->rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = ~op2;
    state_.regs[inst->rd] = result;
    state_.lazy_flags.op = FlagOp::Mvn;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = carry ? 1 : 0;
#endif
    ALU_DISPATCH();
}
