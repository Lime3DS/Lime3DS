// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/core.h"
#include "core/hle/service/nwm/nwm_inf.h"
#include "core/hle/service/nwm/nwm_uds.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_INF)

namespace Service::NWM {

void NWM_INF::RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx) {
    // TODO(PTR) Update implementation to cover differences between NWM_INF and NWM_UDS

    // adding in extra context value for transition from INF to UDS
    u32* ctx_data = ctx.CommandBuffer();
    std::array<u32, IPC::COMMAND_BUFFER_LENGTH + 2 * IPC::MAX_STATIC_BUFFERS> cmd_buf;
    int i;
    for (i = 0; i < 15; i++) {
        cmd_buf[i] = ctx_data[i];
    }
    cmd_buf[15] = 0;    // dummy wlan_comm_id
    cmd_buf[16] = 0;    // dummy id
    for (i = 17; i <= 20; i++) {
        cmd_buf[i] = ctx_data[i - 1];
    }

    Kernel::KernelSystem kernel = ctx.kernel;
    std::shared_ptr<Thread> thread = ctx.ClientThread();
    auto current_process = thread->owner_process.lock();
    auto context =
            std::make_shared<Kernel::HLERequestContext>(kernel, SharedFrom(this), /* TODO */);
    context->PopulateFromIncomingCommandBuffer(cmd_buf.data(), current_process);

    auto nwm_uds = Core::System::GetInstance().ServiceManager().GetService<Service::NWM::NWM_UDS>("nwm::UDS");
    nwm_uds->HandleSyncRequest(context);
}

NWM_INF::NWM_INF() : ServiceFramework("nwm::INF") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0006, &NWM_INF::RecvBeaconBroadcastData, "RecvBeaconBroadcastData"},
        {0x0007, nullptr, "ConnectToEncryptedAP"},
        {0x0008, nullptr, "ConnectToAP"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NWM
