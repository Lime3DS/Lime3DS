// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/cfg/cfg.h"

namespace Service::CFG {

class CFG_S final : public Module::Interface {
public:
    explicit CFG_S(std::shared_ptr<Module> cfg);

private:
    SERVICE_SERIALIZATION(CFG_S, cfg, Module)
};

} // namespace Service::CFG

BOOST_CLASS_EXPORT_KEY(Service::CFG::CFG_S)
BOOST_SERIALIZATION_CONSTRUCT(Service::CFG::CFG_S)
