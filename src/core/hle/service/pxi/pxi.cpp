// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/service/pxi/dev.h"
#include "core/hle/service/pxi/pxi.h"

namespace Service::PXI {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<DEV>()->InstallAsService(service_manager);
}

} // namespace Service::PXI
