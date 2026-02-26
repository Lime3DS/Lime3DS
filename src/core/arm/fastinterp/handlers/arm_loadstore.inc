// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// ARM Load/Store handlers: register/scaled-offset (Ldr, LdrB, LdrH, LdrSB,
//     LdrSH, Str, StrB, StrH), immediate-offset fast paths (LdrImm, StrImm,
//     LdrBImm, StrBImm, LdrHImm, StrHImm), PC-relative literal (LdrLit),
//     doubleword (Ldrd, Strd), and block transfer (Ldm, Stm)

handle_Ldr: {
    CHECK_COND();
    // rn==15 is only reachable behind a PcSetup prefix (regs[15] = pipeline
    // PC): word literals fold to LdrLit at decode, and the Thumb byte/half
    // literal alignment is baked into the decoded offset.
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(ShiftNoCarry(state_.regs[inst->ls.rm], inst->ls.shift_type,
                                                inst->ls.shift_imm));
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    state_.regs[inst->rd] = MemRead32(effective_addr);
    executed += inst->ticks;

    // If destination is PC, it's a branch
    if (inst->rd == 15) {
        u32 new_pc = state_.regs[15];
        state_.thumb_mode = new_pc & 1;
        SET_PC(new_pc & ~1u);
        goto chain;
    }

    SET_PC(GET_PC() + inst->inst_size);
    DISPATCH();
}

handle_LdrB: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(ShiftNoCarry(state_.regs[inst->ls.rm], inst->ls.shift_type,
                                                inst->ls.shift_imm));
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    state_.regs[inst->rd] = MemRead8(effective_addr);
    ADVANCE_AND_DISPATCH();
}

handle_LdrH: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(state_.regs[inst->ls.rm]);
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    state_.regs[inst->rd] = MemRead16(effective_addr);
    ADVANCE_AND_DISPATCH();
}

handle_LdrSB: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(state_.regs[inst->ls.rm]);
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    state_.regs[inst->rd] = static_cast<u32>(static_cast<s8>(MemRead8(effective_addr)));
    ADVANCE_AND_DISPATCH();
}

handle_LdrSH: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(state_.regs[inst->ls.rm]);
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    state_.regs[inst->rd] = static_cast<u32>(static_cast<s16>(MemRead16(effective_addr)));
    ADVANCE_AND_DISPATCH();
}

handle_Str: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(ShiftNoCarry(state_.regs[inst->ls.rm], inst->ls.shift_type,
                                                inst->ls.shift_imm));
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    MemWrite32(effective_addr, state_.regs[inst->rd]);
    ADVANCE_AND_DISPATCH();
}

handle_StrB: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(ShiftNoCarry(state_.regs[inst->ls.rm], inst->ls.shift_type,
                                                inst->ls.shift_imm));
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    MemWrite8(effective_addr, static_cast<u8>(state_.regs[inst->rd]));
    ADVANCE_AND_DISPATCH();
}

handle_StrH: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(state_.regs[inst->ls.rm]);
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }

    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }

    MemWrite16(effective_addr, static_cast<u16>(state_.regs[inst->rd]));
    ADVANCE_AND_DISPATCH();
}

// Specialized immediate-offset load/store (decode guarantees: offset mode,
// immediate offset, rd != 15, rn != 15)

handle_LdrImm: {
    CHECK_COND();
    MEM_READ32(state_.regs[inst->ls.rn] + inst->ls.offset, state_.regs[inst->rd]);
    ADVANCE_AND_DISPATCH();
}

handle_StrImm: {
    CHECK_COND();
    MEM_WRITE32(state_.regs[inst->ls.rn] + inst->ls.offset, state_.regs[inst->rd]);
    ADVANCE_AND_DISPATCH();
}

handle_LdrBImm: {
    CHECK_COND();
    MEM_READ8(state_.regs[inst->ls.rn] + inst->ls.offset, state_.regs[inst->rd]);
    ADVANCE_AND_DISPATCH();
}

handle_StrBImm: {
    CHECK_COND();
    MEM_WRITE8(state_.regs[inst->ls.rn] + inst->ls.offset, state_.regs[inst->rd]);
    ADVANCE_AND_DISPATCH();
}

handle_LdrHImm: {
    CHECK_COND();
    u16 val;
    MEM_READ16(state_.regs[inst->ls.rn] + inst->ls.offset, val);
    state_.regs[inst->rd] = val; // zero-extend (matches generic LdrH)
    ADVANCE_AND_DISPATCH();
}

handle_StrHImm: {
    CHECK_COND();
    MEM_WRITE16(state_.regs[inst->ls.rn] + inst->ls.offset, state_.regs[inst->rd]);
    ADVANCE_AND_DISPATCH();
}

// LDR literal: ls.offset holds the absolute literal-pool address,
// precomputed at decode from the pipeline-adjusted PC. rd==15 is a branch
// (ARM jump tables use LDR PC, [PC, #imm]).
handle_LdrLit: {
    CHECK_COND();
    u32 value;
    MEM_READ32(static_cast<u32>(inst->ls.offset), value);
    state_.regs[inst->rd] = value;
    executed += inst->ticks;
    if (inst->rd == 15) {
        state_.thumb_mode = value & 1;
        SET_PC(value & ~1u);
        goto chain;
    }
    SET_PC(GET_PC() + inst->inst_size);
    DISPATCH();
}

