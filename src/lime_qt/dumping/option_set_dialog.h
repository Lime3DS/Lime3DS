// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QDialog>
#include "core/dumping/ffmpeg_backend.h"

namespace Ui {
class OptionSetDialog;
}

class OptionSetDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionSetDialog(QWidget* parent, VideoDumper::OptionInfo option,
                             const std::string& initial_value);
    ~OptionSetDialog() override;

    // {is_set, value}
    std::pair<bool, std::string> GetCurrentValue();

private:
    void InitializeUI(const std::string& initial_value);
    void SetCheckBoxDefaults(const std::string& initial_value);
    void UpdateUIDisplay();

    std::unique_ptr<Ui::OptionSetDialog> ui;
    VideoDumper::OptionInfo option;
    bool is_set = true;
    int layout_type = -1; // 0 - line edit, 1 - combo box, 2 - flags (check boxes)
};
