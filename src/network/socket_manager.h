// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include "atomic"
#include "common/common_types.h"

namespace Network {
class SocketManager {
public:
    static void EnableSockets();
    static void DisableSockets();

private:
    SocketManager();
    static std::atomic<u32> count;
};
} // namespace Network