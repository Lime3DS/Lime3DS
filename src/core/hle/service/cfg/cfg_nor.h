// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::CFG {

class CFG_NOR final : public ServiceFramework<CFG_NOR> {
public:
    CFG_NOR();

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::CFG

BOOST_CLASS_EXPORT_KEY(Service::CFG::CFG_NOR)
