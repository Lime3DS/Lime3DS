// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// ARM Miscellaneous handlers: Clz, Swi, Mrs, Msr, sign/zero extend (Sxtb,
//     Sxth, Uxtb, Uxth), byte reverse (Rev, Rev16, Revsh), saturating add/sub
//     (Qadd, Qsub, Qdadd, Qdsub), Ssat, Usat, parallel add/sub (Sadd16..Uhsub8),
//     extend-and-add (Sxtab, Sxtab16, Sxtah, Uxtab, Uxtab16, Uxtah), bitfield
//     (Bfc, Bfi, Sbfx, Ubfx), Movt, Rbit, Pkhbt, Pkhtb, Sel, Usad8, Usada8,
//     synchronization (Ldrex/Strex{,b,h,d}, Clrex), and coprocessor (Mrc, Mcr)

handle_Clz: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    state_.regs[inst->rd] = val ? __builtin_clz(val) : 32;
    ADVANCE_AND_DISPATCH();
}

handle_Swi: {
    CHECK_COND();
    // Software interrupt - dispatch to kernel SVC handler
    SET_PC(GET_PC() + inst->inst_size);
    SYNC_PC();
    {
        u32 svc_num = inst->imm32 & 0xFF;
        // Flush accumulated ticks to timer before kernel call so the
        // kernel sees correct time state for scheduling decisions.
        executed += inst->ticks;
        timer->AddTicks(executed);
        executed = 0;

        svc_context_->CallSVC(svc_num);
    }
    // Kernel may have switched thread context via LoadContext,
    // which changes state_.regs[15]. Reload local_pc and exit the
    // block (context may have changed). Don't halt - PrepareReschedule
    // will set halted_ if the kernel needs a reschedule.
    local_pc = state_.regs[15];
    goto done;
}

handle_Mrs: {
    CHECK_COND();
    state_.regs[inst->rd] = state_.GetCPSR();
    ADVANCE_AND_DISPATCH();
}

handle_Msr: {
    CHECK_COND();
    u32 value = (inst->dp.rm != 0xFF) ? state_.regs[inst->dp.rm] : inst->dp.imm;
    // Only update allowed fields (based on mask in flags)
    u32 mask = 0;
    if (inst->dp.flags & 0x01) mask |= 0x000000FF;  // Control
    if (inst->dp.flags & 0x02) mask |= 0x0000FF00;  // Extension
    if (inst->dp.flags & 0x04) mask |= 0x00FF0000;  // Status
    if (inst->dp.flags & 0x08) mask |= 0xFF000000;  // Flags
    state_.SetCPSR((state_.GetCPSR() & ~mask) | (value & mask));
    ADVANCE_AND_DISPATCH();
}

// ----------------------------------------------------------------------------
// Extension Instructions
// ----------------------------------------------------------------------------

handle_Sxtb: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    u8 rotation = inst->dp.shift_imm;
    val = std::rotr(val, rotation);
    state_.regs[inst->rd] = static_cast<u32>(static_cast<s8>(val));
    ADVANCE_AND_DISPATCH();
}

handle_Sxth: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    u8 rotation = inst->dp.shift_imm;
    val = std::rotr(val, rotation);
    state_.regs[inst->rd] = static_cast<u32>(static_cast<s16>(val));
    ADVANCE_AND_DISPATCH();
}

handle_Uxtb: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    u8 rotation = inst->dp.shift_imm;
    val = std::rotr(val, rotation);
    state_.regs[inst->rd] = val & 0xFF;
    ADVANCE_AND_DISPATCH();
}

handle_Uxth: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    u8 rotation = inst->dp.shift_imm;
    val = std::rotr(val, rotation);
    state_.regs[inst->rd] = val & 0xFFFF;
    ADVANCE_AND_DISPATCH();
}

// ----------------------------------------------------------------------------
// Byte Reversal
// ----------------------------------------------------------------------------

handle_Rev: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    state_.regs[inst->rd] = __builtin_bswap32(val);
    ADVANCE_AND_DISPATCH();
}

handle_Rev16: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    state_.regs[inst->rd] = ((val & 0xFF00FF00) >> 8) | ((val & 0x00FF00FF) << 8);
    ADVANCE_AND_DISPATCH();
}

handle_Revsh: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    u16 swapped = ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
    state_.regs[inst->rd] = static_cast<u32>(static_cast<s16>(swapped));
    ADVANCE_AND_DISPATCH();
}

