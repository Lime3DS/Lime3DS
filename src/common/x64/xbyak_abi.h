// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/arch.h"
#if LIME3DS_ARCH(x86_64)

#include <bitset>
#include <initializer_list>
#include <xbyak/xbyak.h>
#include "common/assert.h"

namespace Common::X64 {

constexpr std::size_t RegToIndex(const Xbyak::Reg& reg) {
    using Kind = Xbyak::Reg::Kind;
    ASSERT_MSG((reg.getKind() & (Kind::REG | Kind::XMM)) != 0,
               "RegSet only support GPRs and XMM registers.");
    ASSERT_MSG(reg.getIdx() < 16, "RegSet only supports XXM0-15.");
    return reg.getIdx() + (reg.getKind() == Kind::REG ? 0 : 16);
}

constexpr Xbyak::Reg64 IndexToReg64(std::size_t reg_index) {
    ASSERT(reg_index < 16);
    return Xbyak::Reg64(static_cast<int>(reg_index));
}

constexpr Xbyak::Xmm IndexToXmm(std::size_t reg_index) {
    ASSERT(reg_index >= 16 && reg_index < 32);
    return Xbyak::Xmm(static_cast<int>(reg_index - 16));
}

constexpr Xbyak::Reg IndexToReg(std::size_t reg_index) {
    if (reg_index < 16) {
        return IndexToReg64(reg_index);
    } else {
        return IndexToXmm(reg_index);
    }
}

inline std::bitset<32> BuildRegSet(std::initializer_list<Xbyak::Reg> regs) {
    std::bitset<32> bits;
    for (const Xbyak::Reg& reg : regs) {
        bits[RegToIndex(reg)] = true;
    }
    return bits;
}

constexpr inline std::bitset<32> ABI_ALL_GPRS(0x0000FFFF);
constexpr inline std::bitset<32> ABI_ALL_XMMS(0xFFFF0000);

#ifdef _WIN32

// Microsoft x64 ABI
constexpr inline Xbyak::Reg ABI_RETURN = Xbyak::util::rax;
constexpr inline Xbyak::Reg ABI_PARAM1 = Xbyak::util::rcx;
constexpr inline Xbyak::Reg ABI_PARAM2 = Xbyak::util::rdx;
constexpr inline Xbyak::Reg ABI_PARAM3 = Xbyak::util::r8;
constexpr inline Xbyak::Reg ABI_PARAM4 = Xbyak::util::r9;

const std::bitset<32> ABI_ALL_CALLER_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rcx,
    Xbyak::util::rdx,
    Xbyak::util::r8,
    Xbyak::util::r9,
    Xbyak::util::r10,
    Xbyak::util::r11,
    // XMMs
    Xbyak::util::xmm0,
    Xbyak::util::xmm1,
    Xbyak::util::xmm2,
    Xbyak::util::xmm3,
    Xbyak::util::xmm4,
    Xbyak::util::xmm5,
});

const std::bitset<32> ABI_ALL_CALLEE_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rbx,
    Xbyak::util::rsi,
    Xbyak::util::rdi,
    Xbyak::util::rbp,
    Xbyak::util::r12,
    Xbyak::util::r13,
    Xbyak::util::r14,
    Xbyak::util::r15,
    // XMMs
    Xbyak::util::xmm6,
    Xbyak::util::xmm7,
    Xbyak::util::xmm8,
    Xbyak::util::xmm9,
    Xbyak::util::xmm10,
    Xbyak::util::xmm11,
    Xbyak::util::xmm12,
    Xbyak::util::xmm13,
    Xbyak::util::xmm14,
    Xbyak::util::xmm15,
});

constexpr std::size_t ABI_SHADOW_SPACE = 0x20;

#else

// System V x86-64 ABI
constexpr inline Xbyak::Reg ABI_RETURN = Xbyak::util::rax;
constexpr inline Xbyak::Reg ABI_PARAM1 = Xbyak::util::rdi;
constexpr inline Xbyak::Reg ABI_PARAM2 = Xbyak::util::rsi;
constexpr inline Xbyak::Reg ABI_PARAM3 = Xbyak::util::rdx;
constexpr inline Xbyak::Reg ABI_PARAM4 = Xbyak::util::rcx;

