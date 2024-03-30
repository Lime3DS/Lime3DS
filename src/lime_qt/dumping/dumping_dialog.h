// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>
#include <QDialog>
#include "core/dumping/ffmpeg_backend.h"

namespace Ui {
class DumpingDialog;
}

namespace Core {
class System;
}

class QLineEdit;

class DumpingDialog : public QDialog {
    Q_OBJECT

public:
    explicit DumpingDialog(QWidget* parent, Core::System& system);
    ~DumpingDialog() override;

    QString GetFilePath() const;
    void ApplyConfiguration();

private:
    void Populate();
    void PopulateEncoders();
    void SetConfiguration();
    void OnToolButtonClicked();
    void OpenOptionsDialog(const std::vector<VideoDumper::OptionInfo>& specific_options,
                           const std::vector<VideoDumper::OptionInfo>& generic_options,
                           QLineEdit* line_edit);

    std::unique_ptr<Ui::DumpingDialog> ui;
    Core::System& system;

    QString last_path;

    std::vector<VideoDumper::FormatInfo> formats;
    std::vector<VideoDumper::OptionInfo> format_generic_options;
    std::vector<VideoDumper::EncoderInfo> video_encoders;
    std::vector<VideoDumper::EncoderInfo> audio_encoders;
    std::vector<VideoDumper::OptionInfo> encoder_generic_options;
};
