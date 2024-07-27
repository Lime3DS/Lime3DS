// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <limits>
#include <string_view>
#include "common/common_types.h"

namespace VideoCore {

enum class CustomPixelFormat : u32 {
    RGBA8 = 0,
    BC1 = 1,
    BC3 = 2,
    BC5 = 3,
    BC7 = 4,
    ASTC4 = 5,
    ASTC6 = 6,
    ASTC8 = 7,
    Invalid = std::numeric_limits<u32>::max(),
};

enum class CustomFileFormat : u32 {
    None = 0,
    PNG = 1,
    DDS = 2,
    KTX = 3,
};

std::string_view CustomPixelFormatAsString(CustomPixelFormat format);

bool IsCustomFormatCompressed(CustomPixelFormat format);

} // namespace VideoCore
