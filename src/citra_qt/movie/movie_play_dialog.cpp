// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QPushButton>
#include <QTime>
#include "citra_qt/game_list.h"
#include "citra_qt/game_list_p.h"
#include "citra_qt/movie/movie_play_dialog.h"
#include "citra_qt/uisettings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/hid/hid.h"
#include "core/movie.h"
#include "ui_movie_play_dialog.h"

MoviePlayDialog::MoviePlayDialog(QWidget* parent, GameList* game_list_, const Core::System& system_)
    : QDialog(parent),
      ui(std::make_unique<Ui::MoviePlayDialog>()), game_list{game_list_}, system{system_} {
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(ui->filePathButton, &QToolButton::clicked, this, &MoviePlayDialog::OnToolButtonClicked);
    connect(ui->filePath, &QLineEdit::editingFinished, this, &MoviePlayDialog::UpdateUIDisplay);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &MoviePlayDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &MoviePlayDialog::reject);

    if (system.IsPoweredOn()) {
        QString note_text;
        note_text = tr("Current running game will be stopped.");
        if (system.Movie().GetPlayMode() == Core::Movie::PlayMode::Recording) {
            note_text.append(tr("<br>Current recording will be discarded."));
        }
        ui->note2Label->setText(note_text);
    }
}

MoviePlayDialog::~MoviePlayDialog() = default;

QString MoviePlayDialog::GetMoviePath() const {
    return ui->filePath->text();
}

QString MoviePlayDialog::GetGamePath() const {
    const auto metadata = system.Movie().GetMovieMetadata(GetMoviePath().toStdString());
    return game_list->FindGameByProgramID(metadata.program_id, GameListItemPath::FullPathRole);
}

void MoviePlayDialog::OnToolButtonClicked() {
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Play Movie"), UISettings::values.movie_playback_path,
                                     tr("Citra TAS Movie (*.ctm)"));
    if (path.isEmpty()) {
        return;
    }
    ui->filePath->setText(path);
    UISettings::values.movie_playback_path = path;
    UpdateUIDisplay();
}

void MoviePlayDialog::UpdateUIDisplay() {
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    ui->gameLineEdit->clear();
    ui->authorLineEdit->clear();
    ui->rerecordCountLineEdit->clear();
    ui->lengthLineEdit->clear();
    ui->note1Label->setVisible(true);

    const auto& movie = system.Movie();
    const auto path = GetMoviePath().toStdString();

    const auto validation_result = movie.ValidateMovie(path);
    if (validation_result == Core::Movie::ValidationResult::Invalid) {
        ui->note1Label->setText(tr("Invalid movie file."));
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }

    ui->note2Label->setVisible(true);
    ui->infoGroupBox->setVisible(true);

    switch (validation_result) {
    case Core::Movie::ValidationResult::OK:
        ui->note1Label->setText(QString{});
        break;
    case Core::Movie::ValidationResult::RevisionDismatch:
        ui->note1Label->setText(tr("Revision dismatch, playback may desync."));
        break;
    case Core::Movie::ValidationResult::InputCountDismatch:
        ui->note1Label->setText(tr("Indicated length is incorrect, file may be corrupted."));
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        break;
    default:
        UNREACHABLE();
    }

    const auto metadata = movie.GetMovieMetadata(path);

    // Format game title
    const auto title =
        game_list->FindGameByProgramID(metadata.program_id, GameListItemPath::TitleRole);
    if (title.isEmpty()) {
        ui->gameLineEdit->setText(tr("(unknown)"));
        ui->note1Label->setText(tr("Game used in this movie is not in game list."));
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    } else {
        ui->gameLineEdit->setText(title);
    }

    ui->authorLineEdit->setText(metadata.author.empty() ? tr("(unknown)")
                                                        : QString::fromStdString(metadata.author));
    ui->rerecordCountLineEdit->setText(
        metadata.rerecord_count == 0 ? tr("(unknown)") : QString::number(metadata.rerecord_count));

    // Format length
    if (metadata.input_count == 0) {
        ui->lengthLineEdit->setText(tr("(unknown)"));
    } else {
        if (metadata.input_count >
            BASE_CLOCK_RATE_ARM11 * 24 * 60 * 60 / Service::HID::Module::pad_update_ticks) {
            // More than a day
            ui->lengthLineEdit->setText(tr("(>1 day)"));
        } else {
            const u64 msecs = Service::HID::Module::pad_update_ticks * metadata.input_count * 1000 /
                              BASE_CLOCK_RATE_ARM11;
            ui->lengthLineEdit->setText(QTime::fromMSecsSinceStartOfDay(static_cast<int>(msecs))
                                            .toString(QStringLiteral("hh:mm:ss.zzz")));
        }
    }
}
