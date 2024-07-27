// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/hle/service/gsp/gsp_lcd.h"

namespace Service::GSP {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<GSP_GPU>(system)->InstallAsService(service_manager);
    std::make_shared<GSP_LCD>()->InstallAsService(service_manager);
}

} // namespace Service::GSP
