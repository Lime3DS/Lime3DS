// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if LIME3DS_ARCH(arm64)

#include <type_traits>
#include <oaknut/oaknut.hpp>
#include "common/aarch64/oaknut_abi.h"

namespace Common::A64 {

// BL can only reach targets within +-128MiB(24 bits)
inline bool IsWithin128M(uintptr_t ref, uintptr_t target) {
    const u64 distance = target - (ref + 4);
    return !(distance >= 0x800'0000ULL && distance <= ~0x800'0000ULL);
}

inline bool IsWithin128M(const oaknut::CodeGenerator& code, uintptr_t target) {
    return IsWithin128M(code.xptr<uintptr_t>(), target);
}

template <typename T>
inline void CallFarFunction(oaknut::CodeGenerator& code, const T f) {
    static_assert(std::is_pointer_v<T>, "Argument must be a (function) pointer.");
    const std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(f);
    if (IsWithin128M(code, addr)) {
        code.BL(reinterpret_cast<const void*>(f));
    } else {
        // X16(IP0) and X17(IP1) is the standard veneer register
        // LR is also available as an intermediate register
        // https://developer.arm.com/documentation/102374/0101/Procedure-Call-Standard
        code.MOVP2R(oaknut::util::X16, reinterpret_cast<const void*>(f));
        code.BLR(oaknut::util::X16);
    }
}

} // namespace Common::A64

#endif // LIME3DS_ARCH(arm64)
