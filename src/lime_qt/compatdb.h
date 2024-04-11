// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QFutureWatcher>
#include <QWizard>

namespace Core {
class TelemetrySession;
}

namespace Ui {
class CompatDB;
}

class CompatDB : public QWizard {
    Q_OBJECT

public:
    explicit CompatDB(Core::TelemetrySession& telemetry_session_, QWidget* parent = nullptr);
    ~CompatDB();

private:
    QFutureWatcher<bool> testcase_watcher;

    std::unique_ptr<Ui::CompatDB> ui;

    void Submit();
    void OnTestcaseSubmitted();
    void EnableNext();

    Core::TelemetrySession& telemetry_session;
};
