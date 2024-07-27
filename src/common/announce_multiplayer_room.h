// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "common/web_result.h"

namespace AnnounceMultiplayerRoom {

using MacAddress = std::array<u8, 6>;

struct Room {
    struct Member {
        std::string username;
        std::string nickname;
        std::string avatar_url;
        MacAddress mac_address;
        std::string game_name;
        u64 game_id;
    };
    std::string id;
    std::string verify_UID; ///< UID used for verification
    std::string name;
    std::string description;
    std::string owner;
    std::string ip;
    u16 port;
    u32 max_player;
    u32 net_version;
    bool has_password;
    std::string preferred_game;
    u64 preferred_game_id;

    std::vector<Member> members;
};
using RoomList = std::vector<Room>;

/**
 * A AnnounceMultiplayerRoom interface class. A backend to submit/get to/from a web service should
 * implement this interface.
 */
class Backend : NonCopyable {
public:
    virtual ~Backend() = default;

    /**
     * Sets the Information that gets used for the announce
     * @param uid The Id of the room
     * @param name The name of the room
     * @param description The room description
     * @param port The port of the room
     * @param net_version The version of the libNetwork that gets used
     * @param has_password True if the room is passowrd protected
     * @param preferred_game The preferred game of the room
     * @param preferred_game_id The title id of the preferred game
     */
    virtual void SetRoomInformation(const std::string& name, const std::string& description,
                                    const u16 port, const u32 max_player, const u32 net_version,
                                    const bool has_password, const std::string& preferred_game,
                                    const u64 preferred_game_id) = 0;
    /**
     * Adds a player information to the data that gets announced
     * @param nickname The nickname of the player
     * @param mac_address The MAC Address of the player
     * @param game_id The title id of the game the player plays
     * @param game_name The name of the game the player plays
     */
    virtual void AddPlayer(const std::string& username, const std::string& nickname,
                           const std::string& avatar_url, const MacAddress& mac_address,
                           const u64 game_id, const std::string& game_name) = 0;

    /**
     * Updates the data in the announce service. Re-register the room when required.
     * @result The result of the update attempt
     */
    virtual Common::WebResult Update() = 0;

    /**
     * Registers the data in the announce service
     * @result The result of the register attempt. When the result code is Success, A global Guid of
     * the room which may be used for verification will be in the result's returned_data.
     */
    virtual Common::WebResult Register() = 0;

    /**
     * Empties the stored players
     */
    virtual void ClearPlayers() = 0;

    /**
     * Get the room information from the announce service
     * @result A list of all rooms the announce service has
     */
    virtual RoomList GetRoomList() = 0;

    /**
     * Sends a delete message to the announce service
     */
    virtual void Delete() = 0;
};

/**
 * Empty implementation of AnnounceMultiplayerRoom interface that drops all data. Used when a
 * functional backend implementation is not available.
 */
class NullBackend : public Backend {
public:
    ~NullBackend() = default;
    void SetRoomInformation(const std::string& /*name*/, const std::string& /*description*/,
                            const u16 /*port*/, const u32 /*max_player*/, const u32 /*net_version*/,
                            const bool /*has_password*/, const std::string& /*preferred_game*/,
                            const u64 /*preferred_game_id*/) override {}
    void AddPlayer(const std::string& /*username*/, const std::string& /*nickname*/,
                   const std::string& /*avatar_url*/, const MacAddress& /*mac_address*/,
                   const u64 /*game_id*/, const std::string& /*game_name*/) override {}
    Common::WebResult Update() override {
        return Common::WebResult{Common::WebResult::Code::NoWebservice, "WebService is missing"};
    }
    Common::WebResult Register() override {
        return Common::WebResult{Common::WebResult::Code::NoWebservice, "WebService is missing"};
    }
    void ClearPlayers() override {}
    RoomList GetRoomList() override {
        return RoomList{};
    }

    void Delete() override {}
};

} // namespace AnnounceMultiplayerRoom
