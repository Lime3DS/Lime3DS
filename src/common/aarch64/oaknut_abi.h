// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if LIME3DS_ARCH(arm64)

#include <bitset>
#include <initializer_list>
#include <oaknut/oaknut.hpp>
#include "common/assert.h"

namespace Common::A64 {

constexpr std::size_t RegToIndex(const oaknut::Reg& reg) {
    ASSERT(reg.index() != 31); // ZR not allowed
    return reg.index() + (reg.is_vector() ? 32 : 0);
}

constexpr oaknut::XReg IndexToXReg(std::size_t reg_index) {
    ASSERT(reg_index <= 30);
    return oaknut::XReg(static_cast<int>(reg_index));
}

constexpr oaknut::VReg IndexToVReg(std::size_t reg_index) {
    ASSERT(reg_index >= 32 && reg_index < 64);
    return oaknut::QReg(static_cast<int>(reg_index - 32));
}

constexpr oaknut::Reg IndexToReg(std::size_t reg_index) {
    if (reg_index < 32) {
        return IndexToXReg(reg_index);
    } else {
        return IndexToVReg(reg_index);
    }
}

inline constexpr std::bitset<64> BuildRegSet(std::initializer_list<oaknut::Reg> regs) {
    std::bitset<64> bits;
    for (const oaknut::Reg& reg : regs) {
        bits.set(RegToIndex(reg));
    }
    return bits;
}

constexpr inline std::bitset<64> ABI_ALL_GPRS(0x00000000'7FFFFFFF);
constexpr inline std::bitset<64> ABI_ALL_FPRS(0xFFFFFFFF'00000000);

constexpr inline oaknut::XReg ABI_RETURN = oaknut::util::X0;
constexpr inline oaknut::XReg ABI_PARAM1 = oaknut::util::X0;
constexpr inline oaknut::XReg ABI_PARAM2 = oaknut::util::X1;
constexpr inline oaknut::XReg ABI_PARAM3 = oaknut::util::X2;
constexpr inline oaknut::XReg ABI_PARAM4 = oaknut::util::X3;

constexpr std::bitset<64> ABI_ALL_CALLER_SAVED = 0xffffffff'4000ffff;
constexpr std::bitset<64> ABI_ALL_CALLEE_SAVED = 0x0000ff00'7ff80000;

struct ABIFrameInfo {
    u32 subtraction;
    u32 fprs_offset;
};

inline ABIFrameInfo ABI_CalculateFrameSize(std::bitset<64> regs, std::size_t frame_size) {
    const std::size_t gprs_count = (regs & ABI_ALL_GPRS).count();
    const std::size_t fprs_count = (regs & ABI_ALL_FPRS).count();

    const std::size_t gprs_size = (gprs_count + 1) / 2 * 16;
    const std::size_t fprs_size = fprs_count * 16;

    std::size_t total_size = 0;
    total_size += gprs_size;
    const std::size_t fprs_base_subtraction = total_size;
    total_size += fprs_size;
    total_size += frame_size;

    return ABIFrameInfo{static_cast<u32>(total_size), static_cast<u32>(fprs_base_subtraction)};
}

inline void ABI_PushRegisters(oaknut::CodeGenerator& code, std::bitset<64> regs,
                              std::size_t frame_size = 0) {
    using namespace oaknut;
    using namespace oaknut::util;
    auto frame_info = ABI_CalculateFrameSize(regs, frame_size);

    // Allocate stack-space
    if (frame_info.subtraction != 0) {
        code.SUB(SP, SP, frame_info.subtraction);
    }

    {
        const std::bitset<64> gprs_mask = (regs & ABI_ALL_GPRS);
        std::vector<XReg> gprs;
        gprs.reserve(32);
        for (u8 i = 0; i < 32; ++i) {
            if (gprs_mask.test(i)) {
                gprs.emplace_back(IndexToXReg(i));
            }
        }

        if (!gprs.empty()) {
            for (std::size_t i = 0; i < gprs.size() - 1; i += 2) {
                code.STP(gprs[i], gprs[i + 1], SP, i * sizeof(u64));
            }
            if (gprs.size() % 2 == 1) {
                const std::size_t i = gprs.size() - 1;
                code.STR(gprs[i], SP, i * sizeof(u64));
            }
        }
    }

    {
        const std::bitset<64> fprs_mask = (regs & ABI_ALL_FPRS);
        std::vector<QReg> fprs;
        fprs.reserve(32);
        for (u8 i = 32; i < 64; ++i) {
            if (fprs_mask.test(i)) {
                fprs.emplace_back(IndexToVReg(i).toQ());
            }
        }

        if (!fprs.empty()) {
            for (std::size_t i = 0; i < fprs.size() - 1; i += 2) {
                code.STP(fprs[i], fprs[i + 1], SP, frame_info.fprs_offset + i * (sizeof(u64) * 2));
            }
            if (fprs.size() % 2 == 1) {
                const std::size_t i = fprs.size() - 1;
                code.STR(fprs[i], SP, frame_info.fprs_offset + i * (sizeof(u64) * 2));
            }
        }
    }

    // Allocate frame-space
    if (frame_size != 0) {
        code.SUB(SP, SP, frame_size);
    }
}

inline void ABI_PopRegisters(oaknut::CodeGenerator& code, std::bitset<64> regs,
                             std::size_t frame_size = 0) {
    using namespace oaknut;
    using namespace oaknut::util;
    auto frame_info = ABI_CalculateFrameSize(regs, frame_size);

    // Free frame-space
    if (frame_size != 0) {
        code.ADD(SP, SP, frame_size);
    }

    {
        const std::bitset<64> gprs_mask = (regs & ABI_ALL_GPRS);
        std::vector<XReg> gprs;
        gprs.reserve(32);
        for (u8 i = 0; i < 32; ++i) {
            if (gprs_mask.test(i)) {
                gprs.emplace_back(IndexToXReg(i));
            }
        }

        if (!gprs.empty()) {
            for (std::size_t i = 0; i < gprs.size() - 1; i += 2) {
                code.LDP(gprs[i], gprs[i + 1], SP, i * sizeof(u64));
            }
            if (gprs.size() % 2 == 1) {
                const std::size_t i = gprs.size() - 1;
                code.LDR(gprs[i], SP, i * sizeof(u64));
            }
        }
    }

    {
        const std::bitset<64> fprs_mask = (regs & ABI_ALL_FPRS);
        std::vector<QReg> fprs;
        fprs.reserve(32);
        for (u8 i = 32; i < 64; ++i) {
            if (fprs_mask.test(i)) {
                fprs.emplace_back(IndexToVReg(i).toQ());
            }
        }

        if (!fprs.empty()) {
            for (std::size_t i = 0; i < fprs.size() - 1; i += 2) {
                code.LDP(fprs[i], fprs[i + 1], SP, frame_info.fprs_offset + i * (sizeof(u64) * 2));
            }
            if (fprs.size() % 2 == 1) {
                const std::size_t i = fprs.size() - 1;
                code.LDR(fprs[i], SP, frame_info.fprs_offset + i * (sizeof(u64) * 2));
            }
        }
    }

    // Free stack-space
    if (frame_info.subtraction != 0) {
        code.ADD(SP, SP, frame_info.subtraction);
    }
}

} // namespace Common::A64

#endif // LIME3DS_ARCH(arm64)
