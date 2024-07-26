// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/ptm/ptm.h"

namespace Service::PTM {

class PTM_U final : public Module::Interface {
public:
    explicit PTM_U(std::shared_ptr<Module> ptm);

private:
    SERVICE_SERIALIZATION(PTM_U, ptm, Module)
};

} // namespace Service::PTM

BOOST_CLASS_EXPORT_KEY(Service::PTM::PTM_U)
BOOST_SERIALIZATION_CONSTRUCT(Service::PTM::PTM_U)
