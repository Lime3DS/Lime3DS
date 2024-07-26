// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

namespace Kernel {

class ClientPort;
class ServerSession;
class SessionRequestHandler;

class ServerPort final : public WaitObject {
public:
    explicit ServerPort(KernelSystem& kernel);
    ~ServerPort() override;

    std::string GetTypeName() const override {
        return "ServerPort";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ServerPort;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Accepts a pending incoming connection on this port. If there are no pending sessions, will
     * return ResultNoPendingSessions.
     */
    Result Accept(std::shared_ptr<ServerSession>* out_server_session);

    /**
     * Sets the HLE handler template for the port. ServerSessions crated by connecting to this port
     * will inherit a reference to this handler.
     */
    void SetHleHandler(std::shared_ptr<SessionRequestHandler> hle_handler_) {
        hle_handler = std::move(hle_handler_);
    }

    std::string name; ///< Name of port (optional)

    /// ServerSessions waiting to be accepted by the port
    std::vector<std::shared_ptr<ServerSession>> pending_sessions;

    /// This session's HLE request handler template (optional)
    /// ServerSessions created from this port inherit a reference to this handler.
    std::shared_ptr<SessionRequestHandler> hle_handler;

    bool ShouldWait(const Thread* thread) const override;
    void Acquire(Thread* thread) override;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::ServerPort)
CONSTRUCT_KERNEL_OBJECT(Kernel::ServerPort)
