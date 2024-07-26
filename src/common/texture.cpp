// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "common/texture.h"

namespace Common {

void FlipRGBA8Texture(std::span<u8> tex, u32 width, u32 height) {
    ASSERT(tex.size() == width * height * 4);
    const u32 line_size = width * 4;
    for (u32 line = 0; line < height / 2; line++) {
        const u32 offset_1 = line * line_size;
        const u32 offset_2 = (height - line - 1) * line_size;
        // Swap lines
        std::swap_ranges(tex.begin() + offset_1, tex.begin() + offset_1 + line_size,
                         tex.begin() + offset_2);
    }
}

} // namespace Common
