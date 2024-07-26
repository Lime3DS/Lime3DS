// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/archives.h"
#include "core/file_sys/delay_generator.h"

SERIALIZE_EXPORT_IMPL(FileSys::DefaultDelayGenerator)

namespace FileSys {

DelayGenerator::~DelayGenerator() = default;

u64 DefaultDelayGenerator::GetReadDelayNs(std::size_t length) {
    // This is the delay measured for a romfs read.
    // For now we will take that as a default
    static constexpr u64 slope(94);
    static constexpr u64 offset(582778);
    static constexpr u64 minimum(663124);
    u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
    return IPCDelayNanoseconds;
}

u64 DefaultDelayGenerator::GetOpenDelayNs() {
    // This is the delay measured for a romfs open.
    // For now we will take that as a default
    static constexpr u64 IPCDelayNanoseconds(9438006);
    return IPCDelayNanoseconds;
}

} // namespace FileSys
