// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/am/am.h"

namespace Service::AM {

class AM_SYS final : public Module::Interface {
public:
    explicit AM_SYS(std::shared_ptr<Module> am);

private:
    SERVICE_SERIALIZATION(AM_SYS, am, Module)
};

} // namespace Service::AM

BOOST_CLASS_EXPORT_KEY(Service::AM::AM_SYS)
BOOST_SERIALIZATION_CONSTRUCT(Service::AM::AM_SYS)
