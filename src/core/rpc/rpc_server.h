// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <memory>
#include <span>
#include "common/polyfill_thread.h"
#include "common/threadsafe_queue.h"

namespace Core {
class System;
}

namespace Core::RPC {

class Packet;
struct PacketHeader;

class RPCServer {
public:
    explicit RPCServer(Core::System& system);
    ~RPCServer();

    void QueueRequest(std::unique_ptr<RPC::Packet> request);

private:
    void HandleReadMemory(Packet& packet, u32 address, u32 data_size);
    void HandleWriteMemory(Packet& packet, u32 address, std::span<const u8> data);
    bool ValidatePacket(const PacketHeader& packet_header);
    void HandleSingleRequest(std::unique_ptr<Packet> request);
    void HandleRequestsLoop(std::stop_token stop_token);

private:
    Core::System& system;
    Common::SPSCQueue<std::unique_ptr<Packet>, true> request_queue;
    std::jthread request_handler_thread;
};

} // namespace Core::RPC
