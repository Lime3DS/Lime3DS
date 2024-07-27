// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <vector>
#include "common/common_types.h"

namespace Core {

constexpr u32 NUM_SYSTEM_TITLE_REGIONS = 7;

enum SystemTitleSet : u32 { Minimal = 1 << 0, Old3ds = 1 << 1, New3ds = 1 << 2 };

/// Returns a list of firmware title IDs for a specific set and region.
std::vector<u64> GetSystemTitleIds(SystemTitleSet set, u32 region);

/// Gets the home menu title ID for a specific region.
u64 GetHomeMenuTitleId(u32 region);

/// Gets the home menu NCCH path for a specific region.
std::string GetHomeMenuNcchPath(u32 region);

/// Returns the region of a system title, if it can be determined.
std::optional<u32> GetSystemTitleRegion(u64 title_id);

} // namespace Core
