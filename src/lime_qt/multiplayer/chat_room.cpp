// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <future>
#include <QColor>
#include <QDesktopServices>
#include <QFutureWatcher>
#include <QImage>
#include <QList>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMetaType>
#include <QTime>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>
#include "common/logging/log.h"
#include "lime_qt/game_list_p.h"
#include "lime_qt/multiplayer/chat_room.h"
#include "lime_qt/multiplayer/message.h"
#include "network/announce_multiplayer_session.h"
#include "ui_chat_room.h"
#ifdef ENABLE_WEB_SERVICE
#include "web_service/web_backend.h"
#endif

class ChatMessage {
public:
    explicit ChatMessage(const Network::ChatEntry& chat, QTime ts = {}) {
        /// Convert the time to their default locale defined format
        QLocale locale;
        timestamp = locale.toString(ts.isValid() ? ts : QTime::currentTime(), QLocale::ShortFormat);
        nickname = QString::fromStdString(chat.nickname);
        username = QString::fromStdString(chat.username);
        message = QString::fromStdString(chat.message);

        // Check for user pings
        QString cur_nickname, cur_username;
        if (auto room = Network::GetRoomMember().lock()) {
            cur_nickname = QString::fromStdString(room->GetNickname());
            cur_username = QString::fromStdString(room->GetUsername());
        }

        // Handle pings at the beginning and end of message
        QString fixed_message = QStringLiteral(" %1 ").arg(message);
        if (fixed_message.contains(QStringLiteral(" @%1 ").arg(cur_nickname)) ||
            (!cur_username.isEmpty() &&
             fixed_message.contains(QStringLiteral(" @%1 ").arg(cur_username)))) {

            contains_ping = true;
        } else {
            contains_ping = false;
        }
    }

    bool ContainsPing() const {
        return contains_ping;
    }

    /// Format the message using the players color
    QString GetPlayerChatMessage(u16 player) const {
        auto color = player_color[player % 16];
        QString name;
        if (username.isEmpty() || username == nickname) {
            name = nickname;
        } else {
            name = QStringLiteral("%1 (%2)").arg(nickname, username);
        }

        QString style, text_color;
        if (ContainsPing()) {
            // Add a background color to these messages
            style = QStringLiteral("background-color: %1").arg(QString::fromStdString(ping_color));
            // Add a font color
            text_color = QStringLiteral("color='#000000'");
        }

        return QStringLiteral("[%1] <font color='%2'>&lt;%3&gt;</font> <font style='%4' "
                              "%5>%6</font>")
            .arg(timestamp, QString::fromStdString(color), name.toHtmlEscaped(), style, text_color,
                 message.toHtmlEscaped());
    }

private:
    static constexpr std::array<const char*, 16> player_color = {
        {"#0000FF", "#FF0000", "#8A2BE2", "#FF69B4", "#1E90FF", "#008000", "#00FF7F", "#B22222",
         "#DAA520", "#FF4500", "#2E8B57", "#5F9EA0", "#D2691E", "#9ACD32", "#FF7F50", "FFFF00"}};
    static constexpr char ping_color[] = "#FFFF00";

    QString timestamp;
    QString nickname;
    QString username;
    QString message;
    bool contains_ping;
};

class StatusMessage {
public:
    explicit StatusMessage(const QString& msg, QTime ts = {}) {
        /// Convert the time to their default locale defined format
        QLocale locale;
        timestamp = locale.toString(ts.isValid() ? ts : QTime::currentTime(), QLocale::ShortFormat);
        message = msg;
    }

    QString GetSystemChatMessage() const {
        return QStringLiteral("[%1] <font color='%2'>* %3</font>")
            .arg(timestamp, QString::fromStdString(system_color), message);
    }

private:
    static constexpr const char system_color[] = "#FF8C00";
    QString timestamp;
    QString message;
};

class PlayerListItem : public QStandardItem {
public:
    static const int NicknameRole = Qt::UserRole + 1;
    static const int UsernameRole = Qt::UserRole + 2;
    static const int AvatarUrlRole = Qt::UserRole + 3;
    static const int GameNameRole = Qt::UserRole + 4;

    PlayerListItem() = default;
    explicit PlayerListItem(const std::string& nickname, const std::string& username,
                            const std::string& avatar_url, const std::string& game_name) {
        setEditable(false);
        setData(QString::fromStdString(nickname), NicknameRole);
        setData(QString::fromStdString(username), UsernameRole);
        setData(QString::fromStdString(avatar_url), AvatarUrlRole);
        if (game_name.empty()) {
            setData(QObject::tr("Not playing a game"), GameNameRole);
        } else {
            setData(QString::fromStdString(game_name), GameNameRole);
        }
    }

