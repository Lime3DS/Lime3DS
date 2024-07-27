// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "network/room.h"
#include "network/room_member.h"

namespace Network {

/// Initializes and registers the network device, the room, and the room member.
bool Init();

/// Returns a pointer to the room handle
std::weak_ptr<Room> GetRoom();

/// Returns a pointer to the room member handle
std::weak_ptr<RoomMember> GetRoomMember();

/// Unregisters the network device, the room, and the room member and shut them down.
void Shutdown();

} // namespace Network
