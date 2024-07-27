// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/ptm/ptm.h"

namespace Service::PTM {

class PTM_S_Common : public Module::Interface {
public:
    explicit PTM_S_Common(std::shared_ptr<Module> ptm, const char* name);
};

class PTM_S final : public PTM_S_Common {
public:
    explicit PTM_S(std::shared_ptr<Module> ptm);

private:
    SERVICE_SERIALIZATION(PTM_S, ptm, Module)
};

class PTM_Sysm final : public PTM_S_Common {
public:
    explicit PTM_Sysm(std::shared_ptr<Module> ptm);

private:
    SERVICE_SERIALIZATION(PTM_Sysm, ptm, Module)
};

} // namespace Service::PTM

BOOST_CLASS_EXPORT_KEY(Service::PTM::PTM_S)
BOOST_CLASS_EXPORT_KEY(Service::PTM::PTM_Sysm)
BOOST_SERIALIZATION_CONSTRUCT(Service::PTM::PTM_S)
BOOST_SERIALIZATION_CONSTRUCT(Service::PTM::PTM_Sysm)
