// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/pxi/dev.h"

SERIALIZE_EXPORT_IMPL(Service::PXI::DEV)

namespace Service::PXI {

DEV::DEV() : ServiceFramework("pxi:dev", 1) {
    // clang-format off
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "ReadHostIO"},
        {0x0002, nullptr, "WriteHostIO"},
        {0x0003, nullptr, "ReadHostEx"},
        {0x0004, nullptr, "WriteHostEx"},
        {0x0005, nullptr, "WriteHostExStart"},
        {0x0006, nullptr, "WriteHostExChunk"},
        {0x0007, nullptr, "WriteHostExEnd"},
        {0x0008, nullptr, "InitializeMIDI"},
        {0x0009, nullptr, "FinalizeMIDI"},
        {0x000A, nullptr, "GetMIDIInfo"},
        {0x000B, nullptr, "GetMIDIBufferSize"},
        {0x000C, nullptr, "ReadMIDI"},
        {0x000D, nullptr, "SPIMultiWriteRead"},
        {0x000E, nullptr, "SPIWriteRead"},
        {0x000F, nullptr, "GetCardDevice"},
        // clang-format on
    };
    // clang-format on
    RegisterHandlers(functions);
}

DEV::~DEV() = default;

} // namespace Service::PXI
