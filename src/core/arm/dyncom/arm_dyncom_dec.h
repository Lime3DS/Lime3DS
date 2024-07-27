// Copyright 2012 Michael Kang, 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

enum class ARMDecodeStatus { SUCCESS, FAILURE };

ARMDecodeStatus DecodeARMInstruction(u32 instr, int* idx);
