// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

namespace FileSys::Patch {

bool ApplyIpsPatch(const std::vector<u8>& patch, std::vector<u8>& buffer);

bool ApplyBpsPatch(const std::vector<u8>& patch, std::vector<u8>& buffer);

} // namespace FileSys::Patch
