// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/pm/pm_app.h"

SERIALIZE_EXPORT_IMPL(Service::PM::PM_APP)

namespace Service::PM {

PM_APP::PM_APP() : ServiceFramework("pm:app", 3) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "LaunchTitle"},
        {0x0002, nullptr, "LaunchFIRM"},
        {0x0003, nullptr, "TerminateApplication"},
        {0x0004, nullptr, "TerminateTitle"},
        {0x0005, nullptr, "TerminateProcess"},
        {0x0006, nullptr, "PrepareForReboot"},
        {0x0007, nullptr, "GetFIRMLaunchParams"},
        {0x0008, nullptr, "GetTitleExheaderFlags"},
        {0x0009, nullptr, "SetFIRMLaunchParams"},
        {0x000A, nullptr, "SetAppResourceLimit"},
        {0x000B, nullptr, "GetAppResourceLimit"},
        {0x000C, nullptr, "UnregisterProcess"},
        {0x000D, nullptr, "LaunchTitleUpdate"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::PM
