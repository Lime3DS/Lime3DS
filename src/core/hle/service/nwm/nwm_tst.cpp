// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_tst.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_TST)

namespace Service::NWM {

NWM_TST::NWM_TST() : ServiceFramework("nwm::TST") {
    /*
    static const FunctionInfo functions[] = {
    };
    RegisterHandlers(functions);
    */
}

} // namespace Service::NWM
