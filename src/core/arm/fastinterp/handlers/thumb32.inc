// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Thumb32 handlers: BL, BLX (32-bit Thumb instructions)

handle_ThumbBl: {
    state_.regs[14] = inst->branch.lr;   // Precomputed at decode: (PC+4)|1
    SET_PC(inst->branch.target);
    executed += inst->ticks;
    goto chain;
}

handle_ThumbBlxImm: {
    state_.regs[14] = inst->branch.lr;   // Precomputed at decode: (PC+4)|1
    state_.thumb_mode = false;  // Switch to ARM mode
    SET_PC(inst->branch.target);
    executed += inst->ticks;
    goto chain;
}
