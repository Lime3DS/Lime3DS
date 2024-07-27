// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/hle/service/dlp/dlp.h"
#include "core/hle/service/dlp/dlp_clnt.h"
#include "core/hle/service/dlp/dlp_fkcl.h"
#include "core/hle/service/dlp/dlp_srvr.h"

namespace Service::DLP {

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<DLP_CLNT>()->InstallAsService(service_manager);
    std::make_shared<DLP_FKCL>()->InstallAsService(service_manager);
    std::make_shared<DLP_SRVR>()->InstallAsService(service_manager);
}

} // namespace Service::DLP
