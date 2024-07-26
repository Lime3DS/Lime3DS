// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/qtm/qtm_s.h"

SERIALIZE_EXPORT_IMPL(Service::QTM::QTM_S)

namespace Service::QTM {

void QTM_S::GetHeadtrackingInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const u64 unknown = rp.Pop<u64>();

    std::array<u8, 0x40> data{};
    IPC::RequestBuilder rb = rp.MakeBuilder(17, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw<std::array<u8, 0x40>>(data);

    LOG_DEBUG(Service, "(STUBBED) called");
}

QTM_S::QTM_S() : ServiceFramework("qtm:s", 2) {
    static const FunctionInfo functions[] = {
        // qtm common commands
        // clang-format off
        {0x0001, nullptr, "GetHeadtrackingInfoRaw"},
        {0x0002, &QTM_S::GetHeadtrackingInfo, "GetHeadtrackingInfo"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::QTM
