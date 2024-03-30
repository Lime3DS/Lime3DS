// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>

namespace NetworkMessage {

class ConnectionError {

public:
    explicit ConnectionError(std::string str) : err(std::move(str)) {}
    const std::string& GetString() const {
        return err;
    }

private:
    std::string err;
};

class ErrorManager : QObject {
    Q_OBJECT
public:
    /// When the nickname is considered invalid by the client
    static const ConnectionError USERNAME_NOT_VALID;
    static const ConnectionError ROOMNAME_NOT_VALID;
    /// When the nickname is considered invalid by the room server
    static const ConnectionError USERNAME_NOT_VALID_SERVER;
    static const ConnectionError IP_ADDRESS_NOT_VALID;
    static const ConnectionError PORT_NOT_VALID;
    static const ConnectionError GAME_NOT_SELECTED;
    static const ConnectionError NO_INTERNET;
    static const ConnectionError UNABLE_TO_CONNECT;
    static const ConnectionError ROOM_IS_FULL;
    static const ConnectionError COULD_NOT_CREATE_ROOM;
    static const ConnectionError HOST_BANNED;
    static const ConnectionError WRONG_VERSION;
    static const ConnectionError WRONG_PASSWORD;
    static const ConnectionError GENERIC_ERROR;
    static const ConnectionError LOST_CONNECTION;
    static const ConnectionError HOST_KICKED;
    static const ConnectionError MAC_COLLISION;
    static const ConnectionError CONSOLE_ID_COLLISION;
    static const ConnectionError PERMISSION_DENIED;
    static const ConnectionError NO_SUCH_USER;
    /**
     *  Shows a standard QMessageBox with a error message
     */
    static void ShowError(const ConnectionError& e);
};
/**
 * Show a standard QMessageBox with a warning message about leaving the room
 * return true if the user wants to close the network connection
 */
bool WarnCloseRoom();

/**
 * Show a standard QMessageBox with a warning message about disconnecting from the room
 * return true if the user wants to disconnect
 */
bool WarnDisconnect();

} // namespace NetworkMessage
