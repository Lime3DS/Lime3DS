// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/apt/apt.h"

namespace Service::APT {

class APT_A final : public Module::APTInterface {
public:
    explicit APT_A(std::shared_ptr<Module> apt);

private:
    SERVICE_SERIALIZATION(APT_A, apt, Module)
};

} // namespace Service::APT

BOOST_CLASS_EXPORT_KEY(Service::APT::APT_A)
BOOST_SERIALIZATION_CONSTRUCT(Service::APT::APT_A)
