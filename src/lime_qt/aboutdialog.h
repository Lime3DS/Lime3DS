// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This is an epic test message

#pragma once

#include <memory>
#include <QDialog>

namespace Ui {
class AboutDialog;
}

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = 0);
    ~AboutDialog() override;

private:
    std::unique_ptr<Ui::AboutDialog> ui;
};