    QVariant data(int role) const override {
        if (role != Qt::DisplayRole) {
            return QStandardItem::data(role);
        }
        QString name;
        const QString nickname = data(NicknameRole).toString();
        const QString username = data(UsernameRole).toString();
        if (username.isEmpty() || username == nickname) {
            name = nickname;
        } else {
            name = QStringLiteral("%1 (%2)").arg(nickname, username);
        }
        return QStringLiteral("%1\n      %2").arg(name, data(GameNameRole).toString());
    }
};

ChatRoom::ChatRoom(QWidget* parent) : QWidget(parent), ui(std::make_unique<Ui::ChatRoom>()) {
    ui->setupUi(this);

    // set the item_model for player_view

    player_list = new QStandardItemModel(ui->player_view);
    ui->player_view->setModel(player_list);
    ui->player_view->setContextMenuPolicy(Qt::CustomContextMenu);
    // set a header to make it look better though there is only one column
    player_list->insertColumns(0, 1);
    player_list->setHeaderData(0, Qt::Horizontal, tr("Members"));

    ui->chat_history->document()->setMaximumBlockCount(max_chat_lines);

    // register the network structs to use in slots and signals
    qRegisterMetaType<Network::ChatEntry>();
    qRegisterMetaType<Network::StatusMessageEntry>();
    qRegisterMetaType<Network::RoomInformation>();
    qRegisterMetaType<Network::RoomMember::State>();

    // setup the callbacks for network updates
    if (auto member = Network::GetRoomMember().lock()) {
        member->BindOnChatMessageRecieved(
            [this](const Network::ChatEntry& chat) { emit ChatReceived(chat); });
        member->BindOnStatusMessageReceived(
            [this](const Network::StatusMessageEntry& status_message) {
                emit StatusMessageReceived(status_message);
            });
        connect(this, &ChatRoom::ChatReceived, this, &ChatRoom::OnChatReceive);
        connect(this, &ChatRoom::StatusMessageReceived, this, &ChatRoom::OnStatusMessageReceive);
    } else {
        // TODO (jroweboy) network was not initialized?
    }

    // Connect all the widgets to the appropriate events
    connect(ui->player_view, &QTreeView::customContextMenuRequested, this,
            &ChatRoom::PopupContextMenu);
    connect(ui->chat_message, &QLineEdit::returnPressed, this, &ChatRoom::OnSendChat);
    connect(ui->chat_message, &QLineEdit::textChanged, this, &ChatRoom::OnChatTextChanged);
    connect(ui->send_message, &QPushButton::clicked, this, &ChatRoom::OnSendChat);
}

ChatRoom::~ChatRoom() = default;

void ChatRoom::SetModPerms(bool is_mod) {
    has_mod_perms = is_mod;
}

void ChatRoom::RetranslateUi() {
    ui->retranslateUi(this);
}

void ChatRoom::Clear() {
    ui->chat_history->clear();
    block_list.clear();
}

void ChatRoom::AppendStatusMessage(const QString& msg) {
    ui->chat_history->append(StatusMessage(msg).GetSystemChatMessage());
}

void ChatRoom::AppendChatMessage(const QString& msg) {
    ui->chat_history->append(msg);
}

void ChatRoom::SendModerationRequest(Network::RoomMessageTypes type, const std::string& nickname) {
    if (auto room = Network::GetRoomMember().lock()) {
        auto members = room->GetMemberInformation();
        auto it = std::find_if(members.begin(), members.end(),
                               [&nickname](const Network::RoomMember::MemberInformation& member) {
                                   return member.nickname == nickname;
                               });
        if (it == members.end()) {
            NetworkMessage::ErrorManager::ShowError(NetworkMessage::ErrorManager::NO_SUCH_USER);
            return;
        }
        room->SendModerationRequest(type, nickname);
    }
}

bool ChatRoom::ValidateMessage(const std::string& msg) {
    return !msg.empty();
}

void ChatRoom::OnRoomUpdate(const Network::RoomInformation& info) {
    // TODO(B3N30): change title
    if (auto room_member = Network::GetRoomMember().lock()) {
        SetPlayerList(room_member->GetMemberInformation());
    }
}

void ChatRoom::Disable() {
    ui->send_message->setDisabled(true);
    ui->chat_message->setDisabled(true);
}

void ChatRoom::Enable() {
    ui->send_message->setEnabled(true);
    ui->chat_message->setEnabled(true);
}

