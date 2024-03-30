// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QLabel>
#include <QWidget>

class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~ClickableLabel() = default;

signals:
    void clicked();

protected:
    void mouseReleaseEvent(QMouseEvent* event);
};
