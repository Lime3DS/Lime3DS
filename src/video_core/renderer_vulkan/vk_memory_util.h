// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <bitset>
#include <optional>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

/// Find a memory type with the passed requirements
std::optional<u32> FindMemoryType(
    const vk::PhysicalDeviceMemoryProperties& properties, vk::MemoryPropertyFlags wanted,
    std::bitset<32> memory_type_mask = 0xFFFFFFFF,
    vk::MemoryPropertyFlags excluded = vk::MemoryPropertyFlagBits::eProtected);

} // namespace Vulkan
