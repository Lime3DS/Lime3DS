// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>

namespace Core::RPC {

class Packet;

class UDPServer {
public:
    explicit UDPServer(std::function<void(std::unique_ptr<Packet>)> new_request_callback);
    ~UDPServer();

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Core::RPC
