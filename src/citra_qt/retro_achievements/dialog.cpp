// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "dialog.h"
#include "ui_dialog.h"

#include <QListWidgetItem>

#include <rc_client.h>

#include "common/logging/log.h"

namespace RetroAchievements {

Dialog::Dialog(const std::optional<User>& user, bool enabled, bool game_loaded, QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::RetroAchievementsDialog>()), m_user(user) {
    ui->setupUi(this);

    ui->tabs->setTabVisible(ui->tabs->indexOf(ui->achievements_tab), game_loaded);
    ui->enabled_check_box->setChecked(enabled);

    connect(ui->authentication_button, &QPushButton::clicked, this,
            &Dialog::OnAuthenticationButtonPressed);
    connect(ui->enabled_check_box, &QCheckBox::toggled, this, &Dialog::OnEnabledToggled);

    UpdateUI();
}

Dialog::~Dialog() = default;

void Dialog::OnAuthenticationButtonPressed() {
    LOG_DEBUG(Frontend, "Dialog::OnAuthenticationButtonPressed");

    if (m_user) {
        m_user.reset();
        ui->password_input->clear();
        m_pending_achievement_images.clear();
        OnAchievementsUpdated({});

        emit LoggedOut();
    } else {
        QString username = ui->username_input->text();
        QString password = ui->password_input->text();

        if (username.isEmpty() || password.isEmpty()) {
            m_error_message = QStringLiteral("Fields cannot be empty.");
        } else {
            emit LogInAttempted(username, password);
            return;
        }
    }

    UpdateUI();
}

void Dialog::OnEnabledToggled(bool enabled) {
    LOG_DEBUG(Frontend, "Dialog::OnEnabledToggled");

    emit EnabledToggled(enabled);

    if (enabled) {
        emit AchievementListRefreshRequested();
    } else {
        m_pending_achievement_images.clear();
        OnAchievementsUpdated({});
    }

    UpdateUI();
}

void Dialog::OnAvatarImageDownloaded(QPixmap image) {
    LOG_DEBUG(Frontend, "Dialog::OnAvatarImageDownloaded");

    ui->user_display_avatar->setPixmap(
        image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void Dialog::OnLoginSucceeded(const User& user) {
    LOG_DEBUG(Frontend,
              "Dialog::OnLoginSucceeded(user[.display_name] = \"{}\", user[.avatar_url] = \"{}\")",
              user.display_name, user.avatar_url);

    m_user = user;
    m_error_message.clear();

    UpdateUI();
    emit AchievementListRefreshRequested();
}

void Dialog::OnLoginFailed(const QString& error_message) {
    LOG_DEBUG(Frontend, "Dialog::OnLoginFailed");

    m_error_message = error_message;

    UpdateUI();
}

void Dialog::OnAchievementImageDownloaded(const QString& url, QPixmap image) {
    m_pending_achievement_images.remove(url);
    if (image.isNull()) {
        return;
    }

    m_achievement_images.insert(url, image);
    for (int index = 0; index < ui->achievements_list->count(); ++index) {
        QListWidgetItem* item = ui->achievements_list->item(index);
        if (item->data(Qt::UserRole).toString() == url) {
            item->setIcon(QIcon{image});
        }
    }
}

void Dialog::OnAchievementsUpdated(const std::vector<Achievement>& achievements) {
    ui->achievements_list->clear();

    if (achievements.empty()) {
        auto* item = new QListWidgetItem(QStringLiteral("No achievements available."),
                                         ui->achievements_list);
        item->setFlags(Qt::NoItemFlags);
        return;
    }

    QPixmap placeholder{ui->achievements_list->iconSize()};
    placeholder.fill(Qt::transparent);

    for (const Achievement& achievement : achievements) {
        QString status;
        if (achievement.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED) {
            status = QStringLiteral("Unsupported");
        } else if ((achievement.unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) != 0) {
            status = QStringLiteral("Unlocked (Hardcore)");
        } else if ((achievement.unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE) != 0) {
            status = QStringLiteral("Unlocked");
        } else if (!achievement.measured_progress.empty()) {
            status = QString::fromStdString(achievement.measured_progress);
        } else {
            status = QStringLiteral("Locked");
        }

        if (achievement.category == RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL) {
            status = QStringLiteral("%1 - Unofficial").arg(status);
        }

        QString text = QStringLiteral("%1\n%2\n%3 points - %4")
                           .arg(QString::fromStdString(achievement.title),
                                QString::fromStdString(achievement.description))
                           .arg(achievement.points)
                           .arg(status);

        bool unlocked = achievement.unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
        QString image_url =
            QString::fromStdString(unlocked ? achievement.badge_url : achievement.badge_locked_url);

        auto* item = new QListWidgetItem(QIcon{placeholder}, text, ui->achievements_list);
        item->setData(Qt::UserRole, image_url);
        item->setToolTip(QString::fromStdString(achievement.description));

        auto cached_image = m_achievement_images.constFind(image_url);
        if (cached_image != m_achievement_images.cend()) {
            item->setIcon(QIcon{*cached_image});
        } else if (!image_url.isEmpty() && !m_pending_achievement_images.contains(image_url)) {
            m_pending_achievement_images.insert(image_url);
            emit AchievementImageRequested(image_url);
        }
    }
}

void Dialog::UpdateUI() {
    bool is_enabled = ui->enabled_check_box->isChecked();
    bool has_user = m_user.has_value();

    ui->tabs->setTabEnabled(ui->tabs->indexOf(ui->achievements_tab), is_enabled && has_user);

    ui->user_display->setVisible(is_enabled && has_user);

    ui->password_label->setVisible(!has_user);
    ui->password_input->setVisible(!has_user);

    ui->authentication_credentials->setEnabled(is_enabled && !has_user);

    if (m_user) {
        ui->username_input->setText(QString::fromStdString(m_user->username));

        ui->user_display_name->setText(QString::fromStdString(m_user->username));
        ui->user_display_points->setText(QStringLiteral("%1 points").arg(m_user->score));
        // The avatar image is set in `OnAvatarImageDownloaded`.
    }

    ui->authentication_error_label->setVisible(!m_error_message.isEmpty());
    if (!m_error_message.isEmpty()) {
        ui->authentication_error_label->setText(m_error_message);
    }

    ui->authentication_button->setEnabled(is_enabled);
    ui->authentication_button->setText(has_user ? QStringLiteral("Log Out")
                                                : QStringLiteral("Log In"));
}

} // namespace RetroAchievements
