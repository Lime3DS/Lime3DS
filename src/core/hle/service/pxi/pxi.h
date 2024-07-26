// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PXI {

/// Registers all PXI services with the specified service manager.
void InstallInterfaces(Core::System& system);

} // namespace Service::PXI
