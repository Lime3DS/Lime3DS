// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QMainWindow>
#include <QWidget>

namespace Ui {
class ConfigureStorage;
}

class ConfigureStorage : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureStorage(bool is_powered_on, QWidget* parent = nullptr);
    ~ConfigureStorage() override;

    void ApplyConfiguration();

    void MigrateFolder(const QString& from, const QString& dest);

    void RetranslateUI();
    void SetConfiguration();

    std::unique_ptr<Ui::ConfigureStorage> ui;
    bool is_powered_on;
    QWidget* parent;
};
