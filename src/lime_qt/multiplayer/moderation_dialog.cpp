// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QStandardItem>
#include <QStandardItemModel>
#include "lime_qt/multiplayer/moderation_dialog.h"
#include "network/network.h"
#include "network/room_member.h"
#include "ui_moderation_dialog.h"

namespace Column {
enum {
    SUBJECT,
    TYPE,
    COUNT,
};
}

ModerationDialog::ModerationDialog(QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::ModerationDialog>()) {
    ui->setupUi(this);

    qRegisterMetaType<Network::Room::BanList>();

    if (auto member = Network::GetRoomMember().lock()) {
        callback_handle_status_message = member->BindOnStatusMessageReceived(
            [this](const Network::StatusMessageEntry& status_message) {
                emit StatusMessageReceived(status_message);
            });
        connect(this, &ModerationDialog::StatusMessageReceived, this,
                &ModerationDialog::OnStatusMessageReceived);
        callback_handle_ban_list = member->BindOnBanListReceived(
            [this](const Network::Room::BanList& ban_list) { emit BanListReceived(ban_list); });
        connect(this, &ModerationDialog::BanListReceived, this, &ModerationDialog::PopulateBanList);
    }

    // Initialize the UI
    model = new QStandardItemModel(ui->ban_list_view);
    model->insertColumns(0, Column::COUNT);
    model->setHeaderData(Column::SUBJECT, Qt::Horizontal, tr("Subject"));
    model->setHeaderData(Column::TYPE, Qt::Horizontal, tr("Type"));

    ui->ban_list_view->setModel(model);

    // Load the ban list in background
    LoadBanList();

    connect(ui->refresh, &QPushButton::clicked, this, [this] { LoadBanList(); });
    connect(ui->unban, &QPushButton::clicked, this, [this] {
        auto index = ui->ban_list_view->currentIndex();
        SendUnbanRequest(model->item(index.row(), 0)->text());
    });
    connect(ui->ban_list_view, &QTreeView::clicked, [this] { ui->unban->setEnabled(true); });
}

ModerationDialog::~ModerationDialog() {
    if (callback_handle_status_message) {
        if (auto room = Network::GetRoomMember().lock()) {
            room->Unbind(callback_handle_status_message);
        }
    }

    if (callback_handle_ban_list) {
        if (auto room = Network::GetRoomMember().lock()) {
            room->Unbind(callback_handle_ban_list);
        }
    }
}

void ModerationDialog::LoadBanList() {
    if (auto room = Network::GetRoomMember().lock()) {
        ui->refresh->setEnabled(false);
        ui->refresh->setText(tr("Refreshing"));
        ui->unban->setEnabled(false);
        room->RequestBanList();
    }
}

void ModerationDialog::PopulateBanList(const Network::Room::BanList& ban_list) {
    model->removeRows(0, model->rowCount());
    for (const auto& username : ban_list.first) {
        QStandardItem* subject_item = new QStandardItem(QString::fromStdString(username));
        QStandardItem* type_item = new QStandardItem(tr("Forum Username"));
        model->invisibleRootItem()->appendRow({subject_item, type_item});
    }
    for (const auto& ip : ban_list.second) {
        QStandardItem* subject_item = new QStandardItem(QString::fromStdString(ip));
        QStandardItem* type_item = new QStandardItem(tr("IP Address"));
        model->invisibleRootItem()->appendRow({subject_item, type_item});
    }
    for (int i = 0; i < Column::COUNT - 1; ++i) {
        ui->ban_list_view->resizeColumnToContents(i);
    }
    ui->refresh->setEnabled(true);
    ui->refresh->setText(tr("Refresh"));
    ui->unban->setEnabled(false);
}

void ModerationDialog::SendUnbanRequest(const QString& subject) {
    if (auto room = Network::GetRoomMember().lock()) {
        room->SendModerationRequest(Network::IdModUnban, subject.toStdString());
    }
}

void ModerationDialog::OnStatusMessageReceived(const Network::StatusMessageEntry& status_message) {
    if (status_message.type != Network::IdMemberBanned &&
        status_message.type != Network::IdAddressUnbanned)
        return;

    // Update the ban list for ban/unban
    LoadBanList();
}
