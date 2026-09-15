// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
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

    connect(ui->turbo_limit, &QSlider::valueChanged, this, [&](double value) {
        Settings::values.turbo_limit.SetValue(SliderToSettings(value));
        ui->turbo_limit_display_label->setText(
            QStringLiteral("%1%").arg(Settings::values.turbo_limit.GetValue()));
    });

    // Set a minimum width for the label to prevent the slider from changing size.
    // This scales across DPIs, and is acceptable for uncapitalized strings.
    const auto width = static_cast<int>(tr("unthrottled").size() * 6);
    ui->emulation_speed_display_label->setMinimumWidth(width);
    ui->emulation_speed_combo->setVisible(!Settings::IsConfiguringGlobal());
    ui->screenshot_combo->setVisible(!Settings::IsConfiguringGlobal());
#ifndef __unix__
    ui->toggle_gamemode->setVisible(false);
#endif
#ifndef ENABLE_QT_UPDATE_CHECKER
    ui->updates_group->setVisible(false);
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
        ui->change_screenshot_dir->setEnabled(false);
        const QString dir_path = QFileDialog::getExistingDirectory(
            this, tr("Select Screenshot Directory"), ui->screenshot_dir_path->text(),
            QFileDialog::ShowDirsOnly);
        if (!dir_path.isEmpty()) {
            ui->screenshot_dir_path->setText(dir_path);
        }
        ui->change_screenshot_dir->setEnabled(true);
    });
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    if (Settings::IsConfiguringGlobal()) {
        ui->turbo_limit->setValue(SettingsToSlider(Settings::values.turbo_limit.GetValue()));
        ui->turbo_limit_display_label->setText(
            QStringLiteral("%1%").arg(Settings::values.turbo_limit.GetValue()));

        ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing.GetValue());
        ui->toggle_background_pause->setChecked(
            UISettings::values.pause_when_in_background.GetValue());
        ui->toggle_background_mute->setChecked(
            UISettings::values.mute_when_in_background.GetValue());
        ui->toggle_hide_mouse->setChecked(UISettings::values.hide_mouse.GetValue());
#ifdef ENABLE_QT_UPDATE_CHECKER
        ui->toggle_update_checker->setChecked(
            UISettings::values.check_for_update_on_start.GetValue());
        ui->update_channel_combobox->setCurrentIndex(
            UISettings::values.update_check_channel.GetValue());
#endif
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
    ui->button_reset_defaults->setEnabled(false);
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, QStringLiteral("Azahar"),
        tr("Are you sure you want to <b>reset your settings</b> and close Azahar?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::No) {
        ui->button_reset_defaults->setEnabled(true);
        return;
    }

    FileUtil::Delete(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "qt-config.ini");
    FileUtil::DeleteDirRecursively(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "custom");
    qApp->quit();
}

void ConfigureGeneral::ApplyConfiguration() {
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
#ifdef ENABLE_QT_UPDATE_CHECKER
        UISettings::values.check_for_update_on_start = ui->toggle_update_checker->isChecked();
        UISettings::values.update_check_channel = ui->update_channel_combobox->currentIndex();
#endif
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

    ui->turbo_limit->setVisible(false);
    ui->general_group->setVisible(false);
    ui->button_reset_defaults->setVisible(false);
    ui->toggle_gamemode->setVisible(false);
    ui->updates_group->setVisible(false);
}
