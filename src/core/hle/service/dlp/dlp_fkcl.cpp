// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/dlp/dlp_fkcl.h"

SERIALIZE_EXPORT_IMPL(Service::DLP::DLP_FKCL)

namespace Service::DLP {

DLP_FKCL::DLP_FKCL() : ServiceFramework("dlp:FKCL", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "Initialize"},
        {0x0002, nullptr, "Finalize"},
        {0x0003, nullptr, "GetEventDesc"},
        {0x0004, nullptr, "GetChannels"},
        {0x0005, nullptr, "StartScan"},
        {0x0006, nullptr, "StopScan"},
        {0x0007, nullptr, "GetServerInfo"},
        {0x0008, nullptr, "GetTitleInfo"},
        {0x0009, nullptr, "GetTitleInfoInOrder"},
        {0x000A, nullptr, "DeleteScanInfo"},
        {0x000B, nullptr, "StartFakeSession"},
        {0x000C, nullptr, "GetMyStatus"},
        {0x000D, nullptr, "GetConnectingNodes"},
        {0x000E, nullptr, "GetNodeInfo"},
        {0x000F, nullptr, "GetWirelessRebootPassphrase"},
        {0x0010, nullptr, "StopSession"},
        {0x0011, nullptr, "Initialize2"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
