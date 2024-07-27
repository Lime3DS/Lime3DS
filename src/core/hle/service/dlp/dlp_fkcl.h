// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::DLP {

class DLP_FKCL final : public ServiceFramework<DLP_FKCL> {
public:
    DLP_FKCL();
    ~DLP_FKCL() = default;

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::DLP

BOOST_CLASS_EXPORT_KEY(Service::DLP::DLP_FKCL)
