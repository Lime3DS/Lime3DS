// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include "common/announce_multiplayer_room.h"
#include "web_service/web_backend.h"

namespace WebService {

/**
 * Implementation of AnnounceMultiplayerRoom::Backend that (de)serializes room information into/from
 * JSON, and submits/gets it to/from the Lime3DS web service
 */
class RoomJson : public AnnounceMultiplayerRoom::Backend {
public:
    RoomJson(const std::string& host, const std::string& username, const std::string& token)
        : client(host, username, token), host(host), username(username), token(token) {}
    ~RoomJson() = default;
    void SetRoomInformation(const std::string& name, const std::string& description, const u16 port,
                            const u32 max_player, const u32 net_version, const bool has_password,
                            const std::string& preferred_game,
                            const u64 preferred_game_id) override;
    void AddPlayer(const std::string& username, const std::string& nickname,
                   const std::string& avatar_url,
                   const AnnounceMultiplayerRoom::MacAddress& mac_address, const u64 game_id,
                   const std::string& game_name) override;
    Common::WebResult Update() override;
    Common::WebResult Register() override;
    void ClearPlayers() override;
    AnnounceMultiplayerRoom::RoomList GetRoomList() override;
    void Delete() override;

private:
    AnnounceMultiplayerRoom::Room room;
    Client client;
    std::string host;
    std::string username;
    std::string token;
    std::string room_id;
};

} // namespace WebService
