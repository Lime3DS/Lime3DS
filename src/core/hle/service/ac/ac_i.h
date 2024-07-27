// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/ac/ac.h"

namespace Service::AC {

class AC_I final : public Module::Interface {
public:
    explicit AC_I(std::shared_ptr<Module> ac);

private:
    SERVICE_SERIALIZATION(AC_I, ac, Module)
};

} // namespace Service::AC

BOOST_CLASS_EXPORT_KEY(Service::AC::AC_I)
BOOST_SERIALIZATION_CONSTRUCT(Service::AC::AC_I)
