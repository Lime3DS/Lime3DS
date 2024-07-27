// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <boost/serialization/export.hpp>
#include "common/assert.h"
#include "common/common_types.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace Kernel {

class ClientSession;
class ClientPort;
class ServerSession;
class Session;
class SessionRequestHandler;
class Thread;

/**
 * Kernel object representing the server endpoint of an IPC session. Sessions are the basic CTR-OS
 * primitive for communication between different processes, and are used to implement service calls
 * to the various system services.
 *
 * To make a service call, the client must write the command header and parameters to the buffer
 * located at offset 0x80 of the TLS (Thread-Local Storage) area, then execute a SendSyncRequest
 * SVC call with its ClientSession handle. The kernel will read the command header, using it to
 * marshall the parameters to the process at the server endpoint of the session.
 * After the server replies to the request, the response is marshalled back to the caller's
 * TLS buffer and control is transferred back to it.
 */
class ServerSession final : public WaitObject {
public:
    ~ServerSession() override;
    explicit ServerSession(KernelSystem& kernel);

    std::string GetName() const override {
        return name;
    }
    std::string GetTypeName() const override {
        return "ServerSession";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ServerSession;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Sets the HLE handler for the session. This handler will be called to service IPC requests
     * instead of the regular IPC machinery. (The regular IPC machinery is currently not
     * implemented.)
     */
    void SetHleHandler(std::shared_ptr<SessionRequestHandler> hle_handler_) {
        hle_handler = std::move(hle_handler_);
    }

    /**
     * Handle a sync request from the emulated application.
     * @param thread Thread that initiated the request.
     * @returns Result from the operation.
     */
    Result HandleSyncRequest(std::shared_ptr<Thread> thread);

    bool ShouldWait(const Thread* thread) const override;

    void Acquire(Thread* thread) override;

    std::string name;                ///< The name of this session (optional)
    std::shared_ptr<Session> parent; ///< The parent session, which links to the client endpoint.
    std::shared_ptr<SessionRequestHandler>
        hle_handler; ///< This session's HLE request handler (optional)

    /// List of threads that are pending a response after a sync request. This list is processed in
    /// a LIFO manner, thus, the last request will be dispatched first.
    /// TODO(Subv): Verify if this is indeed processed in LIFO using a hardware test.
    std::vector<std::shared_ptr<Thread>> pending_requesting_threads;

    /// Thread whose request is currently being handled. A request is considered "handled" when a
    /// response is sent via svcReplyAndReceive.
    /// TODO(Subv): Find a better name for this.
    std::shared_ptr<Thread> currently_handling;

    /// A temporary list holding mapped buffer info from IPC request, used for during IPC reply
    std::vector<MappedBufferContext> mapped_buffer_context;

private:
    /**
     * Creates a server session. The server session can have an optional HLE handler,
     * which will be invoked to handle the IPC requests that this session receives.
     * @param kernel The kernel instance to create the server session on
     * @param name Optional name of the server session.
     * @return The created server session
     */
    static ResultVal<std::shared_ptr<ServerSession>> Create(KernelSystem& kernel,
                                                            std::string name = "Unknown");

    friend class KernelSystem;
    KernelSystem& kernel;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::ServerSession)
CONSTRUCT_KERNEL_OBJECT(Kernel::ServerSession)
