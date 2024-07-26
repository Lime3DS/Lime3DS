// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/qtm/qtm_c.h"

SERIALIZE_EXPORT_IMPL(Service::QTM::QTM_C)

namespace Service::QTM {

QTM_C::QTM_C() : ServiceFramework("qtm:c", 2) {
    static const FunctionInfo functions[] = {
        // qtm calibration commands
        // clang-format off
        {0x0001, nullptr, "InitializeHardwareCheck"},
        {0x0005, nullptr, "SetIrLedCheck"},
        // clang-format on
    };

    RegisterHandlers(functions);
};

} // namespace Service::QTM
