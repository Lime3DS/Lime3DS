// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QMessageBox>
#include "common/settings.h"
#include "core/core.h"
#include "lime_qt/dumping/dumping_dialog.h"
#include "lime_qt/dumping/options_dialog.h"
#include "lime_qt/uisettings.h"
#include "ui_dumping_dialog.h"

DumpingDialog::DumpingDialog(QWidget* parent, Core::System& system_)
    : QDialog(parent), ui{std::make_unique<Ui::DumpingDialog>()}, system{system_} {

    ui->setupUi(this);

    format_generic_options = VideoDumper::GetFormatGenericOptions();
    encoder_generic_options = VideoDumper::GetEncoderGenericOptions();

    connect(ui->pathExplore, &QToolButton::clicked, this, &DumpingDialog::OnToolButtonClicked);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, [this] {
        if (ui->pathLineEdit->text().isEmpty()) {
            QMessageBox::critical(this, tr("Lime3DS"), tr("Please specify the output path."));
            return;
        }
        ApplyConfiguration();
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &DumpingDialog::reject);
    connect(ui->formatOptionsButton, &QToolButton::clicked, [this] {
        OpenOptionsDialog(formats.at(ui->formatComboBox->currentData().toUInt()).options,
                          format_generic_options, ui->formatOptionsLineEdit);
    });
    connect(ui->videoEncoderOptionsButton, &QToolButton::clicked, [this] {
        OpenOptionsDialog(
            video_encoders.at(ui->videoEncoderComboBox->currentData().toUInt()).options,
            encoder_generic_options, ui->videoEncoderOptionsLineEdit);
    });
    connect(ui->audioEncoderOptionsButton, &QToolButton::clicked, [this] {
        OpenOptionsDialog(
            audio_encoders.at(ui->audioEncoderComboBox->currentData().toUInt()).options,
            encoder_generic_options, ui->audioEncoderOptionsLineEdit);
    });

    SetConfiguration();

    connect(ui->formatComboBox, qOverload<int>(&QComboBox::currentIndexChanged), [this] {
        ui->pathLineEdit->setText(QString{});
        ui->formatOptionsLineEdit->clear();
        PopulateEncoders();
    });

    connect(ui->videoEncoderComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            [this] { ui->videoEncoderOptionsLineEdit->clear(); });
    connect(ui->audioEncoderComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            [this] { ui->audioEncoderOptionsLineEdit->clear(); });
}

DumpingDialog::~DumpingDialog() = default;

QString DumpingDialog::GetFilePath() const {
    return ui->pathLineEdit->text();
}

void DumpingDialog::Populate() {
    formats = VideoDumper::ListFormats();
    video_encoders = VideoDumper::ListEncoders(AVMEDIA_TYPE_VIDEO);
    audio_encoders = VideoDumper::ListEncoders(AVMEDIA_TYPE_AUDIO);

    // Check that these are not empty
    QString missing;
    if (formats.empty()) {
        missing = tr("output formats");
    }
    if (video_encoders.empty()) {
        missing = tr("video encoders");
    }
    if (audio_encoders.empty()) {
        missing = tr("audio encoders");
    }

    if (!missing.isEmpty()) {
        QMessageBox::critical(this, tr("Lime3DS"),
                              tr("Could not find any available %1.\nPlease check your FFmpeg "
                                 "installation used for compilation.")
                                  .arg(missing));
        reject();
        return;
    }

    // Populate formats
    for (std::size_t i = 0; i < formats.size(); ++i) {
        const auto& format = formats[i];

        // Check format: only formats that have video encoders and audio encoders are displayed
        bool has_video = false;
        for (const auto& video_encoder : video_encoders) {
            if (format.supported_video_codecs.count(video_encoder.codec)) {
                has_video = true;
                break;
            }
        }
        if (!has_video)
            continue;

        bool has_audio = false;
        for (const auto& audio_encoder : audio_encoders) {
            if (format.supported_audio_codecs.count(audio_encoder.codec)) {
                has_audio = true;
                break;
            }
        }
        if (!has_audio)
            continue;

        ui->formatComboBox->addItem(tr("%1 (%2)").arg(QString::fromStdString(format.long_name),
                                                      QString::fromStdString(format.name)),
                                    static_cast<unsigned long long>(i));
        if (format.name == Settings::values.output_format) {
            ui->formatComboBox->setCurrentIndex(ui->formatComboBox->count() - 1);
        }
    }
    PopulateEncoders();
}

