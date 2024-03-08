// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QPushButton>
#include "citra_qt/movie/movie_record_dialog.h"
#include "citra_qt/uisettings.h"
#include "core/core.h"
#include "core/movie.h"
#include "ui_movie_record_dialog.h"

MovieRecordDialog::MovieRecordDialog(QWidget* parent, const Core::System& system_)
    : QDialog(parent), ui(std::make_unique<Ui::MovieRecordDialog>()), system{system_} {
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(ui->filePathButton, &QToolButton::clicked, this,
            &MovieRecordDialog::OnToolButtonClicked);
    connect(ui->filePath, &QLineEdit::editingFinished, this, &MovieRecordDialog::UpdateUIDisplay);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &MovieRecordDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &MovieRecordDialog::reject);

    QString note_text;
    if (system.IsPoweredOn()) {
        note_text = tr("Current running game will be restarted.");
        if (system.Movie().GetPlayMode() == Core::Movie::PlayMode::Recording) {
            note_text.append(tr("<br>Current recording will be discarded."));
        }
    } else {
        note_text = tr("Recording will start once you boot a game.");
    }
    ui->noteLabel->setText(note_text);
}

MovieRecordDialog::~MovieRecordDialog() = default;

QString MovieRecordDialog::GetPath() const {
    return ui->filePath->text();
}

QString MovieRecordDialog::GetAuthor() const {
    return ui->authorLineEdit->text();
}

void MovieRecordDialog::OnToolButtonClicked() {
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Record Movie"), UISettings::values.movie_record_path,
                                     tr("Citra TAS Movie (*.ctm)"));
    if (path.isEmpty()) {
        return;
    }
    ui->filePath->setText(path);
    UISettings::values.movie_record_path = path;
    UpdateUIDisplay();
}

void MovieRecordDialog::UpdateUIDisplay() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->filePath->text().isEmpty());
}
