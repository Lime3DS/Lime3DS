// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cecd/cecd_u.h"

SERIALIZE_EXPORT_IMPL(Service::CECD::CECD_U)

namespace Service::CECD {

CECD_U::CECD_U(std::shared_ptr<Module> cecd)
    : Module::Interface(std::move(cecd), "cecd:u", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // cecd:u shared commands
        // clang-format off
        {0x0001, &CECD_U::Open, "Open"},
        {0x0002, &CECD_U::Read, "Read"},
        {0x0003, &CECD_U::ReadMessage, "ReadMessage"},
        {0x0004, &CECD_U::ReadMessageWithHMAC, "ReadMessageWithHMAC"},
        {0x0005, &CECD_U::Write, "Write"},
        {0x0006, &CECD_U::WriteMessage, "WriteMessage"},
        {0x0007, &CECD_U::WriteMessageWithHMAC, "WriteMessageWithHMAC"},
        {0x0008, &CECD_U::Delete, "Delete"},
        {0x0009, &CECD_U::SetData, "SetData"},
        {0x000A, &CECD_U::ReadData, "ReadData"},
        {0x000B, &CECD_U::Start, "Start"},
        {0x000C, &CECD_U::Stop, "Stop"},
        {0x000D, &CECD_U::GetCecInfoBuffer, "GetCecInfoBuffer"},
        {0x000E, &CECD_U::GetCecdState, "GetCecdState"},
        {0x000F, &CECD_U::GetCecInfoEventHandle, "GetCecInfoEventHandle"},
        {0x0010, &CECD_U::GetChangeStateEventHandle, "GetChangeStateEventHandle"},
        {0x0011, &CECD_U::OpenAndWrite, "OpenAndWrite"},
        {0x0012, &CECD_U::OpenAndRead, "OpenAndRead"},
        {0x001E, nullptr, "GetEventLog"},
        {0x001F, nullptr, "GetEventLogStart"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::CECD
