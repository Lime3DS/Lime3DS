// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <vector>
#include "common/common_types.h"

namespace Core::NUS {

std::optional<std::vector<u8>> Download(const std::string& path);

} // namespace Core::NUS
