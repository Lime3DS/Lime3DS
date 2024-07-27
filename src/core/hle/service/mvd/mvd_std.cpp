// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/mvd/mvd_std.h"

SERIALIZE_EXPORT_IMPL(Service::MVD::MVD_STD)

namespace Service::MVD {

MVD_STD::MVD_STD() : ServiceFramework("mvd:std", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "Initialize"},
        {0x0002, nullptr, "Shutdown"},
        {0x0003, nullptr, "CalculateWorkBufSize"},
        {0x0004, nullptr, "CalculateImageSize"},
        {0x0008, nullptr, "ProcessNALUnit"},
        {0x0009, nullptr, "ControlFrameRendering"},
        {0x000A, nullptr, "GetStatus"},
        {0x000B, nullptr, "GetStatusOther"},
        {0x001D, nullptr, "GetConfig"},
        {0x001E, nullptr, "SetConfig"},
        {0x001F, nullptr, "SetOutputBuffer"},
        {0x0021, nullptr, "OverrideOutputBuffers"}
        // clang-format on
    };

    RegisterHandlers(functions);
};

} // namespace Service::MVD
