// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QUrl>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_general.h"
#include "citra_qt/uisettings.h"
#include "common/file_util.h"
#include "common/settings.h"
#include "ui_configure_general.h"

// The QSlider doesn't have an easy way to set a custom step amount,
// so we can just convert from the sliders range (0 - 198) to the expected
// settings range (5 - 995) with simple math.
static constexpr int SliderToSettings(int value) {
    return 5 * value + 5;
}

static constexpr int SettingsToSlider(int value) {
    return (value - 5) / 5;
}

ConfigureGeneral::ConfigureGeneral(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureGeneral>()) {

    ui->setupUi(this);

    // Set a minimum width for the label to prevent the slider from changing size.
    // This scales across DPIs, and is acceptable for uncapitalized strings.
    const auto width = static_cast<int>(tr("unthrottled").size() * 6);
    ui->emulation_speed_display_label->setMinimumWidth(width);
    ui->emulation_speed_combo->setVisible(!Settings::IsConfiguringGlobal());
    ui->screenshot_combo->setVisible(!Settings::IsConfiguringGlobal());
    ui->updateBox->setVisible(UISettings::values.updater_found);
#ifndef __unix__
    ui->toggle_gamemode->setVisible(false);
#endif

    SetupPerGameUI();
    SetConfiguration();

    connect(ui->button_reset_defaults, &QPushButton::clicked, this,
            &ConfigureGeneral::ResetDefaults);

    connect(ui->frame_limit, &QSlider::valueChanged, this, [&](int value) {
        if (value == ui->frame_limit->maximum()) {
            ui->emulation_speed_display_label->setText(tr("unthrottled"));
        } else {
            ui->emulation_speed_display_label->setText(
                QStringLiteral("%1%")
                    .arg(SliderToSettings(value))
                    .rightJustified(tr("unthrottled").size()));
        }
    });

    connect(ui->change_screenshot_dir, &QToolButton::clicked, this, [this] {
        const QString dir_path = QFileDialog::getExistingDirectory(
            this, tr("Select Screenshot Directory"), ui->screenshot_dir_path->text(),
            QFileDialog::ShowDirsOnly);
        if (!dir_path.isEmpty()) {
            ui->screenshot_dir_path->setText(dir_path);
        }
    });
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing.GetValue());
        ui->toggle_background_pause->setChecked(
            UISettings::values.pause_when_in_background.GetValue());
        ui->toggle_background_mute->setChecked(
            UISettings::values.mute_when_in_background.GetValue());
        ui->toggle_hide_mouse->setChecked(UISettings::values.hide_mouse.GetValue());

        ui->toggle_update_check->setChecked(
            UISettings::values.check_for_update_on_start.GetValue());
        ui->toggle_auto_update->setChecked(UISettings::values.update_on_close.GetValue());
#ifdef __unix__
        ui->toggle_gamemode->setChecked(Settings::values.enable_gamemode.GetValue());
