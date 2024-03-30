// Copyright 2021 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
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
    void RetranslateUI();
    void SetConfiguration();

    std::unique_ptr<Ui::ConfigureStorage> ui;
    bool is_powered_on;
};
