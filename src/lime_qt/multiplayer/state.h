// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QWidget>
#include "network/announce_multiplayer_session.h"
#include "network/network.h"

class QStandardItemModel;
class Lobby;
class HostRoomWindow;
class ClientRoomWindow;
class DirectConnectWindow;
class ClickableLabel;

namespace Core {
class System;
}

class MultiplayerState : public QWidget {
    Q_OBJECT;

public:
    explicit MultiplayerState(Core::System& system, QWidget* parent, QStandardItemModel* game_list,
                              QAction* leave_room, QAction* show_room);
    ~MultiplayerState();

    /**
     * Close all open multiplayer related dialogs
     */
    void Close();

    ClickableLabel* GetStatusText() const {
        return status_text;
    }

    ClickableLabel* GetStatusIcon() const {
        return status_icon;
    }

    void retranslateUi();

    /**
     * Whether a public room is being hosted or not.
     * When this is true, Web Services configuration should be disabled.
     */
    bool IsHostingPublicRoom() const;

    void UpdateCredentials();

    /**
     * Updates the multiplayer dialogs with a new game list model.
     * This model should be the original model of the game list.
     */
    void UpdateGameList(QStandardItemModel* game_list);

public slots:
    void OnNetworkStateChanged(const Network::RoomMember::State& state);
    void OnNetworkError(const Network::RoomMember::Error& error);
    void OnViewLobby();
    void OnCreateRoom();
    bool OnCloseRoom();
    void OnOpenNetworkRoom();
    void OnDirectConnectToRoom();
    void OnAnnounceFailed(const Common::WebResult&);
    void UpdateThemedIcons();
    void ShowNotification();
    void HideNotification();

signals:
    void NetworkStateChanged(const Network::RoomMember::State&);
    void NetworkError(const Network::RoomMember::Error&);
    void AnnounceFailed(const Common::WebResult&);

private:
    Core::System& system;
    Lobby* lobby = nullptr;
    HostRoomWindow* host_room = nullptr;
    ClientRoomWindow* client_room = nullptr;
    DirectConnectWindow* direct_connect = nullptr;
    ClickableLabel* status_icon = nullptr;
    ClickableLabel* status_text = nullptr;
    QStandardItemModel* game_list_model = nullptr;
    QAction* leave_room;
    QAction* show_room;
    std::shared_ptr<Network::AnnounceMultiplayerSession> announce_multiplayer_session;
    Network::RoomMember::State current_state = Network::RoomMember::State::Uninitialized;
    bool has_mod_perms = false;
    Network::RoomMember::CallbackHandle<Network::RoomMember::State> state_callback_handle;
    Network::RoomMember::CallbackHandle<Network::RoomMember::Error> error_callback_handle;

    bool show_notification = false;
};

Q_DECLARE_METATYPE(Common::WebResult);
