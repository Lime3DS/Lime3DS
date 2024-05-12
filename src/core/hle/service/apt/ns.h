// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NS {

/// Loads and launches the title identified by title_id in the specified media type.
std::shared_ptr<Kernel::Process> LaunchTitle(Core::System& system, FS::MediaType media_type,
                                             u64 title_id);

/// Reboots the system to the specified title.
void RebootToTitle(Core::System& system, FS::MediaType media_type, u64 title_id);

} // namespace Service::NS