// ----------------------------------------------------------------------------
// Saturating Arithmetic
// ----------------------------------------------------------------------------

handle_Qadd: {
    // QADD Rd, Rm, Rn - Signed saturating add
    CHECK_COND();
    s32 rm = static_cast<s32>(state_.regs[inst->dp.rm]);
    s32 rn = static_cast<s32>(state_.regs[inst->dp.rn]);
    s64 result = static_cast<s64>(rm) + static_cast<s64>(rn);
    if (result > INT32_MAX) {
        state_.regs[inst->rd] = INT32_MAX;
        state_.cpsr_other |= (1 << 27);  // Set Q flag
    } else if (result < INT32_MIN) {
        state_.regs[inst->rd] = INT32_MIN;
        state_.cpsr_other |= (1 << 27);  // Set Q flag
    } else {
        state_.regs[inst->rd] = static_cast<u32>(result);
    }
    ADVANCE_AND_DISPATCH();
}

handle_Qsub: {
    // QSUB Rd, Rm, Rn - Signed saturating subtract
    CHECK_COND();
    s32 rm = static_cast<s32>(state_.regs[inst->dp.rm]);
    s32 rn = static_cast<s32>(state_.regs[inst->dp.rn]);
    s64 result = static_cast<s64>(rm) - static_cast<s64>(rn);
    if (result > INT32_MAX) {
        state_.regs[inst->rd] = INT32_MAX;
        state_.cpsr_other |= (1 << 27);  // Set Q flag
    } else if (result < INT32_MIN) {
        state_.regs[inst->rd] = INT32_MIN;
        state_.cpsr_other |= (1 << 27);  // Set Q flag
    } else {
        state_.regs[inst->rd] = static_cast<u32>(result);
    }
    ADVANCE_AND_DISPATCH();
}

handle_Qdadd: {
    // QDADD Rd, Rm, Rn - Signed saturating double and add
    // Rd = saturate(Rm + saturate(Rn * 2))
    CHECK_COND();
    s32 rm = static_cast<s32>(state_.regs[inst->dp.rm]);
    s32 rn = static_cast<s32>(state_.regs[inst->dp.rn]);
    // Double Rn with saturation
    s64 doubled = static_cast<s64>(rn) * 2;
    s32 sat_doubled;
    if (doubled > INT32_MAX) {
        sat_doubled = INT32_MAX;
        state_.cpsr_other |= (1 << 27);
    } else if (doubled < INT32_MIN) {
        sat_doubled = INT32_MIN;
        state_.cpsr_other |= (1 << 27);
    } else {
        sat_doubled = static_cast<s32>(doubled);
    }
    // Add with saturation
    s64 result = static_cast<s64>(rm) + static_cast<s64>(sat_doubled);
    if (result > INT32_MAX) {
        state_.regs[inst->rd] = INT32_MAX;
        state_.cpsr_other |= (1 << 27);
    } else if (result < INT32_MIN) {
        state_.regs[inst->rd] = INT32_MIN;
        state_.cpsr_other |= (1 << 27);
    } else {
        state_.regs[inst->rd] = static_cast<u32>(result);
    }
    ADVANCE_AND_DISPATCH();
}

handle_Qdsub: {
    // QDSUB Rd, Rm, Rn - Signed saturating double and subtract
    // Rd = saturate(Rm - saturate(Rn * 2))
    CHECK_COND();
    s32 rm = static_cast<s32>(state_.regs[inst->dp.rm]);
    s32 rn = static_cast<s32>(state_.regs[inst->dp.rn]);
    // Double Rn with saturation
    s64 doubled = static_cast<s64>(rn) * 2;
    s32 sat_doubled;
    if (doubled > INT32_MAX) {
        sat_doubled = INT32_MAX;
        state_.cpsr_other |= (1 << 27);
    } else if (doubled < INT32_MIN) {
        sat_doubled = INT32_MIN;
        state_.cpsr_other |= (1 << 27);
    } else {
        sat_doubled = static_cast<s32>(doubled);
    }
    // Subtract with saturation
    s64 result = static_cast<s64>(rm) - static_cast<s64>(sat_doubled);
    if (result > INT32_MAX) {
        state_.regs[inst->rd] = INT32_MAX;
        state_.cpsr_other |= (1 << 27);
    } else if (result < INT32_MIN) {
        state_.regs[inst->rd] = INT32_MIN;
        state_.cpsr_other |= (1 << 27);
    } else {
        state_.regs[inst->rd] = static_cast<u32>(result);
    }
    ADVANCE_AND_DISPATCH();
}

