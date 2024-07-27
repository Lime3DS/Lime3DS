// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QVariant>
#include "lime_qt/multiplayer/validation.h"

namespace Ui {
class HostRoom;
}

namespace Core {
class System;
}

namespace Network {
class AnnounceMultiplayerSession;
}

class ConnectionError;
class ComboBoxProxyModel;

class ChatMessage;

namespace Network::VerifyUser {
class Backend;
};

class HostRoomWindow : public QDialog {
    Q_OBJECT

public:
    explicit HostRoomWindow(Core::System& system, QWidget* parent, QStandardItemModel* list,
                            std::shared_ptr<Network::AnnounceMultiplayerSession> session);
    ~HostRoomWindow();

    /**
     * Updates the dialog with a new game list model.
     * This model should be the original model of the game list.
     */
    void UpdateGameList(QStandardItemModel* list);
    void RetranslateUi();

private:
    void Host();
    std::unique_ptr<Network::VerifyUser::Backend> CreateVerifyBackend(bool use_validation) const;

    std::unique_ptr<Ui::HostRoom> ui;
    Core::System& system;
    std::weak_ptr<Network::AnnounceMultiplayerSession> announce_multiplayer_session;
    QStandardItemModel* game_list;
    ComboBoxProxyModel* proxy;
    Validation validation;
};

/**
 * Proxy Model for the game list combo box so we can reuse the game list model while still
 * displaying the fields slightly differently
 */
class ComboBoxProxyModel : public QSortFilterProxyModel {
    Q_OBJECT

public:
    int columnCount(const QModelIndex& idx) const override {
        return 1;
    }

    QVariant data(const QModelIndex& idx, int role) const override;

    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
