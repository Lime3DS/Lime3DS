// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/retro_achievements/notification.h"

#include <QEvent>
#include <QTimer>

#include "ui_notification.h"

namespace RetroAchievements {

static const int margin = 16;
static const int duration_ms = 5000;

Notification::Notification(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::RetroAchievementsNotification>()),
      display_timer(QTimer(this)) {
    ui->setupUi(this);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_StyledBackground, true);

    display_timer.setSingleShot(true);
    connect(&display_timer, &QTimer::timeout, this, &Notification::hide);

    parent->installEventFilter(this);
}

Notification::~Notification() = default;

void Notification::Show(const QString& title, const QString& body, const QPixmap& image) {
    display_timer.stop();

    ui->title->setText(title);
    ui->body->setText(body);
    SetImage(image);

    adjustSize();
    UpdatePositionInParent();
    show();
    raise();
    display_timer.start(duration_ms);
}

void Notification::SetImage(const QPixmap& image) {
    if (image.isNull()) {
        ui->image->clear();
        return;
    }

    ui->image->setPixmap(
        image.scaled(ui->image->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

bool Notification::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize) {
        UpdatePositionInParent();
    }
    return QWidget::eventFilter(watched, event);
}

void Notification::UpdatePositionInParent() {
    if (QWidget* parent = parentWidget()) {
        move(margin, margin);
    }
}

} // namespace RetroAchievements
