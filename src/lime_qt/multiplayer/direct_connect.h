// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include <QFutureWatcher>
#include "lime_qt/multiplayer/validation.h"

namespace Ui {
class DirectConnect;
}

namespace Core {
class System;
}

class DirectConnectWindow : public QDialog {
    Q_OBJECT

public:
    explicit DirectConnectWindow(Core::System& system, QWidget* parent = nullptr);
    ~DirectConnectWindow();

    void RetranslateUi();

signals:
    /**
     * Signalled by this widget when it is closing itself and destroying any state such as
     * connections that it might have.
     */
    void Closed();

private slots:
    void OnConnection();

private:
    void Connect();
    void BeginConnecting();
    void EndConnecting();

    QFutureWatcher<void>* watcher;
    std::unique_ptr<Ui::DirectConnect> ui;
    Core::System& system;
    Validation validation;
};
