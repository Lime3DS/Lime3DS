// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ir/ir_u.h"

SERIALIZE_EXPORT_IMPL(Service::IR::IR_U)

namespace Service::IR {

IR_U::IR_U() : ServiceFramework("ir:u", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "Initialize"},
        {0x0002, nullptr, "Shutdown"},
        {0x0003, nullptr, "StartSendTransfer"},
        {0x0004, nullptr, "WaitSendTransfer"},
        {0x0005, nullptr, "StartRecvTransfer"},
        {0x0006, nullptr, "WaitRecvTransfer"},
        {0x0007, nullptr, "GetRecvTransferCount"},
        {0x0008, nullptr, "GetSendState"},
        {0x0009, nullptr, "SetBitRate"},
        {0x000A, nullptr, "GetBitRate"},
        {0x000B, nullptr, "SetIRLEDState"},
        {0x000C, nullptr, "GetIRLEDRecvState"},
        {0x000D, nullptr, "GetSendFinishedEvent"},
        {0x000E, nullptr, "GetRecvFinishedEvent"},
        {0x000F, nullptr, "GetTransferState"},
        {0x0010, nullptr, "GetErrorStatus"},
        {0x0011, nullptr, "SetSleepModeActive"},
        {0x0012, nullptr, "SetSleepModeState"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::IR