handle_Ssat: {
    // SSAT Rd, #sat, Rm{, shift}
    // Saturate signed value to sat bits (-2^(sat-1) to 2^(sat-1)-1)
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 shift_amt = inst->dp.shift_imm;
    s32 shifted;
    if (inst->dp.shift_type == ShiftType::ASR) {
        // ASR #0 encodes ASR #32; >> 31 sign-fills identically without the
        // undefined shift-by-32
        if (shift_amt == 0) shift_amt = 31;
        shifted = static_cast<s32>(rm_val) >> shift_amt;
    } else {
        // LSL
        shifted = static_cast<s32>(rm_val << shift_amt);
    }
    u32 sat_bits = inst->dp.imm; // Already decoded as 1-32
    // 64-bit bounds: sat_bits == 32 would overflow the 32-bit shift
    s64 max_val = (s64(1) << (sat_bits - 1)) - 1;
    s64 min_val = -(s64(1) << (sat_bits - 1));
    if (shifted > max_val) {
        state_.regs[inst->rd] = static_cast<u32>(max_val);
        state_.cpsr_other |= (1 << 27);  // Q flag
    } else if (shifted < min_val) {
        state_.regs[inst->rd] = static_cast<u32>(min_val);
        state_.cpsr_other |= (1 << 27);  // Q flag
    } else {
        state_.regs[inst->rd] = static_cast<u32>(shifted);
    }
    ADVANCE_AND_DISPATCH();
}

handle_Usat: {
    // USAT Rd, #sat, Rm{, shift}
    // Saturate to unsigned value to sat bits (0 to 2^sat-1)
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 shift_amt = inst->dp.shift_imm;
    s32 shifted;
    if (inst->dp.shift_type == ShiftType::ASR) {
        // ASR #0 encodes ASR #32; >> 31 sign-fills identically without the
        // undefined shift-by-32
        if (shift_amt == 0) shift_amt = 31;
        shifted = static_cast<s32>(rm_val) >> shift_amt;
    } else {
        // LSL
        shifted = static_cast<s32>(rm_val << shift_amt);
    }
    u32 sat_bits = inst->dp.imm; // 0-31
    u32 max_val = sat_bits == 0 ? 0 : ((1u << sat_bits) - 1);
    if (shifted < 0) {
        state_.regs[inst->rd] = 0;
        state_.cpsr_other |= (1 << 27);  // Q flag
    } else if (static_cast<u32>(shifted) > max_val) {
        state_.regs[inst->rd] = max_val;
        state_.cpsr_other |= (1 << 27);  // Q flag
    } else {
        state_.regs[inst->rd] = static_cast<u32>(shifted);
    }
    ADVANCE_AND_DISPATCH();
}

// ----------------------------------------------------------------------------
// Parallel Add/Sub (set GE flags in CPSR[19:16])
// ----------------------------------------------------------------------------

// GE flag helpers: CPSR bits [19:16]
#define SET_GE(val) (state_.cpsr_other = (state_.cpsr_other & ~0x000F0000u) | (((val) & 0xF) << 16))
#define GET_GE() ((state_.cpsr_other >> 16) & 0xF)

handle_Sadd16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    s32 lo = static_cast<s16>(rn) + static_cast<s16>(rm);
    s32 hi = static_cast<s16>(rn >> 16) + static_cast<s16>(rm >> 16);
    state_.regs[inst->rd] = (static_cast<u32>(static_cast<u16>(lo))) |
                            (static_cast<u32>(static_cast<u16>(hi)) << 16);
    u32 ge = 0;
    if (lo >= 0) ge |= 0x3;
    if (hi >= 0) ge |= 0xC;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

handle_Ssub16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    s32 lo = static_cast<s16>(rn) - static_cast<s16>(rm);
    s32 hi = static_cast<s16>(rn >> 16) - static_cast<s16>(rm >> 16);
    state_.regs[inst->rd] = (static_cast<u32>(static_cast<u16>(lo))) |
                            (static_cast<u32>(static_cast<u16>(hi)) << 16);
    u32 ge = 0;
    if (lo >= 0) ge |= 0x3;
    if (hi >= 0) ge |= 0xC;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

handle_Sadd8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    u32 ge = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = static_cast<s8>((rn >> (i * 8)) & 0xFF);
        s32 b = static_cast<s8>((rm >> (i * 8)) & 0xFF);
        s32 sum = a + b;
        result |= (static_cast<u32>(static_cast<u8>(sum)) << (i * 8));
        if (sum >= 0) ge |= (1u << i);
    }
    state_.regs[inst->rd] = result;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

