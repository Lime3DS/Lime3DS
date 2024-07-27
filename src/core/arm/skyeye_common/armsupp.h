// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#define BITS(s, a, b) ((s << ((sizeof(s) * 8 - 1) - b)) >> (sizeof(s) * 8 - b + a - 1))
#define BIT(s, n) ((s >> (n)) & 1)

#define POS(i) ((~(i)) >> 31)
#define NEG(i) ((i) >> 31)

bool AddOverflow(u32, u32, u32);
bool SubOverflow(u32, u32, u32);

u32 AddWithCarry(u32, u32, u32, bool*, bool*);
bool ARMul_AddOverflowQ(u32, u32);

u8 ARMul_SignedSaturatedAdd8(u8, u8);
u8 ARMul_SignedSaturatedSub8(u8, u8);
u16 ARMul_SignedSaturatedAdd16(u16, u16);
u16 ARMul_SignedSaturatedSub16(u16, u16);

u8 ARMul_UnsignedSaturatedAdd8(u8, u8);
u16 ARMul_UnsignedSaturatedAdd16(u16, u16);
u8 ARMul_UnsignedSaturatedSub8(u8, u8);
u16 ARMul_UnsignedSaturatedSub16(u16, u16);
u8 ARMul_UnsignedAbsoluteDifference(u8, u8);
u32 ARMul_SignedSatQ(s32, u8, bool*);
u32 ARMul_UnsignedSatQ(s32, u8, bool*);
