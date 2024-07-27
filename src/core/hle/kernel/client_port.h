// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/result.h"

namespace Kernel {

class ClientSession;

class ClientPort final : public Object {
public:
    explicit ClientPort(KernelSystem& kernel);
    ~ClientPort() override;

    friend class ServerPort;
    std::string GetTypeName() const override {
        return "ClientPort";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::ClientPort;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::shared_ptr<ServerPort> GetServerPort() const {
        return server_port;
    }

    /**
     * Creates a new Session pair, adds the created ServerSession to the associated ServerPort's
     * list of pending sessions, and signals the ServerPort, causing any threads
     * waiting on it to awake.
     * @returns ClientSession The client endpoint of the created Session pair, or error code.
     */
    Result Connect(std::shared_ptr<ClientSession>* out_client_session);

    /**
     * Signifies that a previously active connection has been closed,
     * decreasing the total number of active connections to this port.
     */
    void ConnectionClosed();

private:
    KernelSystem& kernel;
    std::shared_ptr<ServerPort> server_port; ///< ServerPort associated with this client port.
    u32 max_sessions = 0;    ///< Maximum number of simultaneous sessions the port can have
    u32 active_sessions = 0; ///< Number of currently open sessions to this port
    std::string name;        ///< Name of client port (optional)

    friend class KernelSystem;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::ClientPort)
CONSTRUCT_KERNEL_OBJECT(Kernel::ClientPort)
