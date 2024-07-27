// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/service/mvd/mvd.h"
#include "core/hle/service/mvd/mvd_std.h"

namespace Service::MVD {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<MVD_STD>()->InstallAsService(service_manager);
}

} // namespace Service::MVD
