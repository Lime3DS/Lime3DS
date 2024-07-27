// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::IR {

/// Interface to "ir:u" service
class IR_U final : public ServiceFramework<IR_U> {
public:
    IR_U();

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::IR

BOOST_CLASS_EXPORT_KEY(Service::IR::IR_U)
