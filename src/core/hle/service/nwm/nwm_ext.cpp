// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_ext.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_EXT)

namespace Service::NWM {

NWM_EXT::NWM_EXT() : ServiceFramework("nwm::EXT") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0008, nullptr, "ControlWirelessEnabled"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NWM
