// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "lime_qt/multiplayer/chat_room.h"

namespace Ui {
class ClientRoom;
}

class ClientRoomWindow : public QDialog {
    Q_OBJECT

public:
    explicit ClientRoomWindow(QWidget* parent);
    ~ClientRoomWindow();

    void RetranslateUi();
    void UpdateIconDisplay();

public slots:
    void OnRoomUpdate(const Network::RoomInformation&);
    void OnStateChange(const Network::RoomMember::State&);

signals:
    void RoomInformationChanged(const Network::RoomInformation&);
    void StateChanged(const Network::RoomMember::State&);
    void ShowNotification();

private:
    void Disconnect();
    void UpdateView();
    void SetModPerms(bool is_mod);

    QStandardItemModel* player_list;
    std::unique_ptr<Ui::ClientRoom> ui;
};
