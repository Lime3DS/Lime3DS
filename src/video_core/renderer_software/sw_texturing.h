// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>

#include "common/common_types.h"
#include "common/vector_math.h"
#include "video_core/pica/regs_texturing.h"

namespace SwRenderer {

int GetWrappedTexCoord(Pica::TexturingRegs::TextureConfig::WrapMode mode, s32 val, u32 size);

Common::Vec3<u8> GetColorModifier(Pica::TexturingRegs::TevStageConfig::ColorModifier factor,
                                  const Common::Vec4<u8>& values);

u8 GetAlphaModifier(Pica::TexturingRegs::TevStageConfig::AlphaModifier factor,
                    const Common::Vec4<u8>& values);

Common::Vec3<u8> ColorCombine(Pica::TexturingRegs::TevStageConfig::Operation op,
                              std::span<const Common::Vec3<u8>, 3> input);

u8 AlphaCombine(Pica::TexturingRegs::TevStageConfig::Operation op, const std::array<u8, 3>& input);

} // namespace SwRenderer
