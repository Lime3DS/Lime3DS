// Copyright 2013 Dolphin Emulator Project / 2021 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if LIME3DS_ARCH(arm64)

#include <string>

namespace Common {

/// Arm64 CPU capabilities that may be detected by this module
struct CPUCaps {
    std::string cpu_string;

    bool aes;
    bool afp; // Alternate floating-point behavior
    bool asimd;
    bool crc32;
    bool fma;
    bool fp;
    bool sha1;
    bool sha2;
};

/**
 * Gets the supported capabilities of the host CPU
 * @return Reference to a CPUCaps struct with the detected host CPU capabilities
 */
const CPUCaps& GetCPUCaps();

} // namespace Common

#endif // LIME3DS_ARCH(arm64)