void ChatRoom::OnChatReceive(const Network::ChatEntry& chat) {
    if (!ValidateMessage(chat.message)) {
        return;
    }
    if (auto room = Network::GetRoomMember().lock()) {
        // get the id of the player
        auto members = room->GetMemberInformation();
        auto it = std::find_if(members.begin(), members.end(),
                               [&chat](const Network::RoomMember::MemberInformation& member) {
                                   return member.nickname == chat.nickname &&
                                          member.username == chat.username;
                               });
        if (it == members.end()) {
            LOG_INFO(Network, "Chat message received from unknown player. Ignoring it.");
            return;
        }
        if (block_list.count(chat.nickname)) {
            LOG_INFO(Network, "Chat message received from blocked player {}. Ignoring it.",
                     chat.nickname);
            return;
        }
        auto player = std::distance(members.begin(), it);
        ChatMessage m(chat);
        if (m.ContainsPing()) {
            emit UserPinged();
        }
        AppendChatMessage(m.GetPlayerChatMessage(player));
    }
}

void ChatRoom::OnStatusMessageReceive(const Network::StatusMessageEntry& status_message) {
    QString name;
    if (status_message.username.empty() || status_message.username == status_message.nickname) {
        name = QString::fromStdString(status_message.nickname);
    } else {
        name = QStringLiteral("%1 (%2)").arg(QString::fromStdString(status_message.nickname),
                                             QString::fromStdString(status_message.username));
    }
    QString message;
    switch (status_message.type) {
    case Network::IdMemberJoin:
        message = tr("%1 has joined").arg(name);
        break;
    case Network::IdMemberLeave:
        message = tr("%1 has left").arg(name);
        break;
    case Network::IdMemberKicked:
        message = tr("%1 has been kicked").arg(name);
        break;
    case Network::IdMemberBanned:
        message = tr("%1 has been banned").arg(name);
        break;
    case Network::IdAddressUnbanned:
        message = tr("%1 has been unbanned").arg(name);
        break;
    }
    if (!message.isEmpty())
        AppendStatusMessage(message);
}

void ChatRoom::OnSendChat() {
    if (auto room = Network::GetRoomMember().lock()) {
        if (room->GetState() != Network::RoomMember::State::Joined &&
            room->GetState() != Network::RoomMember::State::Moderator) {

            return;
        }
        auto message = ui->chat_message->text().toStdString();
        if (!ValidateMessage(message)) {
            return;
        }
        auto nick = room->GetNickname();
        auto username = room->GetUsername();
        Network::ChatEntry chat{nick, username, message};

        auto members = room->GetMemberInformation();
        auto it = std::find_if(members.begin(), members.end(),
                               [&chat](const Network::RoomMember::MemberInformation& member) {
                                   return member.nickname == chat.nickname &&
                                          member.username == chat.username;
                               });
        if (it == members.end()) {
            LOG_INFO(Network, "Cannot find self in the player list when sending a message.");
        }
        auto player = std::distance(members.begin(), it);
        ChatMessage m(chat);
        room->SendChatMessage(message);
        AppendChatMessage(m.GetPlayerChatMessage(player));
        ui->chat_message->clear();
    }
}

void ChatRoom::UpdateIconDisplay() {
    for (int row = 0; row < player_list->invisibleRootItem()->rowCount(); ++row) {
        QStandardItem* item = player_list->invisibleRootItem()->child(row);
        const std::string avatar_url =
            item->data(PlayerListItem::AvatarUrlRole).toString().toStdString();
        if (icon_cache.count(avatar_url)) {
            item->setData(icon_cache.at(avatar_url), Qt::DecorationRole);
        } else {
            item->setData(QIcon::fromTheme(QStringLiteral("no_avatar")).pixmap(48),
                          Qt::DecorationRole);
        }
    }
}

