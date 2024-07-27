// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/act/act.h"

namespace Service::ACT {

class ACT_A final : public Module::Interface {
public:
    explicit ACT_A(std::shared_ptr<Module> act);

private:
    SERVICE_SERIALIZATION(ACT_A, act, Module)
};

} // namespace Service::ACT

BOOST_CLASS_EXPORT_KEY(Service::ACT::ACT_A)
BOOST_SERIALIZATION_CONSTRUCT(Service::ACT::ACT_A)
