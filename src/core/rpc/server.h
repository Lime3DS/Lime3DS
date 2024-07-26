// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/rpc/rpc_server.h"

namespace Core {
class System;
}

namespace Core::RPC {

class UDPServer;
class Packet;

class Server {
public:
    explicit Server(Core::System& system_);
    ~Server();

    void NewRequestCallback(std::unique_ptr<Packet> new_request);

private:
    RPCServer rpc_server;
    std::unique_ptr<UDPServer> udp_server;
};

} // namespace Core::RPC
