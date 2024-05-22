// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "socket_manager.h"

namespace Network {
std::atomic<u32> SocketManager::count = 0;

void SocketManager::EnableSockets() {
    if (count++ == 0) {
#ifdef _WIN32
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
#endif
    }
}

void SocketManager::DisableSockets() {
    if (--count == 0) {
#ifdef _WIN32
        WSACleanup();
#endif
    }
}
} // namespace Network