handle_Ssub8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    u32 ge = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = static_cast<s8>((rn >> (i * 8)) & 0xFF);
        s32 b = static_cast<s8>((rm >> (i * 8)) & 0xFF);
        s32 diff = a - b;
        result |= (static_cast<u32>(static_cast<u8>(diff)) << (i * 8));
        if (diff >= 0) ge |= (1u << i);
    }
    state_.regs[inst->rd] = result;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

handle_Uadd16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 lo = (rn & 0xFFFF) + (rm & 0xFFFF);
    u32 hi = ((rn >> 16) & 0xFFFF) + ((rm >> 16) & 0xFFFF);
    state_.regs[inst->rd] = (lo & 0xFFFF) | ((hi & 0xFFFF) << 16);
    u32 ge = 0;
    if (lo >= 0x10000) ge |= 0x3;
    if (hi >= 0x10000) ge |= 0xC;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

handle_Usub16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 rn_lo = rn & 0xFFFF, rm_lo = rm & 0xFFFF;
    u32 rn_hi = (rn >> 16) & 0xFFFF, rm_hi = (rm >> 16) & 0xFFFF;
    state_.regs[inst->rd] = ((rn_lo - rm_lo) & 0xFFFF) | (((rn_hi - rm_hi) & 0xFFFF) << 16);
    u32 ge = 0;
    if (rn_lo >= rm_lo) ge |= 0x3;
    if (rn_hi >= rm_hi) ge |= 0xC;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

handle_Uadd8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    u32 ge = 0;
    for (int i = 0; i < 4; i++) {
        u32 a = (rn >> (i * 8)) & 0xFF;
        u32 b = (rm >> (i * 8)) & 0xFF;
        u32 sum = a + b;
        result |= ((sum & 0xFF) << (i * 8));
        if (sum >= 0x100) ge |= (1u << i);
    }
    state_.regs[inst->rd] = result;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

handle_Usub8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    u32 ge = 0;
    for (int i = 0; i < 4; i++) {
        u32 a = (rn >> (i * 8)) & 0xFF;
        u32 b = (rm >> (i * 8)) & 0xFF;
        result |= (((a - b) & 0xFF) << (i * 8));
        if (a >= b) ge |= (1u << i);
    }
    state_.regs[inst->rd] = result;
    SET_GE(ge);
    ADVANCE_AND_DISPATCH();
}

