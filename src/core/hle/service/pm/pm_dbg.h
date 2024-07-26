// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::PM {

class PM_DBG final : public ServiceFramework<PM_DBG> {
public:
    PM_DBG();
    ~PM_DBG() = default;

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::PM

BOOST_CLASS_EXPORT_KEY(Service::PM::PM_DBG)
