// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thumb16 handlers, working directly on the Thumb encoding (no ARM translation).
//
// All operand fields are pre-decoded by DecodeThumb16() into:
//   inst->rd, inst->dp.rn, inst->dp.rm, inst->dp.imm, inst->dp.shift_imm,
//   inst->lsm.rn, inst->lsm.reg_list, inst->branch.target
// Immediate offsets are pre-scaled (e.g. imm5*4 for word loads).

handle_ThumbLslImm: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    const u8 imm5 = inst->dp.shift_imm;
    u32 val = state_.regs[rm];
    state_.lazy_flags.Materialize(); // preserve old C/V in nzcv before overwriting op
    bool carry;
    if (imm5 != 0) {
        carry = (val >> (32 - imm5)) & 1;
        val <<= imm5;
    } else {
        carry = state_.lazy_flags.GetC();
    }
    state_.regs[rd] = val;
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = val;
    state_.lazy_flags.shifter_carry = carry;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLsrImm: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u8 imm5 = inst->dp.shift_imm;
    if (imm5 == 0) imm5 = 32;
    u32 val = state_.regs[rm];
    state_.lazy_flags.Materialize(); // preserve old V in nzcv before overwriting op
    bool carry = (imm5 == 32) ? ((val >> 31) & 1) : ((val >> (imm5 - 1)) & 1);
    val = (imm5 == 32) ? 0 : (val >> imm5);
    state_.regs[rd] = val;
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = val;
    state_.lazy_flags.shifter_carry = carry;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAsrImm: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u8 imm5 = inst->dp.shift_imm;
    if (imm5 == 0) imm5 = 32;
    s32 val = static_cast<s32>(state_.regs[rm]);
    state_.lazy_flags.Materialize(); // preserve old V in nzcv before overwriting op
    bool carry = (imm5 >= 32) ? ((val >> 31) & 1) : ((val >> (imm5 - 1)) & 1);
    val = (imm5 >= 32) ? (val >> 31) : (val >> imm5);
    state_.regs[rd] = static_cast<u32>(val);
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = static_cast<u32>(val);
    state_.lazy_flags.shifter_carry = carry;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAddReg3: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rn];
    u32 op2 = state_.regs[rm];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ADDS(result, op1, op2, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 + op2;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Add;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbSubReg3: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rn];
    u32 op2 = state_.regs[rm];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SUBS(result, op1, op2, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Sub;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAddImm3: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm3 = inst->dp.imm;
    u32 op1 = state_.regs[rn];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ADDS(result, op1, imm3, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 + imm3;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Add;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = imm3;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbSubImm3: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm3 = inst->dp.imm;
    u32 op1 = state_.regs[rn];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SUBS(result, op1, imm3, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - imm3;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Sub;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = imm3;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbMovImm8: {
    const u8 rd = inst->rd;
    const u32 imm8 = inst->dp.imm;
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
#ifdef __aarch64__
    u32 nzcv;
    ARM64_MOVS(state_.regs[rd], imm8, nzcv, ARM64_CV_KEEP(old_nzcv));
    state_.lazy_flags.SetPacked(nzcv);
#else
    state_.regs[rd] = imm8;
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = imm8;
    state_.lazy_flags.shifter_carry = (old_nzcv >> 29) & 1;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbCmpImm8: {
    const u8 rn = inst->rd;
    const u32 imm8 = inst->dp.imm;
    u32 op1 = state_.regs[rn];
#ifdef __aarch64__
    u32 nzcv;
    ARM64_CMP(op1, imm8, nzcv);
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - imm8;
    state_.lazy_flags.op = FlagOp::Cmp;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = imm8;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAddImm8: {
    const u8 rd = inst->rd;
    const u32 imm8 = inst->dp.imm;
    u32 op1 = state_.regs[rd];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ADDS(result, op1, imm8, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 + imm8;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Add;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = imm8;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbSubImm8: {
    const u8 rd = inst->rd;
    const u32 imm8 = inst->dp.imm;
    u32 op1 = state_.regs[rd];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SUBS(result, op1, imm8, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - imm8;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Sub;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = imm8;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAnd: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rd];
    u32 op2 = state_.regs[rm];
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ANDS(result, op1, op2, nzcv, ARM64_CV_KEEP(old_nzcv));
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 & op2;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::And;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = (old_nzcv >> 29) & 1;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbEor: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rd];
    u32 op2 = state_.regs[rm];
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_EORS(result, op1, op2, nzcv, ARM64_CV_KEEP(old_nzcv));
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 ^ op2;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Eor;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = (old_nzcv >> 29) & 1;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLslReg: {
    const u8 rd = inst->rd;
    const u8 rs = inst->dp.rm;
    u32 val = state_.regs[rd];
    u8 shift = state_.regs[rs] & 0xFF;
    bool carry = state_.lazy_flags.GetC(); // materializes; V preserved in nzcv
    if (shift != 0) {
        if (shift < 32) {
            carry = (val >> (32 - shift)) & 1;
            val <<= shift;
        } else if (shift == 32) {
            carry = val & 1;
            val = 0;
        } else {
            carry = 0;
            val = 0;
        }
    }
    state_.regs[rd] = val;
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = val;
    state_.lazy_flags.shifter_carry = carry;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLsrReg: {
    const u8 rd = inst->rd;
    const u8 rs = inst->dp.rm;
    u32 val = state_.regs[rd];
    u8 shift = state_.regs[rs] & 0xFF;
    bool carry = state_.lazy_flags.GetC(); // materializes; V preserved in nzcv
    if (shift != 0) {
        if (shift < 32) {
            carry = (val >> (shift - 1)) & 1;
            val >>= shift;
        } else if (shift == 32) {
            carry = (val >> 31) & 1;
            val = 0;
        } else {
            carry = 0;
            val = 0;
        }
    }
    state_.regs[rd] = val;
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = val;
    state_.lazy_flags.shifter_carry = carry;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAsrReg: {
    const u8 rd = inst->rd;
    const u8 rs = inst->dp.rm;
    s32 val = static_cast<s32>(state_.regs[rd]);
    u8 shift = state_.regs[rs] & 0xFF;
    bool carry = state_.lazy_flags.GetC(); // materializes; V preserved in nzcv
    if (shift != 0) {
        if (shift < 32) {
            carry = (val >> (shift - 1)) & 1;
            val >>= shift;
        } else {
            carry = (val >> 31) & 1;
            val = val >> 31;
        }
    }
    state_.regs[rd] = static_cast<u32>(val);
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = static_cast<u32>(val);
    state_.lazy_flags.shifter_carry = carry;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAdc: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rd];
    u32 op2 = state_.regs[rm];
    u32 carry_in = state_.lazy_flags.GetC() ? 1 : 0;
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ADCS(result, op1, op2, carry_in, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 + op2 + carry_in;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Adc;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
    state_.lazy_flags.shifter_carry = carry_in;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbSbc: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rd];
    u32 op2 = state_.regs[rm];
    u32 carry_in = state_.lazy_flags.GetC() ? 1 : 0;
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SBCS(result, op1, op2, carry_in, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 - op2 - (1 - carry_in);
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Sbc;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
    state_.lazy_flags.shifter_carry = carry_in;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbRor: {
    const u8 rd = inst->rd;
    const u8 rs = inst->dp.rm;
    u32 val = state_.regs[rd];
    u8 shift = state_.regs[rs] & 0xFF;
    bool carry = state_.lazy_flags.GetC(); // materializes; V preserved in nzcv
    if (shift != 0) {
        shift &= 31;
        if (shift == 0) {
            carry = (val >> 31) & 1;
        } else {
            val = (val >> shift) | (val << (32 - shift));
            carry = (val >> 31) & 1;
        }
    }
    state_.regs[rd] = val;
    state_.lazy_flags.op = FlagOp::Mov;
    state_.lazy_flags.result = val;
    state_.lazy_flags.shifter_carry = carry;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbTst: {
    const u8 rn = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rn];
    u32 op2 = state_.regs[rm];
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
#ifdef __aarch64__
    u32 nzcv;
    ARM64_TST(op1, op2, nzcv, ARM64_CV_KEEP(old_nzcv));
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 & op2;
    state_.lazy_flags.op = FlagOp::And;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = (old_nzcv >> 29) & 1;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbNeg: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op2 = state_.regs[rm];
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_SUBS(result, 0, op2, nzcv);
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = 0 - op2;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Rsb;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op2;
    state_.lazy_flags.operand2 = 0;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbCmpReg: {
    const u8 rn = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rn];
    u32 op2 = state_.regs[rm];
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
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbCmn: {
    const u8 rn = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rn];
    u32 op2 = state_.regs[rm];
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
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbOrr: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rd];
    u32 op2 = state_.regs[rm];
    const u32 old_nzcv = state_.lazy_flags.GetPacked();
#ifdef __aarch64__
    u32 result, nzcv;
    ARM64_ORRS(result, op1, op2, nzcv, ARM64_CV_KEEP(old_nzcv));
    state_.regs[rd] = result;
    state_.lazy_flags.SetPacked(nzcv);
#else
    u32 result = op1 | op2;
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Orr;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = (old_nzcv >> 29) & 1;
#endif
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbMul: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 result = state_.regs[rd] * state_.regs[rm];
    state_.regs[rd] = result;
    state_.lazy_flags.Materialize();
    state_.lazy_flags.op = FlagOp::Mul;
    state_.lazy_flags.result = result;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbBic: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 result = state_.regs[rd] & ~state_.regs[rm];
    bool old_c = state_.lazy_flags.GetC(); // materializes; V preserved in nzcv
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Bic;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = old_c;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbMvn: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 result = ~state_.regs[rm];
    bool old_c = state_.lazy_flags.GetC(); // materializes; V preserved in nzcv
    state_.regs[rd] = result;
    state_.lazy_flags.op = FlagOp::Mvn;
    state_.lazy_flags.result = result;
    state_.lazy_flags.shifter_carry = old_c;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAddHi: {
    // rm==15 (ADD Rd, PC) is folded to Mov #imm at decode; rd==15 (ADD PC, Rm)
    // arrives behind a PcSetup prefix, so regs[15] holds PC+4 here.
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    state_.regs[rd] = state_.regs[rd] + state_.regs[rm];
    if (rd == 15) {
        // Branch to the just-computed value (bit 0 ignored, no mode switch)
        u32 new_pc = state_.regs[15];
        SET_PC(new_pc & ~1u);
        executed += inst->ticks;
        goto chain;
    }
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbCmpHi: {
    const u8 rn = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 op1 = state_.regs[rn];
    u32 op2 = state_.regs[rm];
    u32 result = op1 - op2;
    state_.lazy_flags.op = FlagOp::Cmp;
    state_.lazy_flags.result = result;
    state_.lazy_flags.operand1 = op1;
    state_.lazy_flags.operand2 = op2;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbMovHi: {
    // rm==15 (MOV Rd, PC) is folded to Mov #imm at decode
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    state_.regs[rd] = state_.regs[rm];
    if (rd == 15) {
        // Branch to the just-written value (bit 0 ignored, no mode switch)
        u32 new_pc = state_.regs[15];
        SET_PC(new_pc & ~1u);
        executed += inst->ticks;
        goto chain;
    }
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbBx: {
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rm];
    state_.thumb_mode = addr & 1;
    SET_PC(addr & ~1u);
    executed += inst->ticks;
    goto chain;
}

handle_ThumbBlxReg: {
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rm];
    state_.regs[14] = (GET_PC() + 2) | 1;
    state_.thumb_mode = addr & 1;
    SET_PC(addr & ~1u);
    executed += inst->ticks;
    goto chain;
}

handle_ThumbLdrReg: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    state_.regs[rd] = MemRead32(addr);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStrReg: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    MemWrite32(addr, state_.regs[rd]);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrImm5: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm5*4)
    u32 addr = state_.regs[rn] + imm;
    state_.regs[rd] = MemRead32(addr);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStrImm5: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm5*4)
    u32 addr = state_.regs[rn] + imm;
    MemWrite32(addr, state_.regs[rd]);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrbReg: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    state_.regs[rd] = MemRead8(addr);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStrbReg: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    MemWrite8(addr, static_cast<u8>(state_.regs[rd]));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrbImm5: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm = inst->dp.imm;  // Pre-decoded (byte offset, no scaling)
    u32 addr = state_.regs[rn] + imm;
    state_.regs[rd] = MemRead8(addr);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStrbImm5: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm = inst->dp.imm;  // Pre-decoded (byte offset, no scaling)
    u32 addr = state_.regs[rn] + imm;
    MemWrite8(addr, static_cast<u8>(state_.regs[rd]));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrhReg: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    state_.regs[rd] = MemRead16(addr);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStrhReg: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    MemWrite16(addr, static_cast<u16>(state_.regs[rd]));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrhImm5: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm5*2)
    u32 addr = state_.regs[rn] + imm;
    state_.regs[rd] = MemRead16(addr);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStrhImm5: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm5*2)
    u32 addr = state_.regs[rn] + imm;
    MemWrite16(addr, static_cast<u16>(state_.regs[rd]));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrsb: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    state_.regs[rd] = static_cast<u32>(static_cast<s8>(MemRead8(addr)));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrsh: {
    const u8 rd = inst->rd;
    const u8 rn = inst->dp.rn;
    const u8 rm = inst->dp.rm;
    u32 addr = state_.regs[rn] + state_.regs[rm];
    state_.regs[rd] = static_cast<u32>(static_cast<s16>(MemRead16(addr)));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrSp: {
    const u8 rd = inst->rd;
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm8*4)
    u32 addr = state_.regs[13] + imm;
    state_.regs[rd] = MemRead32(addr);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStrSp: {
    const u8 rd = inst->rd;
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm8*4)
    u32 addr = state_.regs[13] + imm;
    MemWrite32(addr, state_.regs[rd]);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdrPc: {
    const u8 rd = inst->rd;
    // dp.imm holds the absolute literal-pool address (precomputed at decode)
    state_.regs[rd] = MemRead32(inst->dp.imm);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAddPcImm: {
    const u8 rd = inst->rd;
    // dp.imm holds the final ADR value (precomputed at decode)
    state_.regs[rd] = inst->dp.imm;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAddSpImm: {
    const u8 rd = inst->rd;
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm8*4)
    state_.regs[rd] = state_.regs[13] + imm;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbAddSpImm7: {
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm7*4)
    state_.regs[13] = state_.regs[13] + imm;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbSubSpImm7: {
    const u32 imm = inst->dp.imm;  // Pre-scaled by decoder (imm7*4)
    state_.regs[13] = state_.regs[13] - imm;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbPush: {
    u16 reglist = inst->lsm.reg_list;  // LR bit already included by decoder
    // count/base_off/wb_delta (== -count*4) precomputed at decode
    u32 addr = state_.regs[13] + inst->lsm.base_off;
    state_.regs[13] = addr;
    u32 end_addr = addr + inst->lsm.count * 4 - 4;

    // Fast path: if all accesses are on the same mapped page, use direct pointer
    u8* base_page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
    bool same_page = (addr >> FASTINTERP_PAGE_BITS) == (end_addr >> FASTINTERP_PAGE_BITS);

    if (__builtin_expect(base_page != nullptr && same_page, 1)) {
        for (int i = 0; i < 16; i++) {
            if (reglist & (1 << i)) {
                u32 val = state_.regs[i];
                std::memcpy(&base_page[addr & FASTINTERP_PAGE_MASK], &val, 4);
                addr += 4;
            }
        }
    } else {
        for (int i = 0; i < 16; i++) {
            if (reglist & (1 << i)) {
                MemWrite32(addr, state_.regs[i]);
                addr += 4;
            }
        }
    }
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbPop: {
    u16 reglist = inst->lsm.reg_list;  // PC bit already included by decoder
    // count/wb_delta (== +count*4) precomputed at decode; base_off is 0
    u32 base = state_.regs[13];
    u32 addr = base;
    u32 end_addr = addr + inst->lsm.count * 4 - 4;

    // Fast path: if all accesses are on the same mapped page, use direct pointer
    const u8* base_page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
    bool same_page = (addr >> FASTINTERP_PAGE_BITS) == (end_addr >> FASTINTERP_PAGE_BITS);

    if (__builtin_expect(base_page != nullptr && same_page, 1)) {
        for (int i = 0; i < 16; i++) {
            if (reglist & (1 << i)) {
                u32 loaded_val;
                std::memcpy(&loaded_val, &base_page[addr & FASTINTERP_PAGE_MASK], 4);
                state_.regs[i] = loaded_val;
                addr += 4;
            }
        }
    } else {
        for (int i = 0; i < 16; i++) {
            if (reglist & (1 << i)) {
                state_.regs[i] = MemRead32(addr);
                addr += 4;
            }
        }
    }
    state_.regs[13] = base + inst->lsm.wb_delta;
    if (reglist & (1 << 15)) {
        u32 new_pc = state_.regs[15];
        state_.thumb_mode = new_pc & 1;
        SET_PC(new_pc & ~1u);
        executed += inst->ticks;
        goto chain;
    }
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbLdmia: {
    const u8 rn = inst->lsm.rn;
    const u8 reglist = inst->lsm.reg_list;
    // count/wb_delta (== +count*4) precomputed at decode; base_off is 0
    u32 base = state_.regs[rn];
    u32 addr = base;
    u32 end_addr = addr + inst->lsm.count * 4 - 4;

    // Fast path: if all accesses are on the same mapped page, use direct pointer
    const u8* base_page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
    bool same_page = (addr >> FASTINTERP_PAGE_BITS) == (end_addr >> FASTINTERP_PAGE_BITS);

    if (__builtin_expect(base_page != nullptr && same_page, 1)) {
        for (int i = 0; i < 8; i++) {
            if (reglist & (1 << i)) {
                u32 loaded_val;
                std::memcpy(&loaded_val, &base_page[addr & FASTINTERP_PAGE_MASK], 4);
                state_.regs[i] = loaded_val;
                addr += 4;
            }
        }
    } else {
        for (int i = 0; i < 8; i++) {
            if (reglist & (1 << i)) {
                state_.regs[i] = MemRead32(addr);
                addr += 4;
            }
        }
    }
    // Writeback if Rn not in reglist
    if (!(reglist & (1 << rn))) {
        state_.regs[rn] = base + inst->lsm.wb_delta;
    }
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbStmia: {
    const u8 rn = inst->lsm.rn;
    const u8 reglist = inst->lsm.reg_list;
    // count/wb_delta (== +count*4) precomputed at decode; base_off is 0
    u32 base = state_.regs[rn];
    u32 addr = base;
    u32 end_addr = addr + inst->lsm.count * 4 - 4;

    // Fast path: if all accesses are on the same mapped page, use direct pointer
    u8* base_page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS];
    bool same_page = (addr >> FASTINTERP_PAGE_BITS) == (end_addr >> FASTINTERP_PAGE_BITS);

    if (__builtin_expect(base_page != nullptr && same_page, 1)) {
        for (int i = 0; i < 8; i++) {
            if (reglist & (1 << i)) {
                u32 val = state_.regs[i];
                std::memcpy(&base_page[addr & FASTINTERP_PAGE_MASK], &val, 4);
                addr += 4;
            }
        }
    } else {
        for (int i = 0; i < 8; i++) {
            if (reglist & (1 << i)) {
                MemWrite32(addr, state_.regs[i]);
                addr += 4;
            }
        }
    }
    state_.regs[rn] = base + inst->lsm.wb_delta;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbBCond: {
    if (!CheckCondition(inst->cond)) {
        executed += inst->ticks;
        SET_PC(GET_PC() + 2);
        DISPATCH();
    }
    SET_PC(inst->branch.target); // Absolute target precomputed at decode
    executed += inst->ticks;
    goto chain;
}

handle_ThumbB: {
    SET_PC(inst->branch.target); // Absolute target precomputed at decode
    executed += inst->ticks;
    goto chain;
}

handle_ThumbSwi: {
    // Same tick-flush and context-reload dance as handle_Swi (arm_misc.inc)
    SET_PC(GET_PC() + inst->inst_size);
    SYNC_PC();
    {
        u32 svc_num = inst->imm32 & 0xFF;
        executed += inst->ticks;
        timer->AddTicks(executed);
        executed = 0;

        svc_context_->CallSVC(svc_num);
    }
    local_pc = state_.regs[15];
    goto done;
}

handle_ThumbSxth: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    state_.regs[rd] = static_cast<u32>(static_cast<s16>(state_.regs[rm] & 0xFFFF));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbSxtb: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    state_.regs[rd] = static_cast<u32>(static_cast<s8>(state_.regs[rm] & 0xFF));
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbUxth: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    state_.regs[rd] = state_.regs[rm] & 0xFFFF;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbUxtb: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    state_.regs[rd] = state_.regs[rm] & 0xFF;
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbRev: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 val = state_.regs[rm];
    state_.regs[rd] = ((val & 0xFF) << 24) | ((val & 0xFF00) << 8) |
                      ((val >> 8) & 0xFF00) | ((val >> 24) & 0xFF);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbRev16: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 val = state_.regs[rm];
    state_.regs[rd] = ((val & 0x00FF00FF) << 8) | ((val >> 8) & 0x00FF00FF);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}

handle_ThumbRevsh: {
    const u8 rd = inst->rd;
    const u8 rm = inst->dp.rm;
    u32 val = state_.regs[rm];
    s16 result = static_cast<s16>(((val & 0xFF) << 8) | ((val >> 8) & 0xFF));
    state_.regs[rd] = static_cast<u32>(result);
    executed += inst->ticks;
    SET_PC(GET_PC() + 2);
    DISPATCH();
}