// ----------------------------------------------------------------------------
// Doubleword Load/Store
// ----------------------------------------------------------------------------

handle_Ldrd: {
    CHECK_COND();
    // rn==15 (LDRD literal) arrives behind a PcSetup prefix with the
    // alignment baked into the decoded offset
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(state_.regs[inst->ls.rm]);
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }
    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }
    state_.regs[inst->rd] = MemRead32(effective_addr);
    state_.regs[inst->ls.rt2] = MemRead32(effective_addr + 4);
    executed += inst->ticks;
    // If Rt2 == PC it's a branch (decode rejects this; kept for safety)
    if (inst->ls.rt2 == 15) {
        u32 new_pc = state_.regs[15];
        state_.thumb_mode = new_pc & 1;
        SET_PC(new_pc & ~1u);
        goto chain;
    }
    SET_PC(GET_PC() + inst->inst_size);
    DISPATCH();
}

handle_Strd: {
    CHECK_COND();
    u32 addr = state_.regs[inst->ls.rn];
    s32 offset = inst->ls.offset;
    if (inst->ls.rm != 0xFF) {
        offset = static_cast<s32>(state_.regs[inst->ls.rm]);
        if (!(inst->ls.flags & 0x01)) {
            offset = -offset; // U==0 => subtract register offset
        }
    }
    u32 effective_addr;
    switch (inst->ls.mode) {
    case AddrMode::Offset:
        effective_addr = addr + offset;
        break;
    case AddrMode::PreIndex:
        effective_addr = addr + offset;
        state_.regs[inst->ls.rn] = effective_addr;
        break;
    case AddrMode::PostIndex:
        effective_addr = addr;
        state_.regs[inst->ls.rn] = addr + offset;
        break;
    }
    MemWrite32(effective_addr, state_.regs[inst->rd]);
    MemWrite32(effective_addr + 4, state_.regs[inst->ls.rt2]);
    ADVANCE_AND_DISPATCH();
}

// ----------------------------------------------------------------------------
// Load/Store Multiple
// ----------------------------------------------------------------------------

handle_Ldm: {
    CHECK_COND();
    u32 addr = state_.regs[inst->lsm.rn];
    u16 reg_list = inst->lsm.reg_list;
    bool writeback = (inst->lsm.flags & 0x01) != 0;  // W bit (bit 0)

    // count/base_off/wb_delta precomputed at decode (fold increment/before)
    u32 current = addr + inst->lsm.base_off;
    u32 end_addr = current + inst->lsm.count * 4 - 4;

    // Fast path: if all accesses are on the same mapped page, use direct pointer
    // This avoids per-access null checks in the loop (critical for LDM performance)
    const u8* base_page = local_page_ptrs[current >> FASTINTERP_PAGE_BITS];
    bool same_page = (current >> FASTINTERP_PAGE_BITS) == (end_addr >> FASTINTERP_PAGE_BITS);

    if (__builtin_expect(base_page != nullptr && same_page, 1)) {
        // Fast path: direct pointer access without per-iteration checks
        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) {
                u32 loaded_val;
                std::memcpy(&loaded_val, &base_page[current & FASTINTERP_PAGE_MASK], 4);
                state_.regs[i] = loaded_val;
                current += 4;
            }
        }
    } else {
        // Slow path: per-access checks (page boundary crossing or unmapped)
        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) {
                state_.regs[i] = MemRead32(current);
                current += 4;
            }
        }
    }

    if (writeback) {
        state_.regs[inst->lsm.rn] = addr + inst->lsm.wb_delta;
    }

    executed += inst->ticks;

    // If PC was loaded, it's a branch - don't advance PC, end block
    if (reg_list & (1 << 15)) {
        // PC was loaded from memory - handle mode switch if needed
        u32 new_pc = state_.regs[15];
        state_.thumb_mode = new_pc & 1;
        SET_PC(new_pc & ~1u);
        goto chain;
    }

    SET_PC(GET_PC() + inst->inst_size);
    DISPATCH();
}

handle_Stm: {
    CHECK_COND();
    u32 addr = state_.regs[inst->lsm.rn];
    u16 reg_list = inst->lsm.reg_list;
    bool writeback = (inst->lsm.flags & 0x01) != 0;  // W bit (bit 0)

    // count/base_off/wb_delta precomputed at decode (fold increment/before)
    u32 current = addr + inst->lsm.base_off;
    u32 end_addr = current + inst->lsm.count * 4 - 4;

    // Fast path: if all accesses are on the same mapped page, use direct pointer
    u8* base_page = local_page_ptrs[current >> FASTINTERP_PAGE_BITS];
    bool same_page = (current >> FASTINTERP_PAGE_BITS) == (end_addr >> FASTINTERP_PAGE_BITS);

    if (__builtin_expect(base_page != nullptr && same_page, 1)) {
        // Fast path: direct pointer access without per-iteration checks
        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) {
                u32 val = state_.regs[i];
                std::memcpy(&base_page[current & FASTINTERP_PAGE_MASK], &val, 4);
                current += 4;
            }
        }
    } else {
        // Slow path: per-access checks (page boundary crossing or unmapped)
        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) {
                MemWrite32(current, state_.regs[i]);
                current += 4;
            }
        }
    }

    if (writeback) {
        state_.regs[inst->lsm.rn] = addr + inst->lsm.wb_delta;
    }

    ADVANCE_AND_DISPATCH();
}
