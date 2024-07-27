// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_vulkan/vk_common.h"

// Implement vma functions
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

// Store the dispatch loader here
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
