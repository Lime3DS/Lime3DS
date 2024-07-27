// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_soc.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_SOC)

namespace Service::NWM {

NWM_SOC::NWM_SOC() : ServiceFramework("nwm::SOC") {
    /*
    static const FunctionInfo functions[] = {
    };
    RegisterHandlers(functions);
    */
}

} // namespace Service::NWM
