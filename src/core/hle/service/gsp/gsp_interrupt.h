// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include "common/common_types.h"

namespace Service::GSP {

/// GSP interrupt ID
enum class InterruptId : u8 {
    PSC0 = 0x00,
    PSC1 = 0x01,
    PDC0 = 0x02,
    PDC1 = 0x03,
    PPF = 0x04,
    P3D = 0x05,
    DMA = 0x06,
};

/// GSP thread interrupt relay queue
struct InterruptRelayQueue {
    // Index of last interrupt in the queue
    u8 index;
    // Number of interrupts remaining to be processed by the userland code
    u8 number_interrupts;
    // Error code - zero on success, otherwise an error has occurred
    u8 error_code;
    u8 padding1;

    u32 missed_PDC0;
    u32 missed_PDC1;

    InterruptId slot[0x34]; ///< Interrupt ID slots
};
static_assert(sizeof(InterruptRelayQueue) == 0x40, "InterruptRelayQueue struct has incorrect size");

using InterruptHandler = std::function<void(InterruptId)>;

} // namespace Service::GSP
