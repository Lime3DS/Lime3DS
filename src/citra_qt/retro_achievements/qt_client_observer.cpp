// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "qt_client_observer.h"

#include <QString>

#include "common/logging/log.h"

namespace RetroAchievements {

void QtClientObserver::OnLoginSucceeded(const User& user) {
    emit LoginSucceeded(user);
}

void QtClientObserver::OnLoginFailed(int result, std::string_view error_message) {
    emit LoginFailed(QString::fromUtf8(error_message.data(), error_message.size()));
}

void QtClientObserver::OnLoadGameSucceeded(const Game& game) {
    emit LoadGameSucceeded(game);
}

void QtClientObserver::OnLoadGameFailed(int result, std::string_view error_message) {
    emit LoadGameFailed(QString::fromUtf8(error_message.data(), error_message.size()));
}

void QtClientObserver::OnEvent(const rc_client_event_t* event) {
    QString title, body, image_url;

    switch (event->type) {
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
        emit LeaderboardTrackerShown(event->leaderboard_tracker->id,
                                     QString::fromUtf8(event->leaderboard_tracker->display));
        return;
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
        emit LeaderboardTrackerUpdated(event->leaderboard_tracker->id,
                                       QString::fromUtf8(event->leaderboard_tracker->display));
        return;
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
        emit LeaderboardTrackerHidden(event->leaderboard_tracker->id);
        return;
    default:
        break;
    }

    switch (event->type) {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
    case RC_CLIENT_EVENT_DISCONNECTED:
    case RC_CLIENT_EVENT_RECONNECTED:
        emit AchievementListChanged();
        break;
    default:
        break;
    }

    switch (event->type) {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        title = QStringLiteral("Achievement Unlocked");
        body = QString::fromUtf8(event->achievement->title);
        image_url = QString::fromUtf8(event->achievement->badge_url);
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
        title = QStringLiteral("Leaderboard Started");
        body = QString::fromUtf8(event->leaderboard->title);
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
        title = QStringLiteral("Leaderboard Failed");
        body = QString::fromUtf8(event->leaderboard->title);
        break;
    case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
        title = QStringLiteral("Leaderboard Submitted");
        body = QString::fromUtf8(event->leaderboard->title);
        break;
    case RC_CLIENT_EVENT_GAME_COMPLETED:
        title = QStringLiteral("Game Completed");
        body = QStringLiteral("All achievements earned.");
        break;
    case RC_CLIENT_EVENT_SUBSET_COMPLETED:
        title = QStringLiteral("Subset Completed");
        body = QString::fromUtf8(event->subset->title);
        image_url = QString::fromUtf8(event->subset->badge_url);
        break;
    case RC_CLIENT_EVENT_SERVER_ERROR:
        title = QStringLiteral("RetroAchievements Error");
        body = QString::fromUtf8(event->server_error->error_message);
        break;
    case RC_CLIENT_EVENT_DISCONNECTED:
        title = QStringLiteral("RetroAchievements Disconnected");
        body = QStringLiteral("Unlocks will be submitted when the connection is restored.");
        break;
    case RC_CLIENT_EVENT_RECONNECTED:
        title = QStringLiteral("RetroAchievements Reconnected");
        body = QStringLiteral("Pending unlocks submitted.");
        break;
    default:
        return;
    }

    emit EventNotification(title, body, image_url);
}

} // namespace RetroAchievements
