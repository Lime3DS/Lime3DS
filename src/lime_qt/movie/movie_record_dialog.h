// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QDialog>

namespace Ui {
class MovieRecordDialog;
}

namespace Core {
class System;
}

class MovieRecordDialog : public QDialog {
    Q_OBJECT

public:
    explicit MovieRecordDialog(QWidget* parent, const Core::System& system);
    ~MovieRecordDialog() override;

    QString GetPath() const;
    QString GetAuthor() const;

private:
    void OnToolButtonClicked();
    void UpdateUIDisplay();

    std::unique_ptr<Ui::MovieRecordDialog> ui;
    const Core::System& system;
};