#endif
    }

    if (Settings::values.frame_limit.GetValue() == 0) {
        ui->frame_limit->setValue(ui->frame_limit->maximum());
    } else {
        ui->frame_limit->setValue(SettingsToSlider(Settings::values.frame_limit.GetValue()));
    }
    if (ui->frame_limit->value() == ui->frame_limit->maximum()) {
        ui->emulation_speed_display_label->setText(tr("unthrottled"));
    } else {
        ui->emulation_speed_display_label->setText(
            QStringLiteral("%1%")
                .arg(SliderToSettings(ui->frame_limit->value()))
                .rightJustified(tr("unthrottled").size()));
    }

    if (!Settings::IsConfiguringGlobal()) {
        if (Settings::values.frame_limit.UsingGlobal()) {
            ui->emulation_speed_combo->setCurrentIndex(0);
            ui->frame_limit->setEnabled(false);
        } else {
            ui->emulation_speed_combo->setCurrentIndex(1);
            ui->frame_limit->setEnabled(true);
        }
        if (UISettings::values.screenshot_path.UsingGlobal()) {
            ui->screenshot_combo->setCurrentIndex(0);
            ui->screenshot_dir_path->setEnabled(false);
            ui->change_screenshot_dir->setEnabled(false);
        } else {
            ui->screenshot_combo->setCurrentIndex(1);
            ui->screenshot_dir_path->setEnabled(true);
            ui->change_screenshot_dir->setEnabled(true);
        }
        ConfigurationShared::SetHighlight(ui->widget_screenshot,
                                          !UISettings::values.screenshot_path.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->emulation_speed_layout,
                                          !Settings::values.frame_limit.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->widget_region,
                                          !Settings::values.region_value.UsingGlobal());
        const bool is_region_global = Settings::values.region_value.UsingGlobal();
        ui->region_combobox->setCurrentIndex(
            is_region_global ? ConfigurationShared::USE_GLOBAL_INDEX
                             : static_cast<int>(Settings::values.region_value.GetValue()) +
                                   ConfigurationShared::USE_GLOBAL_OFFSET + 1);
    } else {
        // The first item is "auto-select" with actual value -1, so plus one here will do the trick
        ui->region_combobox->setCurrentIndex(Settings::values.region_value.GetValue() + 1);
    }

    UISettings::values.screenshot_path.SetGlobal(ui->screenshot_combo->currentIndex() ==
                                                 ConfigurationShared::USE_GLOBAL_INDEX);
    std::string screenshot_path = UISettings::values.screenshot_path.GetValue();
    if (screenshot_path.empty()) {
        screenshot_path = FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + "screenshots/";
        FileUtil::CreateFullPath(screenshot_path);
        UISettings::values.screenshot_path = screenshot_path;
    }
    ui->screenshot_dir_path->setText(QString::fromStdString(screenshot_path));
}

void ConfigureGeneral::ResetDefaults() {
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Citra"),
        tr("Are you sure you want to <b>reset your settings</b> and close Citra?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::No) {
        return;
    }

    FileUtil::Delete(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "qt-config.ini");
    FileUtil::DeleteDirRecursively(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "custom");
    std::exit(0);
}

void ConfigureGeneral::ApplyConfiguration() {
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.region_value, ui->region_combobox,
                                             [](s32 index) { return index - 1; });

    ConfigurationShared::ApplyPerGameSetting(
        &Settings::values.frame_limit, ui->emulation_speed_combo, [this](s32) {
            const bool is_maximum = ui->frame_limit->value() == ui->frame_limit->maximum();
            return is_maximum ? 0 : SliderToSettings(ui->frame_limit->value());
        });

    ConfigurationShared::ApplyPerGameSetting(
        &UISettings::values.screenshot_path, ui->screenshot_combo,
        [this](s32) { return ui->screenshot_dir_path->text().toStdString(); });

    if (Settings::IsConfiguringGlobal()) {
        UISettings::values.confirm_before_closing = ui->toggle_check_exit->isChecked();
        UISettings::values.pause_when_in_background = ui->toggle_background_pause->isChecked();
        UISettings::values.mute_when_in_background = ui->toggle_background_mute->isChecked();
        UISettings::values.hide_mouse = ui->toggle_hide_mouse->isChecked();

        UISettings::values.check_for_update_on_start = ui->toggle_update_check->isChecked();
        UISettings::values.update_on_close = ui->toggle_auto_update->isChecked();
#ifdef __unix__
        Settings::values.enable_gamemode = ui->toggle_gamemode->isChecked();
#endif
    }
}

void ConfigureGeneral::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGeneral::SetupPerGameUI() {
    if (Settings::IsConfiguringGlobal()) {
        ui->region_combobox->setEnabled(Settings::values.region_value.UsingGlobal());
        ui->frame_limit->setEnabled(Settings::values.frame_limit.UsingGlobal());
        return;
    }

    connect(ui->emulation_speed_combo, qOverload<int>(&QComboBox::activated), this,
            [this](int index) {
                ui->frame_limit->setEnabled(index == 1);
                ConfigurationShared::SetHighlight(ui->emulation_speed_layout, index == 1);
            });

    connect(ui->screenshot_combo, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        ui->screenshot_dir_path->setEnabled(index == 1);
        ui->change_screenshot_dir->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(ui->widget_screenshot, index == 1);
    });

    ui->general_group->setVisible(false);
    ui->updateBox->setVisible(false);
    ui->button_reset_defaults->setVisible(false);
    ui->toggle_gamemode->setVisible(false);

    ConfigurationShared::SetColoredComboBox(
        ui->region_combobox, ui->widget_region,
        static_cast<u32>(Settings::values.region_value.GetValue(true) + 1));
}
