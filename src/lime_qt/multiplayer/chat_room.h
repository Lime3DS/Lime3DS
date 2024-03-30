// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVariant>
#include "network/network.h"

namespace Ui {
class ChatRoom;
}

namespace Network {
class AnnounceMultiplayerSession;
}

class ConnectionError;
class ComboBoxProxyModel;

class ChatMessage;

class ChatRoom : public QWidget {
    Q_OBJECT

public:
    explicit ChatRoom(QWidget* parent);
    void RetranslateUi();
    void SetPlayerList(const Network::RoomMember::MemberList& member_list);
    void Clear();
    void AppendStatusMessage(const QString& msg);
    ~ChatRoom();

    void SetModPerms(bool is_mod);
    void UpdateIconDisplay();

public slots:
    void OnRoomUpdate(const Network::RoomInformation& info);
    void OnChatReceive(const Network::ChatEntry&);
    void OnStatusMessageReceive(const Network::StatusMessageEntry&);
    void OnSendChat();
    void OnChatTextChanged();
    void PopupContextMenu(const QPoint& menu_location);
    void Disable();
    void Enable();

signals:
    void ChatReceived(const Network::ChatEntry&);
    void StatusMessageReceived(const Network::StatusMessageEntry&);
    void UserPinged();

private:
    static constexpr u32 max_chat_lines = 1000;
    void AppendChatMessage(const QString&);
    bool ValidateMessage(const std::string&);
    void SendModerationRequest(Network::RoomMessageTypes type, const std::string& nickname);

    bool has_mod_perms = false;
    QStandardItemModel* player_list;
    std::unique_ptr<Ui::ChatRoom> ui;
    std::unordered_set<std::string> block_list;
    std::unordered_map<std::string, QPixmap> icon_cache;
};

Q_DECLARE_METATYPE(Network::ChatEntry);
Q_DECLARE_METATYPE(Network::StatusMessageEntry);
Q_DECLARE_METATYPE(Network::RoomInformation);
Q_DECLARE_METATYPE(Network::RoomMember::State);
Q_DECLARE_METATYPE(Network::RoomMember::Error);
