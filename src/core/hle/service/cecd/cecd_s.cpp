// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cecd/cecd_s.h"

SERIALIZE_EXPORT_IMPL(Service::CECD::CECD_S)

namespace Service::CECD {

CECD_S::CECD_S(std::shared_ptr<Module> cecd)
    : Module::Interface(std::move(cecd), "cecd:s", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // cecd:u shared commands
        // clang-format off
        {0x0001, &CECD_S::Open, "Open"},
        {0x0002, &CECD_S::Read, "Read"},
        {0x0003, &CECD_S::ReadMessage, "ReadMessage"},
        {0x0004, &CECD_S::ReadMessageWithHMAC, "ReadMessageWithHMAC"},
        {0x0005, &CECD_S::Write, "Write"},
        {0x0006, &CECD_S::WriteMessage, "WriteMessage"},
        {0x0007, &CECD_S::WriteMessageWithHMAC, "WriteMessageWithHMAC"},
        {0x0008, &CECD_S::Delete, "Delete"},
        {0x0009, &CECD_S::SetData, "SetData"},
        {0x000A, &CECD_S::ReadData, "ReadData"},
        {0x000B, &CECD_S::Start, "Start"},
        {0x000C, &CECD_S::Stop, "Stop"},
        {0x000D, &CECD_S::GetCecInfoBuffer, "GetCecInfoBuffer"},
        {0x000E, &CECD_S::GetCecdState, "GetCecdState"},
        {0x000F, &CECD_S::GetCecInfoEventHandle, "GetCecInfoEventHandle"},
        {0x0010, &CECD_S::GetChangeStateEventHandle, "GetChangeStateEventHandle"},
        {0x0011, &CECD_S::OpenAndWrite, "OpenAndWrite"},
        {0x0012, &CECD_S::OpenAndRead, "OpenAndRead"},
        {0x001E, nullptr, "GetEventLog"},
        {0x001F, nullptr, "GetEventLogStart"},
        // cecd:s commands
        {0x0402, &CECD_S::GetCecInfoEventHandleSys, "GetCecInfoEventHandleSys"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::CECD
