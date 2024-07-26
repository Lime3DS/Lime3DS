// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/rpc/packet.h"
#include "core/rpc/rpc_server.h"
#include "core/rpc/server.h"
#include "core/rpc/udp_server.h"

namespace Core::RPC {

Server::Server(Core::System& system_) : rpc_server{system_} {
    const auto callback = [this](std::unique_ptr<Packet> new_request) {
        NewRequestCallback(std::move(new_request));
    };

    try {
        udp_server = std::make_unique<UDPServer>(callback);
    } catch (...) {
        LOG_ERROR(RPC_Server, "Error starting UDP server");
    }
}

Server::~Server() {
    udp_server.reset();
    NewRequestCallback(nullptr); // Notify the RPC server to end
}

void Server::NewRequestCallback(std::unique_ptr<RPC::Packet> new_request) {
    if (new_request) {
        LOG_INFO(RPC_Server, "Received request version={} id={} type={} size={}",
                 new_request->GetVersion(), new_request->GetId(), new_request->GetPacketType(),
                 new_request->GetPacketDataSize());
    } else {
        LOG_INFO(RPC_Server, "Received end packet");
    }
    rpc_server.QueueRequest(std::move(new_request));
}

}; // namespace Core::RPC
