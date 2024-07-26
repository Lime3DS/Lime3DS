// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::GSP {

class GSP_LCD final : public ServiceFramework<GSP_LCD> {
public:
    GSP_LCD();
    ~GSP_LCD() = default;

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::GSP

BOOST_CLASS_EXPORT_KEY(Service::GSP::GSP_LCD)
