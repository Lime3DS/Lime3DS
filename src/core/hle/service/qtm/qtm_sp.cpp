// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/qtm/qtm_sp.h"

SERIALIZE_EXPORT_IMPL(Service::QTM::QTM_SP)

namespace Service::QTM {

QTM_SP::QTM_SP() : ServiceFramework("qtm:sp", 2) {
    static const FunctionInfo functions[] = {
        // qtm common commands
        // clang-format off
        {0x0001, nullptr, "GetHeadtrackingInfoRaw"},
        {0x0002, nullptr, "GetHeadtrackingInfo"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::QTM
