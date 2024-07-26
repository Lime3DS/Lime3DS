// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <utility>

#include "common/quaternion.h"
#include "common/vector_math.h"
#include "video_core/pica/pica_core.h"

namespace SwRenderer {

std::pair<Common::Vec4<u8>, Common::Vec4<u8>> ComputeFragmentsColors(
    const Pica::LightingRegs& lighting, const Pica::PicaCore::Lighting& lighting_state,
    const Common::Quaternion<f32>& normquat, const Common::Vec3f& view,
    std::span<const Common::Vec4<u8>, 4> texture_color);

} // namespace SwRenderer
