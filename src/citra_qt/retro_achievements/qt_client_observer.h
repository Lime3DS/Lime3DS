// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string_view>

#include <QMetaType>
#include <QObject>
#include <QString>

#include "retro_achievements/client_observer.h"

namespace RetroAchievements {

class QtClientObserver : public QObject, public RetroAchievements::ClientObserver {
    Q_OBJECT

public:
    void OnLoginSucceeded(const User& user) override;
    void OnLoginFailed(int result, std::string_view error_message) override;

    void OnLoadGameSucceeded(const Game& game) override;
    void OnLoadGameFailed(int result, std::string_view error_message) override;
    void OnEvent(const rc_client_event_t* event) override;

signals:
    void LoginSucceeded(const User& user);
    void LoginFailed(const QString& error_message);

    void LoadGameSucceeded(const Game& game);
    void LoadGameFailed(const QString& error_message);

    void AchievementListChanged();

    void LeaderboardTrackerShown(quint32 id, const QString& display);
    void LeaderboardTrackerUpdated(quint32 id, const QString& display);
    void LeaderboardTrackerHidden(quint32 id);

    void EventNotification(const QString& title, const QString& body, const QString& image_url);
};

} // namespace RetroAchievements

Q_DECLARE_METATYPE(RetroAchievements::User)
Q_DECLARE_METATYPE(RetroAchievements::Game)
