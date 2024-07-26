// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QTreeWidgetItem>
#include "lime_qt/dumping/option_set_dialog.h"
#include "lime_qt/dumping/options_dialog.h"
#include "ui_options_dialog.h"

constexpr char UNSET_TEXT[] = QT_TR_NOOP("[not set]");

void OptionsDialog::PopulateOptions() {
    const auto& options = ui->specificRadioButton->isChecked() ? specific_options : generic_options;
    ui->main->clear();
    ui->main->setSortingEnabled(false);
    for (std::size_t i = 0; i < options.size(); ++i) {
        const auto& option = options.at(i);
        auto* item = new QTreeWidgetItem(
            {QString::fromStdString(option.name), QString::fromStdString(current_values.Get(
                                                      option.name, tr(UNSET_TEXT).toStdString()))});
        item->setData(1, Qt::UserRole, static_cast<unsigned long long>(i)); // ID
        ui->main->addTopLevelItem(item);
    }
    ui->main->setSortingEnabled(true);
    ui->main->sortItems(0, Qt::AscendingOrder);
}

void OptionsDialog::OnSetOptionValue(QTreeWidgetItem* item) {
    const auto& options = ui->specificRadioButton->isChecked() ? specific_options : generic_options;
    const int id = item->data(1, Qt::UserRole).toInt();
    OptionSetDialog dialog(this, options[id],
                           current_values.Get(options[id].name, options[id].default_value));
    if (dialog.exec() != QDialog::DialogCode::Accepted) {
        return;
    }

    const auto& [is_set, value] = dialog.GetCurrentValue();
    if (is_set) {
        current_values.Set(options[id].name, value);
    } else {
        current_values.Erase(options[id].name);
    }
    item->setText(1, is_set ? QString::fromStdString(value) : tr(UNSET_TEXT));
}

std::string OptionsDialog::GetCurrentValue() const {
    return current_values.Serialize();
}

OptionsDialog::OptionsDialog(QWidget* parent,
                             std::vector<VideoDumper::OptionInfo> specific_options_,
                             std::vector<VideoDumper::OptionInfo> generic_options_,
                             const std::string& current_value)
    : QDialog(parent), ui(std::make_unique<Ui::OptionsDialog>()),
      specific_options(std::move(specific_options_)), generic_options(std::move(generic_options_)),
      current_values(current_value) {

    ui->setupUi(this);
    PopulateOptions();

    connect(ui->main, &QTreeWidget::itemDoubleClicked,
            [this](QTreeWidgetItem* item, int column) { OnSetOptionValue(item); });
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &OptionsDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &OptionsDialog::reject);
    connect(ui->specificRadioButton, &QRadioButton::toggled, this, &OptionsDialog::PopulateOptions);
}

OptionsDialog::~OptionsDialog() = default;
