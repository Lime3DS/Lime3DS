// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>
#include <QDialog>
#include "common/param_package.h"
#include "core/dumping/ffmpeg_backend.h"

class QTreeWidgetItem;

namespace Ui {
class OptionsDialog;
}

class OptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget* parent, std::vector<VideoDumper::OptionInfo> specific_options,
                           std::vector<VideoDumper::OptionInfo> generic_options,
                           const std::string& current_value);
    ~OptionsDialog() override;

    std::string GetCurrentValue() const;

private:
    void PopulateOptions();
    void OnSetOptionValue(QTreeWidgetItem* item);

    std::unique_ptr<Ui::OptionsDialog> ui;
    std::vector<VideoDumper::OptionInfo> specific_options;
    std::vector<VideoDumper::OptionInfo> generic_options;
    Common::ParamPackage current_values;
};
