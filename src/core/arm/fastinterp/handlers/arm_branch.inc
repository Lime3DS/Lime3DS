// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// ARM Branch handlers: B, Bl, Bx, Blx, BlxImm

handle_B: {
    CHECK_COND();
    SET_PC(inst->branch.target); // Absolute target precomputed at decode
    executed += inst->ticks;
    goto chain;
}

handle_Bl: {
    CHECK_COND();
    state_.regs[14] = inst->branch.lr; // Precomputed at decode
    SET_PC(inst->branch.target);
    executed += inst->ticks;
    goto chain;
}

handle_Bx: {
    CHECK_COND();
    u32 target = state_.regs[inst->branch.rm];
    state_.thumb_mode = (target & 1) != 0;
    SET_PC(target & ~1u);
    executed += inst->ticks;
    goto chain;
}

handle_Blx: {
    CHECK_COND();
    // BLX register (immediate form is BlxImm); this is the ARM encoding, so
    // the block is ARM mode (Thumb BLX is ThumbBlxReg)
    state_.regs[14] = GET_PC() + 4;
    u32 target = state_.regs[inst->branch.rm];
    state_.thumb_mode = (target & 1) != 0;
    SET_PC(target & ~1u);
    executed += inst->ticks;
    goto chain;
}

handle_BlxImm: {
    CHECK_COND();
    // ARM BLX immediate: target, LR, and destination mode precomputed at decode
    state_.regs[14] = inst->branch.lr;
    state_.thumb_mode = inst->branch.rm != 0;
    SET_PC(inst->branch.target);
    executed += inst->ticks;
    goto chain;
}