void DumpingDialog::PopulateEncoders() {
    const auto& format = formats.at(ui->formatComboBox->currentData().toUInt());

    ui->videoEncoderComboBox->clear();
    for (std::size_t i = 0; i < video_encoders.size(); ++i) {
        const auto& video_encoder = video_encoders[i];
        if (!format.supported_video_codecs.count(video_encoder.codec)) {
            continue;
        }

        ui->videoEncoderComboBox->addItem(
            tr("%1 (%2)").arg(QString::fromStdString(video_encoder.long_name),
                              QString::fromStdString(video_encoder.name)),
            static_cast<unsigned long long>(i));
        if (video_encoder.name == Settings::values.video_encoder) {
            ui->videoEncoderComboBox->setCurrentIndex(ui->videoEncoderComboBox->count() - 1);
        }
    }

    ui->audioEncoderComboBox->clear();
    for (std::size_t i = 0; i < audio_encoders.size(); ++i) {
        const auto& audio_encoder = audio_encoders[i];
        if (!format.supported_audio_codecs.count(audio_encoder.codec)) {
            continue;
        }

        ui->audioEncoderComboBox->addItem(
            tr("%1 (%2)").arg(QString::fromStdString(audio_encoder.long_name),
                              QString::fromStdString(audio_encoder.name)),
            static_cast<unsigned long long>(i));
        if (audio_encoder.name == Settings::values.audio_encoder) {
            ui->audioEncoderComboBox->setCurrentIndex(ui->audioEncoderComboBox->count() - 1);
        }
    }
}

void DumpingDialog::OnToolButtonClicked() {
    const auto& format = formats.at(ui->formatComboBox->currentData().toUInt());

    QString extensions;
    for (const auto& ext : format.extensions) {
        if (!extensions.isEmpty()) {
            extensions.append(QLatin1Char{' '});
        }
        extensions.append(QStringLiteral("*.%1").arg(QString::fromStdString(ext)));
    }

    const auto path = QFileDialog::getSaveFileName(
        this, tr("Select Video Output Path"), last_path,
        tr("%1 (%2)").arg(QString::fromStdString(format.long_name), extensions));
    if (!path.isEmpty()) {
        last_path = QFileInfo(ui->pathLineEdit->text()).path();
        ui->pathLineEdit->setText(path);
    }
}

void DumpingDialog::OpenOptionsDialog(const std::vector<VideoDumper::OptionInfo>& specific_options,
                                      const std::vector<VideoDumper::OptionInfo>& generic_options,
                                      QLineEdit* line_edit) {
    OptionsDialog dialog(this, specific_options, generic_options, line_edit->text().toStdString());
    if (dialog.exec() != QDialog::DialogCode::Accepted) {
        return;
    }

    line_edit->setText(QString::fromStdString(dialog.GetCurrentValue()));
}

void DumpingDialog::SetConfiguration() {
    Populate();

    ui->formatOptionsLineEdit->setText(QString::fromStdString(Settings::values.format_options));
    ui->videoEncoderOptionsLineEdit->setText(
        QString::fromStdString(Settings::values.video_encoder_options));
    ui->audioEncoderOptionsLineEdit->setText(
        QString::fromStdString(Settings::values.audio_encoder_options));
    last_path = UISettings::values.video_dumping_path;
    ui->videoBitrateSpinBox->setValue(static_cast<int>(Settings::values.video_bitrate));
    ui->audioBitrateSpinBox->setValue(static_cast<int>(Settings::values.audio_bitrate));
}

void DumpingDialog::ApplyConfiguration() {
    Settings::values.output_format = formats.at(ui->formatComboBox->currentData().toUInt()).name;
    Settings::values.format_options = ui->formatOptionsLineEdit->text().toStdString();
    Settings::values.video_encoder =
        video_encoders.at(ui->videoEncoderComboBox->currentData().toUInt()).name;
    Settings::values.video_encoder_options = ui->videoEncoderOptionsLineEdit->text().toStdString();
    Settings::values.video_bitrate = ui->videoBitrateSpinBox->value();
    Settings::values.audio_encoder =
        audio_encoders.at(ui->audioEncoderComboBox->currentData().toUInt()).name;
    Settings::values.audio_encoder_options = ui->audioEncoderOptionsLineEdit->text().toStdString();
    Settings::values.audio_bitrate = ui->audioBitrateSpinBox->value();
    UISettings::values.video_dumping_path = last_path;
    system.ApplySettings();
}