void ChatRoom::SetPlayerList(const Network::RoomMember::MemberList& member_list) {
    // TODO(B3N30): Remember which row is selected
    player_list->removeRows(0, player_list->rowCount());
    for (const auto& member : member_list) {
        if (member.nickname.empty())
            continue;
        QStandardItem* name_item = new PlayerListItem(member.nickname, member.username,
                                                      member.avatar_url, member.game_info.name);

#ifdef ENABLE_WEB_SERVICE
        if (!icon_cache.count(member.avatar_url) && !member.avatar_url.empty()) {
            // Start a request to get the member's avatar
            const QUrl url(QString::fromStdString(member.avatar_url));
            QFuture<std::string> future = QtConcurrent::run([url] {
                WebService::Client client(
                    QStringLiteral("%1://%2").arg(url.scheme(), url.host()).toStdString(), "", "");
                auto result = client.GetImage(url.path().toStdString(), true);
                if (result.returned_data.empty()) {
                    LOG_ERROR(WebService, "Failed to get avatar");
                }
                return result.returned_data;
            });
            auto* future_watcher = new QFutureWatcher<std::string>(this);
            connect(future_watcher, &QFutureWatcher<std::string>::finished, this,
                    [this, future_watcher, avatar_url = member.avatar_url] {
                        const std::string result = future_watcher->result();
                        if (result.empty())
                            return;
                        QPixmap pixmap;
                        if (!pixmap.loadFromData(reinterpret_cast<const u8*>(result.data()),
                                                 static_cast<u32>(result.size())))
                            return;
                        icon_cache[avatar_url] =
                            pixmap.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                        // Update all the displayed icons with the new icon_cache
                        UpdateIconDisplay();
                    });
            future_watcher->setFuture(future);
        }
#endif

        player_list->invisibleRootItem()->appendRow(name_item);
    }
    UpdateIconDisplay();
    // TODO(B3N30): Restore row selection
}

void ChatRoom::OnChatTextChanged() {
    if (ui->chat_message->text().length() > static_cast<int>(Network::MaxMessageSize))
        ui->chat_message->setText(
            ui->chat_message->text().left(static_cast<int>(Network::MaxMessageSize)));
}

void ChatRoom::PopupContextMenu(const QPoint& menu_location) {
    QModelIndex item = ui->player_view->indexAt(menu_location);
    if (!item.isValid())
        return;

    std::string nickname =
        player_list->item(item.row())->data(PlayerListItem::NicknameRole).toString().toStdString();

    QMenu context_menu;

    QString username = player_list->item(item.row())->data(PlayerListItem::UsernameRole).toString();
    if (!username.isEmpty()) {
        QAction* view_profile_action = context_menu.addAction(tr("View Profile"));
        connect(view_profile_action, &QAction::triggered, [username] {
            QDesktopServices::openUrl(
                QUrl(QStringLiteral("https://community.citra-emu.org/u/%1").arg(username)));
        });
    }

    std::string cur_nickname;
    if (auto room = Network::GetRoomMember().lock()) {
        cur_nickname = room->GetNickname();
    }

    if (nickname != cur_nickname) { // You can't block yourself
        QAction* block_action = context_menu.addAction(tr("Block Player"));

        block_action->setCheckable(true);
        block_action->setChecked(block_list.count(nickname) > 0);

        connect(block_action, &QAction::triggered, [this, nickname] {
            if (block_list.count(nickname)) {
                block_list.erase(nickname);
            } else {
                QMessageBox::StandardButton result = QMessageBox::question(
                    this, tr("Block Player"),
                    tr("When you block a player, you will no longer receive chat messages from "
                       "them.<br><br>Are you sure you would like to block %1?")
                        .arg(QString::fromStdString(nickname)),
                    QMessageBox::Yes | QMessageBox::No);
                if (result == QMessageBox::Yes)
                    block_list.emplace(nickname);
            }
        });
    }

    if (has_mod_perms && nickname != cur_nickname) { // You can't kick or ban yourself
        context_menu.addSeparator();

        QAction* kick_action = context_menu.addAction(tr("Kick"));
        QAction* ban_action = context_menu.addAction(tr("Ban"));

        connect(kick_action, &QAction::triggered, [this, nickname] {
            QMessageBox::StandardButton result =
                QMessageBox::question(this, tr("Kick Player"),
                                      tr("Are you sure you would like to <b>kick</b> %1?")
                                          .arg(QString::fromStdString(nickname)),
                                      QMessageBox::Yes | QMessageBox::No);
            if (result == QMessageBox::Yes)
                SendModerationRequest(Network::IdModKick, nickname);
        });
        connect(ban_action, &QAction::triggered, [this, nickname] {
            QMessageBox::StandardButton result = QMessageBox::question(
                this, tr("Ban Player"),
                tr("Are you sure you would like to <b>kick and ban</b> %1?\n\nThis would "
                   "ban both their forum username and their IP address.")
                    .arg(QString::fromStdString(nickname)),
                QMessageBox::Yes | QMessageBox::No);
            if (result == QMessageBox::Yes)
                SendModerationRequest(Network::IdModBan, nickname);
        });
    }

    context_menu.exec(ui->player_view->viewport()->mapToGlobal(menu_location));
}
