// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QHash>
#include <QWidget>

class QLabel;

namespace Ui {
class LeaderboardTrackerOverlay;
}

namespace RetroAchievements {

class LeaderboardTrackerOverlay : public QWidget {
    Q_OBJECT

public:
    explicit LeaderboardTrackerOverlay(QWidget* parent);
    ~LeaderboardTrackerOverlay() override;

public slots:
    void ShowTracker(quint32 id, const QString& display);
    void UpdateTracker(quint32 id, const QString& display);
    void HideTracker(quint32 id);
    void Clear();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void UpdatePositionAndSize();

    std::unique_ptr<Ui::LeaderboardTrackerOverlay> ui;
    QHash<quint32, QLabel*> trackers;
};

} // namespace RetroAchievements
