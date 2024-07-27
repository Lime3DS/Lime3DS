// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::PXI {

/// Interface to "pxi:dev" service
class DEV final : public ServiceFramework<DEV> {
public:
    DEV();
    ~DEV();

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::PXI

BOOST_CLASS_EXPORT_KEY(Service::PXI::DEV)
