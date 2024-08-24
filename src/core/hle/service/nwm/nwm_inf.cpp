// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/shared_ptr.hpp>
#include "common/archives.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/nwm/nwm_inf.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/hle/service/nwm/uds_beacon.h"
#include "core/hle/service/nwm/uds_connection.h"
#include "core/hle/service/nwm/uds_data.h"

SERIALIZE_EXPORT_IMPL(Service::NWM::NWM_INF)

namespace Service::NWM {

void NWM_INF::RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    // Using a standalone implementation requires copying private methods as well
    // I am not sure if that should be done

    // Replacing header,
    // adding in extra context value
    // and replacing dummy value with 2 dummy id's
    std::array<u32, IPC::COMMAND_BUFFER_LENGTH + 2 * IPC::MAX_STATIC_BUFFERS> cmd_buf;
    cmd_buf[0] = 0x000F0404;
    int i;
    for (i = 1; i < 15; i++) {
        cmd_buf[i] = rp.Pop<u32>();
    }
    rp.Pop<u32>();
    cmd_buf[15] = 0; // dummy wlan_comm_id
    cmd_buf[16] = 0; // dummy id
    for (i = 17; i <= 20; i++) {
        cmd_buf[i] = rp.Pop<u32>();
    }

    // Prepare for call to NWM_UDS
    std::shared_ptr<Kernel::Thread> thread = ctx.ClientThread();
    auto current_process = thread->owner_process.lock();
    auto context = std::make_shared<Kernel::HLERequestContext>(Core::System::GetInstance().Kernel(),
                                                               ctx.Session(), thread);
    context->PopulateFromIncomingCommandBuffer(cmd_buf.data(), current_process);

    auto nwm_uds =
        Core::System::GetInstance().ServiceManager().GetService<Service::NWM::NWM_UDS>("nwm::UDS");
    nwm_uds->HandleSyncRequest(*context);

    // Push results of delegated call to caller
    IPC::RequestParser rp2(*context);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(rp2.Pop<u32>());
    rb.PushMappedBuffer(rp2.PopMappedBuffer());
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