// Unsigned saturating parallel operations
handle_Uqadd16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 lo = (rn & 0xFFFF) + (rm & 0xFFFF);
    u32 hi = ((rn >> 16) & 0xFFFF) + ((rm >> 16) & 0xFFFF);
    if (lo > 0xFFFF) lo = 0xFFFF;
    if (hi > 0xFFFF) hi = 0xFFFF;
    state_.regs[inst->rd] = (lo & 0xFFFF) | (hi << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Uqsub16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 lo_rn = rn & 0xFFFF, lo_rm = rm & 0xFFFF;
    u32 hi_rn = (rn >> 16) & 0xFFFF, hi_rm = (rm >> 16) & 0xFFFF;
    u32 lo = lo_rn > lo_rm ? lo_rn - lo_rm : 0;
    u32 hi = hi_rn > hi_rm ? hi_rn - hi_rm : 0;
    state_.regs[inst->rd] = (lo & 0xFFFF) | (hi << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Uqadd8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        u32 a = (rn >> (i * 8)) & 0xFF;
        u32 b = (rm >> (i * 8)) & 0xFF;
        u32 sum = a + b;
        if (sum > 0xFF) sum = 0xFF;
        result |= (sum << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_Uqsub8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        u32 a = (rn >> (i * 8)) & 0xFF;
        u32 b = (rm >> (i * 8)) & 0xFF;
        u32 diff = a > b ? a - b : 0;
        result |= (diff << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

// Signed saturating parallel operations
handle_Qadd16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    s32 lo = static_cast<s16>(rn) + static_cast<s16>(rm);
    s32 hi = static_cast<s16>(rn >> 16) + static_cast<s16>(rm >> 16);
    if (lo > 32767) lo = 32767; else if (lo < -32768) lo = -32768;
    if (hi > 32767) hi = 32767; else if (hi < -32768) hi = -32768;
    state_.regs[inst->rd] = (static_cast<u32>(static_cast<u16>(lo))) |
                            (static_cast<u32>(static_cast<u16>(hi)) << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Qsub16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    s32 lo = static_cast<s16>(rn) - static_cast<s16>(rm);
    s32 hi = static_cast<s16>(rn >> 16) - static_cast<s16>(rm >> 16);
    if (lo > 32767) lo = 32767; else if (lo < -32768) lo = -32768;
    if (hi > 32767) hi = 32767; else if (hi < -32768) hi = -32768;
    state_.regs[inst->rd] = (static_cast<u32>(static_cast<u16>(lo))) |
                            (static_cast<u32>(static_cast<u16>(hi)) << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Qadd8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = static_cast<s8>((rn >> (i * 8)) & 0xFF);
        s32 b = static_cast<s8>((rm >> (i * 8)) & 0xFF);
        s32 sum = a + b;
        if (sum > 127) sum = 127; else if (sum < -128) sum = -128;
        result |= (static_cast<u32>(static_cast<u8>(sum)) << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_Qsub8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = static_cast<s8>((rn >> (i * 8)) & 0xFF);
        s32 b = static_cast<s8>((rm >> (i * 8)) & 0xFF);
        s32 diff = a - b;
        if (diff > 127) diff = 127; else if (diff < -128) diff = -128;
        result |= (static_cast<u32>(static_cast<u8>(diff)) << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

// Signed halving parallel operations
handle_Shadd16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    s32 lo = (static_cast<s16>(rn) + static_cast<s16>(rm)) >> 1;
    s32 hi = (static_cast<s16>(rn >> 16) + static_cast<s16>(rm >> 16)) >> 1;
    state_.regs[inst->rd] = (static_cast<u32>(static_cast<u16>(lo))) |
                            (static_cast<u32>(static_cast<u16>(hi)) << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Shsub16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    s32 lo = (static_cast<s16>(rn) - static_cast<s16>(rm)) >> 1;
    s32 hi = (static_cast<s16>(rn >> 16) - static_cast<s16>(rm >> 16)) >> 1;
    state_.regs[inst->rd] = (static_cast<u32>(static_cast<u16>(lo))) |
                            (static_cast<u32>(static_cast<u16>(hi)) << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Shadd8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = static_cast<s8>((rn >> (i * 8)) & 0xFF);
        s32 b = static_cast<s8>((rm >> (i * 8)) & 0xFF);
        result |= (static_cast<u32>(static_cast<u8>((a + b) >> 1)) << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_Shsub8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = static_cast<s8>((rn >> (i * 8)) & 0xFF);
        s32 b = static_cast<s8>((rm >> (i * 8)) & 0xFF);
        result |= (static_cast<u32>(static_cast<u8>((a - b) >> 1)) << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

// Unsigned halving parallel operations
handle_Uhadd16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 lo = ((rn & 0xFFFF) + (rm & 0xFFFF)) >> 1;
    u32 hi = (((rn >> 16) & 0xFFFF) + ((rm >> 16) & 0xFFFF)) >> 1;
    state_.regs[inst->rd] = (lo & 0xFFFF) | (hi << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Uhsub16: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 lo = ((rn & 0xFFFF) - (rm & 0xFFFF)) >> 1;
    u32 hi = (((rn >> 16) & 0xFFFF) - ((rm >> 16) & 0xFFFF)) >> 1;
    state_.regs[inst->rd] = (lo & 0xFFFF) | (hi << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Uhadd8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        u32 a = (rn >> (i * 8)) & 0xFF;
        u32 b = (rm >> (i * 8)) & 0xFF;
        result |= (((a + b) >> 1) << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

handle_Uhsub8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        u32 a = (rn >> (i * 8)) & 0xFF;
        u32 b = (rm >> (i * 8)) & 0xFF;
        result |= (((a - b) >> 1) << (i * 8));
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

// Extend with Add operations
handle_Sxtab: {
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 rotated = std::rotr(rm_val, inst->dp.shift_imm);
    s32 extended = static_cast<s8>(rotated & 0xFF);
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] + static_cast<u32>(extended);
    ADVANCE_AND_DISPATCH();
}

handle_Sxtab16: {
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 rotated = std::rotr(rm_val, inst->dp.shift_imm);
    // Rn=15 encodes SXTB16 (no add)
    u32 rn_val = (inst->dp.rn == 15) ? 0 : state_.regs[inst->dp.rn];
    u32 lo = (rn_val & 0xFFFF) + static_cast<u32>(static_cast<u16>(static_cast<s16>(static_cast<s8>(rotated & 0xFF))));
    u32 hi = ((rn_val >> 16) & 0xFFFF) + static_cast<u32>(static_cast<u16>(static_cast<s16>(static_cast<s8>((rotated >> 16) & 0xFF))));
    state_.regs[inst->rd] = (lo & 0xFFFF) | (hi << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Sxtah: {
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 rotated = std::rotr(rm_val, inst->dp.shift_imm);
    s32 extended = static_cast<s16>(rotated & 0xFFFF);
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] + static_cast<u32>(extended);
    ADVANCE_AND_DISPATCH();
}

handle_Uxtab: {
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 rotated = std::rotr(rm_val, inst->dp.shift_imm);
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] + (rotated & 0xFF);
    ADVANCE_AND_DISPATCH();
}

handle_Uxtab16: {
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 rotated = std::rotr(rm_val, inst->dp.shift_imm);
    // Rn=15 encodes UXTB16 (no add)
    u32 rn_val = (inst->dp.rn == 15) ? 0 : state_.regs[inst->dp.rn];
    u32 lo = (rn_val & 0xFFFF) + (rotated & 0xFF);
    u32 hi = ((rn_val >> 16) & 0xFFFF) + ((rotated >> 16) & 0xFF);
    state_.regs[inst->rd] = (lo & 0xFFFF) | (hi << 16);
    ADVANCE_AND_DISPATCH();
}

handle_Uxtah: {
    CHECK_COND();
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 rotated = std::rotr(rm_val, inst->dp.shift_imm);
    state_.regs[inst->rd] = state_.regs[inst->dp.rn] + (rotated & 0xFFFF);
    ADVANCE_AND_DISPATCH();
}

// Bit field operations
handle_Bfc: {
    CHECK_COND();
    u32 lsb = inst->dp.imm;
    u32 width = inst->dp.shift_imm;
    // width is 1-32: shift the all-ones mask down instead of 1<<32
    u32 mask = (0xFFFFFFFFu >> (32 - width)) << lsb;
    state_.regs[inst->rd] &= ~mask;
    ADVANCE_AND_DISPATCH();
}

handle_Bfi: {
    CHECK_COND();
    u32 lsb = inst->dp.imm;
    u32 width = inst->dp.shift_imm;
    u32 mask = (0xFFFFFFFFu >> (32 - width)) << lsb;
    u32 src = state_.regs[inst->dp.rn];
    state_.regs[inst->rd] = (state_.regs[inst->rd] & ~mask) | ((src << lsb) & mask);
    ADVANCE_AND_DISPATCH();
}

handle_Sbfx: {
    CHECK_COND();
    u32 lsb = inst->dp.imm;
    u32 width = inst->dp.shift_imm;
    u32 src = state_.regs[inst->dp.rn];
    u32 mask = 0xFFFFFFFFu >> (32 - width); // width is 1-32
    u32 field = (src >> lsb) & mask;
    // Sign extend
    if (field & (1u << (width - 1))) {
        field |= ~mask;
    }
    state_.regs[inst->rd] = field;
    ADVANCE_AND_DISPATCH();
}

handle_Ubfx: {
    CHECK_COND();
    u32 lsb = inst->dp.imm;
    u32 width = inst->dp.shift_imm;
    u32 src = state_.regs[inst->dp.rn];
    state_.regs[inst->rd] = (src >> lsb) & (0xFFFFFFFFu >> (32 - width));
    ADVANCE_AND_DISPATCH();
}

// MOVT - Move Top (set upper 16 bits, preserve lower 16)
handle_Movt: {
    CHECK_COND();
    u32 imm16 = inst->dp.imm;
    state_.regs[inst->rd] = (state_.regs[inst->rd] & 0xFFFF) | (imm16 << 16);
    ADVANCE_AND_DISPATCH();
}

// RBIT - Reverse bits
handle_Rbit: {
    CHECK_COND();
    u32 val = state_.regs[inst->dp.rm];
    val = ((val & 0x55555555) << 1) | ((val & 0xAAAAAAAA) >> 1);
    val = ((val & 0x33333333) << 2) | ((val & 0xCCCCCCCC) >> 2);
    val = ((val & 0x0F0F0F0F) << 4) | ((val & 0xF0F0F0F0) >> 4);
    val = ((val & 0x00FF00FF) << 8) | ((val & 0xFF00FF00) >> 8);
    val = (val << 16) | (val >> 16);
    state_.regs[inst->rd] = val;
    ADVANCE_AND_DISPATCH();
}

// ----------------------------------------------------------------------------
// SIMD/Packed
// ----------------------------------------------------------------------------

handle_Pkhbt: {
    // PKHBT Rd, Rn, Rm{, LSL #imm}
    // Rd[15:0] = Rn[15:0], Rd[31:16] = (Rm << shift)[31:16]
    CHECK_COND();
    u32 rn_val = state_.regs[inst->dp.rn];
    u32 rm_val = state_.regs[inst->dp.rm];
    u32 shifted = rm_val << inst->dp.shift_imm;
    state_.regs[inst->rd] = (rn_val & 0xFFFF) | (shifted & 0xFFFF0000);
    ADVANCE_AND_DISPATCH();
}

handle_Pkhtb: {
    // PKHTB Rd, Rn, Rm{, ASR #imm}
    // Rd[31:16] = Rn[31:16], Rd[15:0] = (Rm >> shift)[15:0] (arithmetic shift)
    CHECK_COND();
    u32 rn_val = state_.regs[inst->dp.rn];
    u32 rm_val = state_.regs[inst->dp.rm];
    // ASR #0 encodes ASR #32; >> 31 sign-fills identically without the
    // undefined shift-by-32
    u32 shift = inst->dp.shift_imm == 0 ? 31 : inst->dp.shift_imm;
    s32 shifted = static_cast<s32>(rm_val) >> shift;
    state_.regs[inst->rd] = (rn_val & 0xFFFF0000) | (static_cast<u32>(shifted) & 0xFFFF);
    ADVANCE_AND_DISPATCH();
}

// SEL: select bytes based on GE flags
handle_Sel: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 ge = GET_GE();
    u32 result = 0;
    for (int i = 0; i < 4; i++) {
        u32 mask = 0xFFu << (i * 8);
        result |= (ge & (1u << i)) ? (rn & mask) : (rm & mask);
    }
    state_.regs[inst->rd] = result;
    ADVANCE_AND_DISPATCH();
}

// USAD8: unsigned sum of absolute differences
handle_Usad8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 sum = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = (rn >> (i * 8)) & 0xFF;
        s32 b = (rm >> (i * 8)) & 0xFF;
        sum += static_cast<u32>(a > b ? a - b : b - a);
    }
    state_.regs[inst->rd] = sum;
    ADVANCE_AND_DISPATCH();
}

// USADA8: unsigned sum of absolute differences with accumulate
handle_Usada8: {
    CHECK_COND();
    u32 rn = state_.regs[inst->dp.rn];
    u32 rm = state_.regs[inst->dp.rm];
    u32 sum = 0;
    for (int i = 0; i < 4; i++) {
        s32 a = (rn >> (i * 8)) & 0xFF;
        s32 b = (rm >> (i * 8)) & 0xFF;
        sum += static_cast<u32>(a > b ? a - b : b - a);
    }
    state_.regs[inst->rd] = state_.regs[inst->dp.imm] + sum;
    ADVANCE_AND_DISPATCH();
}

// ----------------------------------------------------------------------------
// Exclusive Load/Store
// ----------------------------------------------------------------------------

// LDREX reserves a value at an address; STREX succeeds only if the reserved
// word is still there (AtomicCompareAndSwap), which is what enforces
// exclusivity against other cores and the DSP host thread. The mapped-RAM path
// reads and swaps through the page table inline; MMIO / rasterizer-cached pages
// (null in the page array) fall back to the MemorySystem slow path.
#define EXCLUSIVE_LOAD(bits, memtype, memread)                                                      \
    CHECK_COND();                                                                                   \
    const u32 addr = state_.regs[inst->dp.rn];                                                      \
    memtype value;                                                                                  \
    if (u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS]) {                                 \
        std::memcpy(&value, &page[addr & FASTINTERP_PAGE_MASK], sizeof(memtype));                   \
    } else {                                                                                        \
        value = memory_.memread(addr);                                                              \
    }                                                                                               \
    exclusive_.addr = addr;                                                                         \
    exclusive_.value = value;                                                                       \
    exclusive_.size = bits / 8;                                                                     \
    exclusive_.valid = true;

#define EXCLUSIVE_STORE(bits, memtype, memwrite, srcval)                                            \
    CHECK_COND();                                                                                   \
    const u32 addr = state_.regs[inst->dp.rn];                                                      \
    bool ok = false;                                                                                \
    if (exclusive_.valid && exclusive_.addr == addr && exclusive_.size == bits / 8) {               \
        const memtype expected = static_cast<memtype>(exclusive_.value);                            \
        if (u8* page = local_page_ptrs[addr >> FASTINTERP_PAGE_BITS]) {                             \
            ok = Common::AtomicCompareAndSwap(                                                      \
                reinterpret_cast<volatile memtype*>(&page[addr & FASTINTERP_PAGE_MASK]), (srcval),  \
                expected);                                                                          \
        } else {                                                                                    \
            ok = memory_.memwrite(addr, (srcval), expected);                                        \
        }                                                                                           \
    }                                                                                               \
    exclusive_.valid = false;                                                                       \
    state_.regs[inst->rd] = ok ? 0 : 1;

handle_Ldrex: {
    EXCLUSIVE_LOAD(32, u32, Read32);
    state_.regs[inst->rd] = value;
    ADVANCE_AND_DISPATCH();
}

handle_Strex: {
    EXCLUSIVE_STORE(32, u32, WriteExclusive32, state_.regs[inst->dp.rm]);
    ADVANCE_AND_DISPATCH();
}

handle_Ldrexb: {
    EXCLUSIVE_LOAD(8, u8, Read8);
    state_.regs[inst->rd] = value;
    ADVANCE_AND_DISPATCH();
}

handle_Strexb: {
    EXCLUSIVE_STORE(8, u8, WriteExclusive8, static_cast<u8>(state_.regs[inst->dp.rm]));
    ADVANCE_AND_DISPATCH();
}

handle_Ldrexh: {
    EXCLUSIVE_LOAD(16, u16, Read16);
    state_.regs[inst->rd] = value;
    ADVANCE_AND_DISPATCH();
}

handle_Strexh: {
    EXCLUSIVE_STORE(16, u16, WriteExclusive16, static_cast<u16>(state_.regs[inst->dp.rm]));
    ADVANCE_AND_DISPATCH();
}

handle_Ldrexd: {
    EXCLUSIVE_LOAD(64, u64, Read64);
    state_.regs[inst->rd] = static_cast<u32>(value);
    state_.regs[inst->rd + 1] = static_cast<u32>(value >> 32);
    ADVANCE_AND_DISPATCH();
}

handle_Strexd: {
    const u64 dval = static_cast<u64>(state_.regs[inst->dp.rm]) |
                     (static_cast<u64>(state_.regs[inst->dp.rm + 1]) << 32);
    EXCLUSIVE_STORE(64, u64, WriteExclusive64, dval);
    ADVANCE_AND_DISPATCH();
}

#undef EXCLUSIVE_LOAD
#undef EXCLUSIVE_STORE

handle_Clrex:
    exclusive_.valid = false;
    ADVANCE_AND_DISPATCH();

// ----------------------------------------------------------------------------
// Coprocessor (CP15) Instructions
// ----------------------------------------------------------------------------

handle_Mrc: {
    // MRC p15, opc1, Rt, CRn, CRm, opc2
    CHECK_COND();
    u32 crn = inst->dp.rn;       // CRn
    u32 crm = inst->dp.rm;       // CRm
    u32 opc1 = inst->dp.shift_imm; // opc1
    u32 opc2 = inst->dp.imm;     // opc2
    u32 value = 0;

    if (crn == 13 && opc1 == 0 && crm == 0) {
        if (opc2 == 2) value = state_.cp15_thread_uprw;
        else if (opc2 == 3) value = state_.cp15_thread_uro;
    }

    if (inst->rd == 15) {
        // Write to CPSR flags
        u32 cpsr = state_.GetCPSR();
        cpsr = (cpsr & ~0xF0000000u) | (value & 0xF0000000u);
        state_.SetCPSR(cpsr);
    } else {
        state_.regs[inst->rd] = value;
    }
    ADVANCE_AND_DISPATCH();
}

handle_Mcr: {
    // MCR p15, opc1, Rt, CRn, CRm, opc2
    CHECK_COND();
    u32 crn = inst->dp.rn;
    u32 crm = inst->dp.rm;
    u32 opc1 = inst->dp.shift_imm;
    u32 opc2 = inst->dp.imm;
    u32 value = state_.regs[inst->rd];

    if (crn == 13 && opc1 == 0 && crm == 0) {
        if (opc2 == 2) state_.cp15_thread_uprw = value;
        else if (opc2 == 3) state_.cp15_thread_uro = value;
    }
    ADVANCE_AND_DISPATCH();
}
