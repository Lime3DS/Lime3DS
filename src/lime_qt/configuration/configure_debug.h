// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureDebug;
}

class ConfigureDebug : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureDebug(bool is_powered_on, QWidget* parent = nullptr);
    ~ConfigureDebug() override;

    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();
    void SetupPerGameUI();

private:
    std::unique_ptr<Ui::ConfigureDebug> ui;
    bool is_powered_on;
};
