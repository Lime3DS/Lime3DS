// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::NWM {

class NWM_TST final : public ServiceFramework<NWM_TST> {
public:
    NWM_TST();

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::NWM

BOOST_CLASS_EXPORT_KEY(Service::NWM::NWM_TST)
