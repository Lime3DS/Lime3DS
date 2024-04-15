// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include "common/archives.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/srv.h"

SERVICE_CONSTRUCT_IMPL(Service::SM::SRV)
SERIALIZE_EXPORT_IMPL(Service::SM::SRV)

namespace Service::SM {

template <class Archive>
void SRV::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar & notification_semaphore;
    ar & get_service_handle_delayed_map;
}

constexpr int MAX_PENDING_NOTIFICATIONS = 16;

/**
 * SRV::RegisterClient service function
 *  Inputs:
 *      0: 0x00010002
 *      1: ProcessId Header (must be 0x20)
 *  Outputs:
 *      0: 0x00010040
 *      1: Result
 */
void SRV::RegisterClient(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const auto pid_descriptor = rp.Pop<u32>();
    if (pid_descriptor != IPC::CallingPidDesc()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(IPC::ResultInvalidBufferDescriptor);
        return;
    }
    const auto caller_pid = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_SRV, "(STUBBED) called. Caller PID={}", caller_pid);
}

/**
 * SRV::EnableNotification service function
 *  Inputs:
 *      0: 0x00020000
 *  Outputs:
 *      0: 0x00020042
 *      1: Result
 *      2: Translation descriptor: 0x20
 *      3: Handle to semaphore signaled on process notification
 */
void SRV::EnableNotification(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    notification_semaphore =
        system.Kernel().CreateSemaphore(0, MAX_PENDING_NOTIFICATIONS, "SRV:Notification").Unwrap();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(notification_semaphore);
    LOG_WARNING(Service_SRV, "(STUBBED) called");
}

class SRV::ThreadCallback : public Kernel::HLERequestContext::WakeupCallback {

public:
    explicit ThreadCallback(Core::System& system_, std::string name_)
        : system(system_), name(name_) {}

    void WakeUp(std::shared_ptr<Kernel::Thread> thread, Kernel::HLERequestContext& ctx,
                Kernel::ThreadWakeupReason reason) {
        LOG_ERROR(Service_SRV, "called service={} wakeup", name);
        std::shared_ptr<Kernel::ClientPort> client_port;
        R_ASSERT(system.ServiceManager().GetServicePort(std::addressof(client_port), name));

        std::shared_ptr<Kernel::ClientSession> session;
        auto result = client_port->Connect(std::addressof(session));
        if (result.IsSuccess()) {
            LOG_DEBUG(Service_SRV, "called service={} -> session={}", name, session->GetObjectId());
            IPC::RequestBuilder rb(ctx, 0x5, 1, 2);
            rb.Push(result);
            rb.PushMoveObjects(std::move(session));
        } else if (result == Kernel::ResultMaxConnectionsReached) {
            LOG_ERROR(Service_SRV, "called service={} -> ResultMaxConnectionsReached", name);
            UNREACHABLE();
        } else {
            LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name, result.raw);
            IPC::RequestBuilder rb(ctx, 0x5, 1, 0);
            rb.Push(result);
        }
    }

private:
    Core::System& system;
    std::string name;

    ThreadCallback() : system(Core::Global<Core::System>()) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::HLERequestContext::WakeupCallback>(*this);
        ar & name;
    }
    friend class boost::serialization::access;
};

/**
 * SRV::GetServiceHandle service function
 *  Inputs:
 *      0: 0x00050100
 *      1-2: 8-byte UTF-8 service name
 *      3: Name length
 *      4: Flags (bit0: if not set, return port-handle if session-handle unavailable)
 *  Outputs:
 *      1: Result
 *      3: Service handle
 */
