// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::NWM {

class NWM_INF final : public ServiceFramework<NWM_INF> {
public:
    NWM_INF();

    /**
     * NWM::RecvBeaconBroadcastData service function
     *  Inputs:
     *      1 : Output buffer max size
     *      2-14 : Input ScanInputStruct.
     *      15 : u32, unknown
     *      16 : Value 0x0
     *      17 : Input handle
     *      18 : (Size<<4) | 12
     *      19 : Output BeaconDataReply buffer ptr
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx);

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::NWM

BOOST_CLASS_EXPORT_KEY(Service::NWM::NWM_INF)
