// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Special handlers: Invalid, Nop, EndBlock, PcSetup

handle_Invalid:
    // Invalid instruction - stop execution
    goto done;

handle_Nop:
    ADVANCE_AND_DISPATCH();

handle_EndBlock:
    // Synthetic block terminator (zero ticks): chain straight to the next block
    // instead of returning to Run().
    goto chain;

handle_PcSetup:
    // Materialize the precomputed PC into regs[15] for the next instruction, which
    // reads R15 as a source and couldn't be folded at decode. Zero ticks; no PC advance.
    state_.regs[15] = inst->imm32;
    DISPATCH();
