// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ndm/ndm_u.h"

SERIALIZE_EXPORT_IMPL(Service::NDM::NDM_U)

namespace Service::NDM {

void NDM_U::EnterExclusiveState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    exclusive_state = rp.PopEnum<ExclusiveState>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED) exclusive_state=0x{:08X}", exclusive_state);
}

void NDM_U::LeaveExclusiveState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::QueryExclusiveMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(exclusive_state);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::LockState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.PopPID();
    daemon_lock_enabled = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::UnlockState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.PopPID();
    daemon_lock_enabled = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SuspendDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    daemon_bit_mask =
        static_cast<DaemonMask>(static_cast<u32>(default_daemon_bit_mask) & ~bit_mask);
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Suspended;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::ResumeDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    daemon_bit_mask = static_cast<DaemonMask>(static_cast<u32>(daemon_bit_mask) & ~bit_mask);
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::SuspendScheduler(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    bool perform_in_background = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED) perform_in_background={}", perform_in_background);
}

void NDM_U::ResumeScheduler(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::QueryStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 daemon = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(daemon_status.at(daemon));
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x{:02X}", daemon);
}

void NDM_U::GetDaemonDisableCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 daemon = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(0); // current process disable count
    rb.Push<u32>(0); // total disable count
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x{:02X}", daemon);
}

void NDM_U::GetSchedulerDisableCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(0); // current process disable count
    rb.Push<u32>(0); // total disable count
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SetScanInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    scan_interval = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED) scan_interval=0x{:08X}", scan_interval);
}

void NDM_U::GetScanInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(scan_interval);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SetRetryInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    retry_interval = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED) retry_interval=0x{:08X}", retry_interval);
}

void NDM_U::GetRetryInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(retry_interval);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::OverrideDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    default_daemon_bit_mask = static_cast<DaemonMask>(bit_mask);
    daemon_bit_mask = default_daemon_bit_mask;
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::ResetDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    default_daemon_bit_mask = DaemonMask::Default;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::GetDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(default_daemon_bit_mask);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::ClearHalfAwakeMacFilter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

NDM_U::NDM_U() : ServiceFramework("ndm:u", 6) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &NDM_U::EnterExclusiveState, "EnterExclusiveState"},
        {0x0002, &NDM_U::LeaveExclusiveState, "LeaveExclusiveState"},
        {0x0003, &NDM_U::QueryExclusiveMode, "QueryExclusiveMode"},
        {0x0004, &NDM_U::LockState, "LockState"},
        {0x0005, &NDM_U::UnlockState, "UnlockState"},
        {0x0006, &NDM_U::SuspendDaemons, "SuspendDaemons"},
        {0x0007, &NDM_U::ResumeDaemons, "ResumeDaemons"},
        {0x0008, &NDM_U::SuspendScheduler, "SuspendScheduler"},
        {0x0009, &NDM_U::ResumeScheduler, "ResumeScheduler"},
        {0x000A, nullptr, "GetCurrentState"},
        {0x000B, nullptr, "GetTargetState"},
        {0x000C, nullptr, "<Stubbed>"},
        {0x000D, &NDM_U::QueryStatus, "QueryStatus"},
        {0x000E, &NDM_U::GetDaemonDisableCount, "GetDaemonDisableCount"},
        {0x000F, &NDM_U::GetSchedulerDisableCount, "GetSchedulerDisableCount"},
        {0x0010, &NDM_U::SetScanInterval, "SetScanInterval"},
        {0x0011, &NDM_U::GetScanInterval, "GetScanInterval"},
        {0x0012, &NDM_U::SetRetryInterval, "SetRetryInterval"},
        {0x0013, &NDM_U::GetRetryInterval, "GetRetryInterval"},
        {0x0014, &NDM_U::OverrideDefaultDaemons, "OverrideDefaultDaemons"},
        {0x0015, &NDM_U::ResetDefaultDaemons, "ResetDefaultDaemons"},
        {0x0016, &NDM_U::GetDefaultDaemons, "GetDefaultDaemons"},
        {0x0017, &NDM_U::ClearHalfAwakeMacFilter, "ClearHalfAwakeMacFilter"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<NDM_U>()->InstallAsService(service_manager);
}

} // namespace Service::NDM
