// Copyright 2024 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/service/mcu/mcu.h"
#include "core/hle/service/mcu/mcu_hwc.h"

namespace Service::MCU {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<HWC>()->InstallAsService(service_manager);
}

} // namespace Service::MCU
