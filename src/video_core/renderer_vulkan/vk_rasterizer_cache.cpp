// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/rasterizer_cache/rasterizer_cache.h"
#include "video_core/renderer_vulkan/vk_texture_runtime.h"

namespace VideoCore {
template class RasterizerCache<Vulkan::Traits>;
} // namespace VideoCore
