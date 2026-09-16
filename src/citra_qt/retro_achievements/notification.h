// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QPixmap>
#include <QTimer>
#include <QWidget>

namespace Ui {
class RetroAchievementsNotification;
}

namespace RetroAchievements {

class Notification final : public QWidget {
    Q_OBJECT

public:
    explicit Notification(QWidget* parent);
    ~Notification() override;

    void Show(const QString& title, const QString& body, const QPixmap& image = {});
    void SetImage(const QPixmap& image);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void UpdatePositionInParent();

    std::unique_ptr<Ui::RetroAchievementsNotification> ui;
    QTimer display_timer;
};

} // namespace RetroAchievements
