// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QWidget>
#include "common/microprofile.h"

class MicroProfileDialog : public QWidget {
    Q_OBJECT

public:
    explicit MicroProfileDialog(QWidget* parent = nullptr);

    /// Returns a QAction that can be used to toggle visibility of this dialog.
    QAction* toggleViewAction();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QAction* toggle_view_action = nullptr;
};
