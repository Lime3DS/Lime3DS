// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <future>
#include <QColor>
#include <QImage>
#include <QList>
#include <QLocale>
#include <QMessageBox>
#include <QMetaType>
#include <QTime>
#include <QtConcurrent/QtConcurrentRun>
#include "citra_qt/game_list_p.h"
#include "citra_qt/multiplayer/host_room.h"
#include "citra_qt/multiplayer/message.h"
#include "citra_qt/multiplayer/state.h"
#include "citra_qt/multiplayer/validation.h"
#include "citra_qt/uisettings.h"
#include "common/logging/log.h"
#include "core/hle/service/cfg/cfg.h"
#include "network/announce_multiplayer_session.h"
#include "network/network_settings.h"
#include "ui_host_room.h"
#ifdef ENABLE_WEB_SERVICE
#include "web_service/verify_user_jwt.h"
#endif

HostRoomWindow::HostRoomWindow(Core::System& system_, QWidget* parent, QStandardItemModel* list,
                               std::shared_ptr<Network::AnnounceMultiplayerSession> session)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::HostRoom>()), system{system_}, announce_multiplayer_session(session) {
    ui->setupUi(this);

    // set up validation for all of the fields
    ui->room_name->setValidator(validation.GetRoomName());
    ui->username->setValidator(validation.GetNickname());
    ui->port->setValidator(validation.GetPort());
    ui->port->setPlaceholderText(QString::number(Network::DefaultRoomPort));

    // Create a proxy to the game list to display the list of preferred games
    game_list = new QStandardItemModel;
    UpdateGameList(list);

    proxy = new ComboBoxProxyModel;
    proxy->setSourceModel(game_list);
    proxy->sort(0, Qt::AscendingOrder);
    ui->game_list->setModel(proxy);

    // Connect all the widgets to the appropriate events
    connect(ui->host, &QPushButton::clicked, this, &HostRoomWindow::Host);

    // Restore the settings:
    ui->username->setText(UISettings::values.room_nickname);
    if (ui->username->text().isEmpty() && !NetSettings::values.citra_username.empty()) {
        // Use Citra Web Service user name as nickname by default
        ui->username->setText(QString::fromStdString(NetSettings::values.citra_username));
    }
    ui->room_name->setText(UISettings::values.room_name);
    ui->port->setText(UISettings::values.room_port);
    ui->max_player->setValue(UISettings::values.max_player);
    int index = UISettings::values.host_type;
    if (index < ui->host_type->count()) {
        ui->host_type->setCurrentIndex(index);
    }
    index = ui->game_list->findData(UISettings::values.game_id, GameListItemPath::ProgramIdRole);
    if (index != -1) {
        ui->game_list->setCurrentIndex(index);
    }
    ui->room_description->setText(UISettings::values.room_description);
}

HostRoomWindow::~HostRoomWindow() = default;

void HostRoomWindow::UpdateGameList(QStandardItemModel* list) {
    game_list->clear();
    for (int i = 0; i < list->rowCount(); i++) {
        auto parent = list->item(i, 0);
        for (int j = 0; j < parent->rowCount(); j++) {
            game_list->appendRow(parent->child(j)->clone());
        }
    }
}

void HostRoomWindow::RetranslateUi() {
    ui->retranslateUi(this);
}

std::unique_ptr<Network::VerifyUser::Backend> HostRoomWindow::CreateVerifyBackend(
    bool use_validation) const {
    std::unique_ptr<Network::VerifyUser::Backend> verify_backend;
    if (use_validation) {
#ifdef ENABLE_WEB_SERVICE
        verify_backend =
            std::make_unique<WebService::VerifyUserJWT>(NetSettings::values.web_api_url);
#else
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
#endif
    } else {
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
    }
    return verify_backend;
}

