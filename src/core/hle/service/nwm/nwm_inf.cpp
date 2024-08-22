// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nwm/nwm_inf.h"
#include "core/hle/service/nwm/nwm_uds.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_INF)

namespace Service::NWM {

NWM_INF::RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx) {
    // TODO(PTR) Update implementation to cover differences between NWM_INF and NWM_UDS
    NWM_UDS::RecvBeaconBroadcastData(ctx);
}

NWM_INF::NWM_INF() : ServiceFramework("nwm::INF") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0006, nullptr, "RecvBeaconBroadcastData"},
        {0x0007, nullptr, "ConnectToEncryptedAP"},
        {0x0008, nullptr, "ConnectToAP"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NWM
