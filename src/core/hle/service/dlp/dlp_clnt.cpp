// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/dlp/dlp_clnt.h"

SERIALIZE_EXPORT_IMPL(Service::DLP::DLP_CLNT)

namespace Service::DLP {

DLP_CLNT::DLP_CLNT() : ServiceFramework("dlp:CLNT", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "Initialize"},
        {0x0002, nullptr, "Finalize"},
        {0x0003, nullptr, "GetEventDesc"},
        {0x0004, nullptr, "GetChannel"},
        {0x0005, nullptr, "StartScan"},
        {0x0006, nullptr, "StopScan"},
        {0x0007, nullptr, "GetServerInfo"},
        {0x0008, nullptr, "GetTitleInfo"},
        {0x0009, nullptr, "GetTitleInfoInOrder"},
        {0x000A, nullptr, "DeleteScanInfo"},
        {0x000B, nullptr, "PrepareForSystemDownload"},
        {0x000C, nullptr, "StartSystemDownload"},
        {0x000D, nullptr, "StartTitleDownload"},
        {0x000E, nullptr, "GetMyStatus"},
        {0x000F, nullptr, "GetConnectingNodes"},
        {0x0010, nullptr, "GetNodeInfo"},
        {0x0011, nullptr, "GetWirelessRebootPassphrase"},
        {0x0012, nullptr, "StopSession"},
        {0x0013, nullptr, "GetCupVersion"},
        {0x0014, nullptr, "GetDupAvailability"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::DLP
