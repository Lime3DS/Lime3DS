// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <boost/container/flat_map.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "common/common_types.h"
#include "common/construct.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/object.h"
#include "core/hle/service/sm/sm.h"

namespace Core {
class System;
}

namespace Kernel {
class KernelSystem;
class ClientPort;
class ServerPort;
class ServerSession;
} // namespace Kernel

namespace Service {

namespace SM {
class ServiceManager;
}

static const int kMaxPortSize = 8; ///< Maximum size of a port name (8 characters)
/// Arbitrary default number of maximum connections to an HLE service.
static const u32 DefaultMaxSessions = 10;

/**
 * This is an non-templated base of ServiceFramework to reduce code bloat and compilation times, it
 * is not meant to be used directly.
 *
 * @see ServiceFramework
 */
class ServiceFrameworkBase : public Kernel::SessionRequestHandler {
public:
    /// Returns the string identifier used to connect to the service.
    std::string GetServiceName() const {
        return service_name;
    }

    /**
     * Returns the maximum number of sessions that can be connected to this service at the same
     * time.
     */
    u32 GetMaxSessions() const {
        return max_sessions;
    }

    /// Creates a port pair and registers this service with the given ServiceManager.
    void InstallAsService(SM::ServiceManager& service_manager);
    /// Creates a port pair and registers it on the kernel's global port registry.
    void InstallAsNamedPort(Kernel::KernelSystem& kernel);

    void HandleSyncRequest(Kernel::HLERequestContext& context) override;

    /// Retrieves name of a function based on the header code. For IPC Recorder.
    std::string GetFunctionName(IPC::Header header) const;

protected:
    /// Member-function pointer type of SyncRequest handlers.
    template <typename Self>
    using HandlerFnP = void (Self::*)(Kernel::HLERequestContext&);

private:
    template <typename T, typename SessionData>
    friend class ServiceFramework;

    struct FunctionInfoBase {
        u32 command_id;
        HandlerFnP<ServiceFrameworkBase> handler_callback;
        const char* name;
    };

    using InvokerFn = void(ServiceFrameworkBase* object, HandlerFnP<ServiceFrameworkBase> member,
                           Kernel::HLERequestContext& ctx);

    // TODO: Replace all these with virtual functions!
    ServiceFrameworkBase(const char* service_name, u32 max_sessions, InvokerFn* handler_invoker);
    ~ServiceFrameworkBase() override;

    void RegisterHandlersBase(const FunctionInfoBase* functions, std::size_t n);
    void ReportUnimplementedFunction(u32* cmd_buf, const FunctionInfoBase* info);

    /// Identifier string used to connect to the service.
    std::string service_name;
    /// Maximum number of concurrent sessions that this service can handle.
    u32 max_sessions;

    /// Function used to safely up-cast pointers to the derived class before invoking a handler.
    InvokerFn* handler_invoker;
    boost::container::flat_map<u32, FunctionInfoBase> handlers;
};

/**
 * Framework for implementing HLE services. Dispatches on the header id of incoming SyncRequests
 * based on a table mapping header ids to handler functions. Service implementations should inherit
 * from ServiceFramework using the CRTP (`class Foo : public ServiceFramework<Foo> { ... };`) and
 * populate it with handlers by calling #RegisterHandlers.
 *
 * In order to avoid duplicating code in the binary and exposing too many implementation details in
 * the header, this class is split into a non-templated base (ServiceFrameworkBase) and a template
 * deriving from it (ServiceFramework). The functions in this class will mostly only erase the type
 * of the passed in function pointers and then delegate the actual work to the implementation in the
 * base class.
 */
template <typename Self, typename SessionData = Kernel::SessionRequestHandler::SessionDataBase>
class ServiceFramework : public ServiceFrameworkBase {
protected:
    /// Contains information about a request type which is handled by the service.
    struct FunctionInfo : FunctionInfoBase {
        /**
         * Constructs a FunctionInfo for a function.
         *
         * @param command_id command ID in the command buffer which will trigger dispatch
         *     to this handler
         * @param handler_callback member function in this service which will be called to handle
         *     the request
         * @param name human-friendly name for the request. Used mostly for logging purposes.
         */
        constexpr FunctionInfo(u32 command_id, HandlerFnP<Self> handler_callback, const char* name)
            : FunctionInfoBase{
                  command_id,
                  // Type-erase member function pointer by casting it down to the base class.
                  static_cast<HandlerFnP<ServiceFrameworkBase>>(handler_callback), name} {}
    };

