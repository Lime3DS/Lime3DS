// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "lime_qt/util/clickable_label.h"

ClickableLabel::ClickableLabel(QWidget* parent, [[maybe_unused]] Qt::WindowFlags f)
    : QLabel(parent) {}

void ClickableLabel::mouseReleaseEvent([[maybe_unused]] QMouseEvent* event) {
    emit clicked();
}