void HostRoomWindow::Host() {
    if (!ui->username->hasAcceptableInput()) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::USERNAME_NOT_VALID);
        return;
    }
    if (!ui->room_name->hasAcceptableInput()) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::ROOMNAME_NOT_VALID);
        return;
    }
    if (!ui->port->hasAcceptableInput()) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::PORT_NOT_VALID);
        return;
    }
    if (ui->game_list->currentIndex() == -1) {
        NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::GAME_NOT_SELECTED);
        return;
    }
    if (auto member = Network::GetRoomMember().lock()) {
        if (member->GetState() == Network::RoomMember::State::Joining) {
            return;
        } else if (member->IsConnected()) {
            auto parent = static_cast<MultiplayerState*>(parentWidget());
            if (!parent->OnCloseRoom()) {
                close();
                return;
            }
        }
        ui->host->setDisabled(true);

        auto game_name = ui->game_list->currentData(Qt::DisplayRole).toString();
        auto game_id = ui->game_list->currentData(GameListItemPath::ProgramIdRole).toLongLong();
        auto port = ui->port->isModified() ? ui->port->text().toInt() : Network::DefaultRoomPort;
        auto password = ui->password->text().toStdString();
        const bool is_public = ui->host_type->currentIndex() == 0;
        Network::Room::BanList ban_list{};
        if (ui->load_ban_list->isChecked()) {
            ban_list = UISettings::values.ban_list;
        }
        if (auto room = Network::GetRoom().lock()) {
            bool created = room->Create(ui->room_name->text().toStdString(),
                                        ui->room_description->toPlainText().toStdString(), "", port,
                                        password, ui->max_player->value(),
                                        NetSettings::values.citra_username, game_name.toStdString(),
                                        game_id, CreateVerifyBackend(is_public), ban_list);
            if (!created) {
                NetworkMessage::ErrorManager::ShowError(
                    NetworkMessage::ErrorManager::COULD_NOT_CREATE_ROOM);
                LOG_ERROR(Network, "Could not create room!");
                ui->host->setEnabled(true);
                return;
            }
        }
        // Start the announce session if they chose Public
        if (is_public) {
            if (auto session = announce_multiplayer_session.lock()) {
                // Register the room first to ensure verify_UID is present when we connect
                Common::WebResult result = session->Register();
                if (result.result_code != Common::WebResult::Code::Success) {
                    QMessageBox::warning(
                        this, tr("Error"),
                        tr("Failed to announce the room to the public lobby. In order to host a "
                           "room publicly, you must have a valid Citra account configured in "
                           "Emulation -> Configure -> Web. If you do not want to publish a room in "
                           "the public lobby, then select Unlisted instead.\nDebug Message: ") +
                            QString::fromStdString(result.result_string),
                        QMessageBox::Ok);
                    ui->host->setEnabled(true);
                    if (auto room = Network::GetRoom().lock()) {
                        room->Destroy();
                    }
                    return;
                }
                session->Start();
            } else {
                LOG_ERROR(Network, "Starting announce session failed");
            }
        }
        std::string token;
#ifdef ENABLE_WEB_SERVICE
        if (is_public) {
            WebService::Client client(NetSettings::values.web_api_url,
                                      NetSettings::values.citra_username,
                                      NetSettings::values.citra_token);
            if (auto room = Network::GetRoom().lock()) {
                token = client.GetExternalJWT(room->GetVerifyUID()).returned_data;
            }
            if (token.empty()) {
                LOG_ERROR(WebService, "Could not get external JWT, verification may fail");
            } else {
                LOG_INFO(WebService, "Successfully requested external JWT: size={}", token.size());
            }
        }
#endif
        member->Join(ui->username->text().toStdString(), Service::CFG::GetConsoleIdHash(system),
                     "127.0.0.1", port, 0, Network::NoPreferredMac, password, token);

        // Store settings
        UISettings::values.room_nickname = ui->username->text();
        UISettings::values.room_name = ui->room_name->text();
        UISettings::values.game_id =
            ui->game_list->currentData(GameListItemPath::ProgramIdRole).toLongLong();
        UISettings::values.max_player = ui->max_player->value();

        UISettings::values.host_type = ui->host_type->currentIndex();
        UISettings::values.room_port = (ui->port->isModified() && !ui->port->text().isEmpty())
                                           ? ui->port->text()
                                           : QString::number(Network::DefaultRoomPort);
        UISettings::values.room_description = ui->room_description->toPlainText();
        ui->host->setEnabled(true);
        close();
    }
}

QVariant ComboBoxProxyModel::data(const QModelIndex& idx, int role) const {
    if (role != Qt::DisplayRole) {
        auto val = QSortFilterProxyModel::data(idx, role);
        // If its the icon, shrink it to 16x16
        if (role == Qt::DecorationRole)
            val = val.value<QImage>().scaled(16, 16, Qt::KeepAspectRatio);
        return val;
    }
    std::string filename;
    Common::SplitPath(
        QSortFilterProxyModel::data(idx, GameListItemPath::FullPathRole).toString().toStdString(),
        nullptr, &filename, nullptr);
    QString title = QSortFilterProxyModel::data(idx, GameListItemPath::TitleRole).toString();
    return title.isEmpty() ? QString::fromStdString(filename) : title;
}

bool ComboBoxProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    auto leftData = left.data(GameListItemPath::TitleRole).toString();
    auto rightData = right.data(GameListItemPath::TitleRole).toString();
    return leftData.compare(rightData) < 0;
}
