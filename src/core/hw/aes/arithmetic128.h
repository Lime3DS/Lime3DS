// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hw/aes/key.h"

namespace HW::AES {
AESKey Lrot128(const AESKey& in, u32 rot);
AESKey Add128(const AESKey& a, const AESKey& b);
AESKey Add128(const AESKey& a, u64 b);
AESKey Xor128(const AESKey& a, const AESKey& b);

} // namespace HW::AES
