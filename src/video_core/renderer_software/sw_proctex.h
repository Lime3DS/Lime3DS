// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica/pica_core.h"

namespace SwRenderer {

/// Generates procedural texture color for the given coordinates
Common::Vec4<u8> ProcTex(float u, float v, const Pica::TexturingRegs& regs,
                         const Pica::PicaCore::ProcTex& state);

} // namespace SwRenderer