    /**
     * Initializes the handler with no functions installed.
     * @param max_sessions Maximum number of sessions that can be
     * connected to this service at the same time.
     */
    explicit ServiceFramework(const char* service_name, u32 max_sessions = DefaultMaxSessions)
        : ServiceFrameworkBase(service_name, max_sessions, Invoker) {}

    /// Registers handlers in the service.
    template <std::size_t N>
    void RegisterHandlers(const FunctionInfo (&functions)[N]) {
        RegisterHandlers(functions, N);
    }

    /**
     * Registers handlers in the service. Usually prefer using the other RegisterHandlers
     * overload in order to avoid needing to specify the array size.
     */
    void RegisterHandlers(const FunctionInfo* functions, std::size_t n) {
        RegisterHandlersBase(functions, n);
    }

    std::unique_ptr<SessionDataBase> MakeSessionData() override {
        return std::make_unique<SessionData>();
    }

    SessionData* GetSessionData(std::shared_ptr<Kernel::ServerSession> server_session) {
        return ServiceFrameworkBase::GetSessionData<SessionData>(server_session);
    }

private:
    /**
     * This function is used to allow invocation of pointers to handlers stored in the base class
     * without needing to expose the type of this derived class. Pointers-to-member may require a
     * fixup when being up or downcast, and thus code that does that needs to know the concrete type
     * of the derived class in order to invoke one of it's functions through a pointer.
     */
    static void Invoker(ServiceFrameworkBase* object, HandlerFnP<ServiceFrameworkBase> member,
                        Kernel::HLERequestContext& ctx) {
        // Cast back up to our original types and call the member function
        (static_cast<Self*>(object)->*static_cast<HandlerFnP<Self>>(member))(ctx);
    }
};

/// Initialize ServiceManager
void Init(Core::System& system);

struct ServiceModuleInfo {
    std::string name;
    u64 title_id;
    std::function<void(Core::System&)> init_function;
};

extern const std::array<ServiceModuleInfo, 41> service_module_map;

} // namespace Service

#define SERVICE_SERIALIZATION(T, MFIELD, TMODULE)                                                  \
    template <class Archive>                                                                       \
    void save_construct(Archive& ar, const unsigned int file_version) const {                      \
        ar << MFIELD;                                                                              \
    }                                                                                              \
                                                                                                   \
    template <class Archive>                                                                       \
    static void load_construct(Archive& ar, T* t, const unsigned int file_version) {               \
        std::shared_ptr<TMODULE> MFIELD;                                                           \
        ar >> MFIELD;                                                                              \
        ::new (t) T(MFIELD);                                                                       \
    }                                                                                              \
                                                                                                   \
    template <class Archive>                                                                       \
    void serialize(Archive& ar, const unsigned int) {                                              \
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);               \
    }                                                                                              \
    friend class boost::serialization::access;                                                     \
    friend class ::construct_access;

#define SERVICE_SERIALIZATION_SIMPLE                                                               \
    template <class Archive>                                                                       \
    void serialize(Archive& ar, const unsigned int) {                                              \
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);               \
    }                                                                                              \
    friend class boost::serialization::access;

#define SERVICE_CONSTRUCT(T)                                                                       \
    namespace boost::serialization {                                                               \
    template <class Archive>                                                                       \
    void load_construct_data(Archive& ar, T* t, const unsigned int);                               \
    }

#define SERVICE_CONSTRUCT_IMPL(T)                                                                  \
    namespace boost::serialization {                                                               \
    template <class Archive>                                                                       \
    void load_construct_data(Archive& ar, T* t, const unsigned int) {                              \
        ::new (t) T(Core::Global<Core::System>());                                                 \
    }                                                                                              \
    template void load_construct_data<iarchive>(iarchive & ar, T* t, const unsigned int);          \
    }
