// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cam/cam_q.h"

namespace Service::CAM {

CAM_Q::CAM_Q() : ServiceFramework("cam:q", 1 /*TODO: find the true value*/) {
    // Empty arrays are illegal -- commented out until an entry is added.
    // static const FunctionInfo functions[] = {};
    // RegisterHandlers(functions);
}

} // namespace Service::CAM

SERIALIZE_EXPORT_IMPL(Service::CAM::CAM_Q)
