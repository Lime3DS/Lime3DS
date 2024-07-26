// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/frd/frd.h"

namespace Service::FRD {

class FRD_A final : public Module::Interface {
public:
    explicit FRD_A(std::shared_ptr<Module> frd);

private:
    SERVICE_SERIALIZATION(FRD_A, frd, Module)
};

} // namespace Service::FRD

BOOST_CLASS_EXPORT_KEY(Service::FRD::FRD_A)
BOOST_SERIALIZATION_CONSTRUCT(Service::FRD::FRD_A)
