// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/slot_vector.h"

#pragma once

namespace VideoCore {

using SurfaceId = Common::SlotId;
using SamplerId = Common::SlotId;
using FramebufferId = Common::SlotId;

/// Fake surface ID for null surfaces
constexpr SurfaceId NULL_SURFACE_ID{0};
/// Fake surface ID for null cube surfaces
constexpr SurfaceId NULL_SURFACE_CUBE_ID{1};
/// Fake sampler ID for null samplers
constexpr SamplerId NULL_SAMPLER_ID{0};

} // namespace VideoCore
