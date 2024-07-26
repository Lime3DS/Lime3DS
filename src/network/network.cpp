// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "enet/enet.h"
#include "network/network.h"

namespace Network {

static std::shared_ptr<RoomMember> g_room_member; ///< RoomMember (Client) for network games
static std::shared_ptr<Room> g_room;              ///< Room (Server) for network games
// TODO(B3N30): Put these globals into a networking class

bool Init() {
    if (enet_initialize() != 0) {
        LOG_ERROR(Network, "Error initalizing ENet");
        return false;
    }
    g_room = std::make_shared<Room>();
    g_room_member = std::make_shared<RoomMember>();
    LOG_DEBUG(Network, "initialized OK");
    return true;
}

std::weak_ptr<Room> GetRoom() {
    return g_room;
}

std::weak_ptr<RoomMember> GetRoomMember() {
    return g_room_member;
}

void Shutdown() {
    if (g_room_member) {
        if (g_room_member->IsConnected())
            g_room_member->Leave();
        g_room_member.reset();
    }
    if (g_room) {
        if (g_room->GetState() == Room::State::Open)
            g_room->Destroy();
        g_room.reset();
    }
    enet_deinitialize();
    LOG_DEBUG(Network, "shutdown OK");
}

} // namespace Network