const std::bitset<32> ABI_ALL_CALLER_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rcx,
    Xbyak::util::rdx,
    Xbyak::util::rdi,
    Xbyak::util::rsi,
    Xbyak::util::r8,
    Xbyak::util::r9,
    Xbyak::util::r10,
    Xbyak::util::r11,
    // XMMs
    Xbyak::util::xmm0,
    Xbyak::util::xmm1,
    Xbyak::util::xmm2,
    Xbyak::util::xmm3,
    Xbyak::util::xmm4,
    Xbyak::util::xmm5,
    Xbyak::util::xmm6,
    Xbyak::util::xmm7,
    Xbyak::util::xmm8,
    Xbyak::util::xmm9,
    Xbyak::util::xmm10,
    Xbyak::util::xmm11,
    Xbyak::util::xmm12,
    Xbyak::util::xmm13,
    Xbyak::util::xmm14,
    Xbyak::util::xmm15,
});

const std::bitset<32> ABI_ALL_CALLEE_SAVED = BuildRegSet({
    // GPRs
    Xbyak::util::rbx,
    Xbyak::util::rbp,
    Xbyak::util::r12,
    Xbyak::util::r13,
    Xbyak::util::r14,
    Xbyak::util::r15,
});

constexpr std::size_t ABI_SHADOW_SPACE = 0;

#endif

struct ABIFrameInfo {
    s32 subtraction;
    s32 xmm_offset;
};

inline ABIFrameInfo ABI_CalculateFrameSize(std::bitset<32> regs, std::size_t rsp_alignment,
                                           std::size_t needed_frame_size) {
    const auto count = (regs & ABI_ALL_GPRS).count();
    rsp_alignment -= count * 8;
    std::size_t subtraction = 0;
    const auto xmm_count = (regs & ABI_ALL_XMMS).count();
    if (xmm_count) {
        // If we have any XMMs to save, we must align the stack here.
        subtraction = rsp_alignment & 0xF;
    }
    subtraction += 0x10 * xmm_count;
    std::size_t xmm_base_subtraction = subtraction;
    subtraction += needed_frame_size;
    subtraction += ABI_SHADOW_SPACE;
    // Final alignment.
    rsp_alignment -= subtraction;
    subtraction += rsp_alignment & 0xF;

    return ABIFrameInfo{static_cast<s32>(subtraction),
                        static_cast<s32>(subtraction - xmm_base_subtraction)};
}

inline std::size_t ABI_PushRegistersAndAdjustStack(Xbyak::CodeGenerator& code, std::bitset<32> regs,
                                                   std::size_t rsp_alignment,
                                                   std::size_t needed_frame_size = 0) {
    auto frame_info = ABI_CalculateFrameSize(regs, rsp_alignment, needed_frame_size);

    for (std::size_t i = 0; i < regs.size(); ++i) {
        if (regs[i] && ABI_ALL_GPRS[i]) {
            code.push(IndexToReg64(i));
        }
    }

    if (frame_info.subtraction != 0) {
        code.sub(code.rsp, frame_info.subtraction);
    }

    for (std::size_t i = 0; i < regs.size(); ++i) {
        if (regs[i] && ABI_ALL_XMMS[i]) {
            code.movaps(code.xword[code.rsp + frame_info.xmm_offset], IndexToXmm(i));
            frame_info.xmm_offset += 0x10;
        }
    }

    return ABI_SHADOW_SPACE;
}

inline void ABI_PopRegistersAndAdjustStack(Xbyak::CodeGenerator& code, std::bitset<32> regs,
                                           std::size_t rsp_alignment,
                                           std::size_t needed_frame_size = 0) {
    auto frame_info = ABI_CalculateFrameSize(regs, rsp_alignment, needed_frame_size);

    for (std::size_t i = 0; i < regs.size(); ++i) {
        if (regs[i] && ABI_ALL_XMMS[i]) {
            code.movaps(IndexToXmm(i), code.xword[code.rsp + frame_info.xmm_offset]);
            frame_info.xmm_offset += 0x10;
        }
    }

    if (frame_info.subtraction != 0) {
        code.add(code.rsp, frame_info.subtraction);
    }

    // GPRs need to be popped in reverse order
    for (int reg_index = 15; reg_index >= 0; reg_index--) {
        if (regs[reg_index]) {
            code.pop(IndexToReg64(reg_index));
        }
    }
}

} // namespace Common::X64

#endif // LIME3DS_ARCH(x86_64)