void SRV::GetServiceHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto name_buf = rp.PopRaw<std::array<char, 8>>();
    std::size_t name_len = rp.Pop<u32>();
    u32 flags = rp.Pop<u32>();

    bool wait_until_available = (flags & 1) == 0;

    if (name_len > Service::kMaxPortSize) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultInvalidNameSize);
        LOG_ERROR(Service_SRV, "called name_len=0x{:X} -> ResultInvalidNameSize", name_len);
        return;
    }
    std::string name(name_buf.data(), name_len);

    // TODO(yuriks): Permission checks go here

    auto get_handle = std::make_shared<ThreadCallback>(system, name);

    std::shared_ptr<Kernel::ClientPort> client_port;
    auto result = system.ServiceManager().GetServicePort(std::addressof(client_port), name);
    if (result.IsError()) {
        if (wait_until_available && result == ResultServiceNotRegistered) {
            LOG_INFO(Service_SRV, "called service={} delayed", name);
            std::shared_ptr<Kernel::Event> get_service_handle_event =
                ctx.SleepClientThread("GetServiceHandle", std::chrono::nanoseconds(-1), get_handle);
            get_service_handle_delayed_map[name] = std::move(get_service_handle_event);
            return;
        } else {
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(result);
            LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name, result.raw);
            return;
        }
    }

    std::shared_ptr<Kernel::ClientSession> session;
    result = client_port->Connect(std::addressof(session));
    if (result.IsSuccess()) {
        LOG_DEBUG(Service_SRV, "called service={} -> session={}", name, session->GetObjectId());
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(result);
        rb.PushMoveObjects(std::move(session));
    } else if (result == Kernel::ResultMaxConnectionsReached && wait_until_available) {
        LOG_WARNING(Service_SRV, "called service={} -> ResultMaxConnectionsReached", name);
        // TODO(Subv): Put the caller guest thread to sleep until this port becomes available again.
        UNIMPLEMENTED_MSG("Unimplemented wait until port {} is available.", name);
    } else {
        LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name, result.raw);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(result);
    }
}

/**
 * SRV::Subscribe service function
 *  Inputs:
 *      0: 0x00090040
 *      1: Notification ID
 *  Outputs:
 *      0: 0x00090040
 *      1: Result
 */
void SRV::Subscribe(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 notification_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x{:X}", notification_id);
}

/**
 * SRV::Unsubscribe service function
 *  Inputs:
 *      0: 0x000A0040
 *      1: Notification ID
 *  Outputs:
 *      0: 0x000A0040
 *      1: Result
 */
void SRV::Unsubscribe(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 notification_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x{:X}", notification_id);
}

/**
 * SRV::PublishToSubscriber service function
 *  Inputs:
 *      0: 0x000C0080
 *      1: Notification ID
 *      2: Flags (bit0: only fire if not fired, bit1: report errors)
 *  Outputs:
 *      0: 0x000C0040
 *      1: Result
 */
void SRV::PublishToSubscriber(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 notification_id = rp.Pop<u32>();
    u8 flags = rp.Pop<u8>();

    // Handle notification 0x203 in HLE, as this one may be used by homebrew to power off the
    // console. Normally, this is handled by NS. If notification handling is properly implemented,
    // this piece of code should be removed, and handled by subscribing from NS instead.
    if (notification_id == 0x203) {
        system.RequestShutdown();
    } else {
        LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x{:X}, flags={}",
                    notification_id, flags);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void SRV::RegisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    auto name_buf = rp.PopRaw<std::array<char, 8>>();
    std::size_t name_len = rp.Pop<u32>();
    u32 max_sessions = rp.Pop<u32>();

    std::string name(name_buf.data(), std::min(name_len, name_buf.size()));

    std::shared_ptr<Kernel::ServerPort> port;
    auto result = system.ServiceManager().RegisterService(std::addressof(port), name, max_sessions);

    if (result.IsError()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(result);
        LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name, result.raw);
        return;
    }

    auto it = get_service_handle_delayed_map.find(name);
    if (it != get_service_handle_delayed_map.end()) {
        it->second->Signal();
        get_service_handle_delayed_map.erase(it);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMoveObjects(std::move(port));
}

SRV::SRV(Core::System& system) : ServiceFramework("srv:", 64), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &SRV::RegisterClient, "RegisterClient"},
        {0x0002, &SRV::EnableNotification, "EnableNotification"},
        {0x0003, &SRV::RegisterService, "RegisterService"},
        {0x0004, nullptr, "UnregisterService"},
        {0x0005, &SRV::GetServiceHandle, "GetServiceHandle"},
        {0x0006, nullptr, "RegisterPort"},
        {0x0007, nullptr, "UnregisterPort"},
        {0x0008, nullptr, "GetPort"},
        {0x0009, &SRV::Subscribe, "Subscribe"},
        {0x000A, &SRV::Unsubscribe, "Unsubscribe"},
        {0x000B, nullptr, "ReceiveNotification"},
        {0x000C, &SRV::PublishToSubscriber, "PublishToSubscriber"},
        {0x000D, nullptr, "PublishAndGetSubscriber"},
        {0x000E, nullptr, "IsServiceRegistered"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

SRV::~SRV() = default;

} // namespace Service::SM

SERIALIZE_EXPORT_IMPL(Service::SM::SRV::ThreadCallback)
