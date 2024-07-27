// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_vulkan/vk_memory_util.h"

namespace Vulkan {

std::optional<u32> FindMemoryType(const vk::PhysicalDeviceMemoryProperties& properties,
                                  vk::MemoryPropertyFlags wanted, std::bitset<32> memory_type_mask,
                                  vk::MemoryPropertyFlags excluded) {
    for (u32 i = 0; i < properties.memoryTypeCount; ++i) {
        if (!memory_type_mask.test(i)) {
            continue;
        }
        const auto flags = properties.memoryTypes[i].propertyFlags;
        if (((flags & wanted) == wanted) && (!(flags & excluded))) {
            return i;
        }
    }
    return std::nullopt;
}
} // namespace Vulkan
