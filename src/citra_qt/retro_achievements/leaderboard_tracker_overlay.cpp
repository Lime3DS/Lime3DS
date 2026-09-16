// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/retro_achievements/leaderboard_tracker_overlay.h"

#include <algorithm>

#include <QEvent>
#include <QLabel>
#include <QLayout>

#include "citra_qt/util/util.h"
#include "common/logging/log.h"
#include "ui_leaderboard_tracker_overlay.h"

namespace RetroAchievements {

static const int margin = 16;
static const int tracker_width = 160;

LeaderboardTrackerOverlay::LeaderboardTrackerOverlay(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LeaderboardTrackerOverlay>()) {
    ui->setupUi(this);

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_StyledBackground, true);
    hide();

    parent->installEventFilter(this);
}

LeaderboardTrackerOverlay::~LeaderboardTrackerOverlay() = default;

void LeaderboardTrackerOverlay::ShowTracker(quint32 id, const QString& display) {
    if (QLabel* label = trackers.value(id)) {
        label->setText(display);
    } else {
        auto* new_label = new QLabel(display, this);
        new_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        new_label->setFont(GetMonospaceFont());
        new_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        ui->tracker_layout->addWidget(new_label);
        trackers.insert(id, new_label);
    }

    UpdatePositionAndSize();
    show();
    raise();
}

void LeaderboardTrackerOverlay::UpdateTracker(quint32 id, const QString& display) {
    QLabel* label = trackers.value(id);
    if (!label) {
        LOG_DEBUG(RetroAchievements, "Ignoring update for unknown leaderboard tracker {}", id);
        return;
    }

    label->setText(display);
    UpdatePositionAndSize();
    raise();
}

void LeaderboardTrackerOverlay::HideTracker(quint32 id) {
    QLabel* label = trackers.take(id);
    if (!label) {
        return;
    }

    ui->tracker_layout->removeWidget(label);
    delete label;

    if (trackers.isEmpty()) {
        hide();
    } else {
        UpdatePositionAndSize();
    }
}

void LeaderboardTrackerOverlay::Clear() {
    while (QLayoutItem* item = ui->tracker_layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    trackers.clear();
    hide();
}

bool LeaderboardTrackerOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Resize) {
        UpdatePositionAndSize();
    }
    return QWidget::eventFilter(watched, event);
}

void LeaderboardTrackerOverlay::UpdatePositionAndSize() {
    QWidget* parent = parentWidget();
    if (!parent || trackers.isEmpty()) {
        return;
    }

    ui->tracker_layout->activate();

    int width = std::min(tracker_width, parent->width());
    int height = std::min(ui->tracker_layout->sizeHint().height(), parent->height());
    setFixedSize(width, height);

    int x = std::max(0, parent->width() - width - margin);
    int y = std::max(0, parent->height() - height - margin);
    move(x, y);
}

} // namespace RetroAchievements
