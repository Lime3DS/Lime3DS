// Copyright 2022 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Core {

u64 TicksForInstruction(bool is_thumb, u32 instruction);

} // namespace Core
