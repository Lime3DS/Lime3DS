// Copyright 2013 Dolphin Emulator Project / 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/arch.h"
#if LIME3DS_ARCH(x86_64)

#include <cstring>
#include "common/common_types.h"
#include "common/x64/cpu_detect.h"

#ifdef _MSC_VER
#include <intrin.h>
#else

#if defined(__DragonFly__) || defined(__FreeBSD__)
// clang-format off
#include <sys/types.h>
#include <machine/cpufunc.h>
// clang-format on
#endif

static inline void __cpuidex(int info[4], int function_id, int subfunction_id) {
#if defined(__DragonFly__) || defined(__FreeBSD__)
    // Despite the name, this is just do_cpuid() with ECX as second input.
    cpuid_count((u_int)function_id, (u_int)subfunction_id, (u_int*)info);
#else
    info[0] = function_id;    // eax
    info[2] = subfunction_id; // ecx
    __asm__("cpuid"
            : "=a"(info[0]), "=b"(info[1]), "=c"(info[2]), "=d"(info[3])
            : "a"(function_id), "c"(subfunction_id));
#endif
}

static inline void __cpuid(int info[4], int function_id) {
    return __cpuidex(info, function_id, 0);
}

#define _XCR_XFEATURE_ENABLED_MASK 0
static inline u64 _xgetbv(u32 index) {
    u32 eax, edx;
    __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
    return ((u64)edx << 32) | eax;
}

#endif // _MSC_VER

namespace Common {

// Detects the various CPU features
static CPUCaps Detect() {
    CPUCaps caps = {};

    // Assumes the CPU supports the CPUID instruction. Those that don't would likely not support
    // Lime3DS at all anyway

    int cpu_id[4];
    std::memset(caps.brand_string, 0, sizeof(caps.brand_string));

    // Detect CPU's CPUID capabilities and grab CPU string
    __cpuid(cpu_id, 0x00000000);
    u32 max_std_fn = cpu_id[0]; // EAX

    std::memcpy(&caps.brand_string[0], &cpu_id[1], sizeof(int));
    std::memcpy(&caps.brand_string[4], &cpu_id[3], sizeof(int));
    std::memcpy(&caps.brand_string[8], &cpu_id[2], sizeof(int));

    __cpuid(cpu_id, 0x80000000);

    u32 max_ex_fn = cpu_id[0];

    // Set reasonable default brand string even if brand string not available
    strcpy(caps.cpu_string, caps.brand_string);

    // Detect family and other miscellaneous features
    if (max_std_fn >= 1) {
        __cpuid(cpu_id, 0x00000001);

        if ((cpu_id[3] >> 25) & 1)
            caps.sse = true;
        if ((cpu_id[3] >> 26) & 1)
            caps.sse2 = true;
        if ((cpu_id[2]) & 1)
            caps.sse3 = true;
        if ((cpu_id[2] >> 9) & 1)
            caps.ssse3 = true;
        if ((cpu_id[2] >> 19) & 1)
            caps.sse4_1 = true;
        if ((cpu_id[2] >> 20) & 1)
            caps.sse4_2 = true;
        if ((cpu_id[2] >> 25) & 1)
            caps.aes = true;

        // AVX support requires 3 separate checks:
        //  - Is the AVX bit set in CPUID?
        //  - Is the XSAVE bit set in CPUID?
        //  - XGETBV result has the XCR bit set.
        if (((cpu_id[2] >> 28) & 1) && ((cpu_id[2] >> 27) & 1)) {
            if ((_xgetbv(_XCR_XFEATURE_ENABLED_MASK) & 0x6) == 0x6) {
                caps.avx = true;
                if ((cpu_id[2] >> 12) & 1)
                    caps.fma = true;
            }
        }

        if (max_std_fn >= 7) {
            __cpuidex(cpu_id, 0x00000007, 0x00000000);
            // Can't enable AVX2 unless the XSAVE/XGETBV checks above passed
            if ((cpu_id[1] >> 5) & 1)
                caps.avx2 = caps.avx;
            if ((cpu_id[1] >> 3) & 1)
                caps.bmi1 = true;
            if ((cpu_id[1] >> 8) & 1)
                caps.bmi2 = true;
            // Checks for AVX512F, AVX512CD, AVX512VL, AVX512DQ, AVX512BW (Intel Skylake-X/SP)
            if ((cpu_id[1] >> 16) & 1 && (cpu_id[1] >> 28) & 1 && (cpu_id[1] >> 31) & 1 &&
                (cpu_id[1] >> 17) & 1 && (cpu_id[1] >> 30) & 1) {
                caps.avx512 = caps.avx2;
            }
        }
    }

    if (max_ex_fn >= 0x80000004) {
        // Extract CPU model string
        __cpuid(cpu_id, 0x80000002);
        std::memcpy(caps.cpu_string, cpu_id, sizeof(cpu_id));
        __cpuid(cpu_id, 0x80000003);
        std::memcpy(caps.cpu_string + 16, cpu_id, sizeof(cpu_id));
        __cpuid(cpu_id, 0x80000004);
        std::memcpy(caps.cpu_string + 32, cpu_id, sizeof(cpu_id));
    }

    if (max_ex_fn >= 0x80000001) {
        // Check for more features
        __cpuid(cpu_id, 0x80000001);
        if ((cpu_id[2] >> 16) & 1)
            caps.fma4 = true;
    }

    return caps;
}

const CPUCaps& GetCPUCaps() {
    static CPUCaps caps = Detect();
    return caps;
}

} // namespace Common

#endif // LIME3DS_ARCH(x86_64)
