// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/cecd/cecd.h"

namespace Service::CECD {

class CECD_U final : public Module::Interface {
public:
    explicit CECD_U(std::shared_ptr<Module> cecd);

private:
    SERVICE_SERIALIZATION(CECD_U, cecd, Module)
};

} // namespace Service::CECD

BOOST_CLASS_EXPORT_KEY(Service::CECD::CECD_U)
BOOST_SERIALIZATION_CONSTRUCT(Service::CECD::CECD_U)
