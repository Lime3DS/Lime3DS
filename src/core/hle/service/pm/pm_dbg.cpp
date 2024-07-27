// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/pm/pm_dbg.h"

SERIALIZE_EXPORT_IMPL(Service::PM::PM_DBG)

namespace Service::PM {

PM_DBG::PM_DBG() : ServiceFramework("pm:dbg", 3) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "LaunchAppDebug"},
        {0x0002, nullptr, "LaunchApp"},
        {0x0003, nullptr, "RunQueuedProcess"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::PM
