// Copyright 2016-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_debug.h"
#include "citra_qt/debugger/console.h"
#include "citra_qt/uisettings.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_debug.h"
#ifdef ENABLE_VULKAN
#include "video_core/renderer_vulkan/vk_instance.h"
#endif

// The QSlider doesn't have an easy way to set a custom step amount,
// so we can just convert from the sliders range (0 - 79) to the expected
// settings range (5 - 400) with simple math.
static constexpr int SliderToSettings(int value) {
    return 5 * value + 5;
}

static constexpr int SettingsToSlider(int value) {
    return (value - 5) / 5;
}

ConfigureDebug::ConfigureDebug(bool is_powered_on_, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureDebug>()), is_powered_on{is_powered_on_} {
    ui->setupUi(this);
    SetConfiguration();

    connect(ui->toggle_gdbstub, &QCheckBox::clicked,
            [this](bool checked) { ui->debug_next_process->setEnabled(checked); });

    connect(ui->debug_next_process, &QCheckBox::clicked, [](bool checked) {
        if (checked) {
            Core::System::GetInstance().SetDebugNextProcessFlag();
        } else {
            Core::System::GetInstance().ClearDebugNextProcessFlag();
        }
    });

    connect(ui->open_log_button, &QPushButton::clicked, []() {
        QString path = QString::fromStdString(FileUtil::GetUserPath(FileUtil::UserPath::LogDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

#ifdef ENABLE_VULKAN
    connect(ui->toggle_renderer_debug, &QCheckBox::clicked, this, [this](bool checked) {
        if (checked && Settings::values.graphics_api.GetValue() == Settings::GraphicsAPI::Vulkan) {
            try {
                Vulkan::Instance debug_inst{true};
            } catch (vk::LayerNotPresentError&) {
                ui->toggle_renderer_debug->toggle();
                QMessageBox::warning(this, tr("Validation layer not available"),
                                     tr("Unable to enable debug renderer because the layer "
                                        "<strong>VK_LAYER_KHRONOS_validation</strong> is missing. "
                                        "Please install the Vulkan SDK or the appropriate package "
                                        "of your distribution"));
            }
        }
    });

    connect(ui->toggle_dump_command_buffers, &QCheckBox::clicked, this, [this](bool checked) {
        if (checked && Settings::values.graphics_api.GetValue() == Settings::GraphicsAPI::Vulkan) {
            try {
                Vulkan::Instance debug_inst{false, true};
            } catch (vk::LayerNotPresentError&) {
                ui->toggle_dump_command_buffers->toggle();
                QMessageBox::warning(this, tr("Command buffer dumping not available"),
                                     tr("Unable to enable command buffer dumping because the layer "
                                        "<strong>VK_LAYER_LUNARG_api_dump</strong> is missing. "
                                        "Please install the Vulkan SDK or the appropriate package "
                                        "of your distribution"));
            }
        }
    });
#endif

    connect(ui->toggle_pica_debugging, &QCheckBox::clicked, this, [this](bool checked) {
        QMessageBox::information(this, tr("Relaunch Required"),
                                 tr("Please relaunch Azahar for this setting to take effect."));
    });

    ui->toggle_cpu_jit->setEnabled(!is_powered_on);
    ui->toggle_renderer_debug->setEnabled(!is_powered_on);
    ui->toggle_pica_debugging->setEnabled(!is_powered_on);
    ui->toggle_dump_command_buffers->setEnabled(!is_powered_on);
    ui->enable_rpc_server->setEnabled(!is_powered_on);
    ui->toggle_unique_data_console_type->setEnabled(!is_powered_on);

    // Set a minimum width for the label to prevent the slider from changing size.
    // This scales across DPIs. (This value should be enough for "xxx%")
    ui->clock_display_label->setMinimumWidth(40);

    connect(ui->slider_clock_speed, &QSlider::valueChanged, this, [&](int value) {
        ui->clock_display_label->setText(QStringLiteral("%1%").arg(SliderToSettings(value)));
    });

    ui->clock_speed_label->setVisible(Settings::IsConfiguringGlobal());
    ui->clock_speed_combo->setVisible(!Settings::IsConfiguringGlobal());

#ifndef ENABLE_GDBSTUB
    ui->gdb_groupbox->setVisible(false);
#endif

    SetupPerGameUI();
}

ConfigureDebug::~ConfigureDebug() = default;

void ConfigureDebug::SetConfiguration() {
    ui->toggle_gdbstub->setChecked(Settings::values.use_gdbstub.GetValue());
    if (!ui->toggle_gdbstub->isChecked()) {
        ui->debug_next_process->setEnabled(false);
    }
    ui->gdbport_spinbox->setEnabled(Settings::values.use_gdbstub.GetValue());
    ui->gdbport_spinbox->setValue(Settings::values.gdbstub_port.GetValue());
    ui->toggle_console->setEnabled(!is_powered_on);
    ui->toggle_console->setChecked(UISettings::values.show_console.GetValue());
    ui->log_filter_edit->setText(QString::fromStdString(Settings::values.log_filter.GetValue()));
    ui->log_regex_filter_edit->setText(
        QString::fromStdString(Settings::values.log_regex_filter.GetValue()));
    ui->toggle_cpu_jit->setChecked(Settings::values.use_cpu_jit.GetValue());
    ui->delay_start_for_lle_modules->setChecked(
        Settings::values.delay_start_for_lle_modules.GetValue());
    ui->deterministic_async_operations->setChecked(
        Settings::values.deterministic_async_operations.GetValue());
    ui->enable_rpc_server->setChecked(Settings::values.enable_rpc_server.GetValue());
#ifndef ENABLE_SCRIPTING
    ui->enable_rpc_server->setVisible(false);
#endif // !ENABLE_SCRIPTING
    ui->toggle_unique_data_console_type->setChecked(
        Settings::values.toggle_unique_data_console_type.GetValue());
    ui->enable_exception_handler->setChecked(Settings::values.enable_exception_handler.GetValue());

    ui->toggle_renderer_debug->setChecked(Settings::values.renderer_debug.GetValue());
    ui->toggle_pica_debugging->setChecked(Settings::values.pica_debugging.GetValue());
    ui->toggle_dump_command_buffers->setChecked(Settings::values.dump_command_buffers.GetValue());

    if (!Settings::IsConfiguringGlobal()) {
        if (Settings::values.cpu_clock_percentage.UsingGlobal()) {
            ui->clock_speed_combo->setCurrentIndex(0);
            ui->slider_clock_speed->setEnabled(false);
        } else {
            ui->clock_speed_combo->setCurrentIndex(1);
            ui->slider_clock_speed->setEnabled(true);
        }
        ConfigurationShared::SetHighlight(ui->clock_speed_widget,
                                          !Settings::values.cpu_clock_percentage.UsingGlobal());
    }

    ui->slider_clock_speed->setValue(
        SettingsToSlider(Settings::values.cpu_clock_percentage.GetValue()));
    ui->clock_display_label->setText(
        QStringLiteral("%1%").arg(Settings::values.cpu_clock_percentage.GetValue()));
    ui->instant_debug_log->setChecked(Settings::values.instant_debug_log.GetValue());

    if (Core::System::GetInstance().GetDebugNextProcessFlag()) {
        ui->debug_next_process->setChecked(true);
    }
}

void ConfigureDebug::ApplyConfiguration() {
    Settings::values.use_gdbstub = ui->toggle_gdbstub->isChecked();
    Settings::values.gdbstub_port = static_cast<u16>(ui->gdbport_spinbox->value());
    UISettings::values.show_console = ui->toggle_console->isChecked();
    Settings::values.log_filter = ui->log_filter_edit->text().toStdString();
    Settings::values.log_regex_filter = ui->log_regex_filter_edit->text().toStdString();
    Debugger::ToggleConsole();
    Common::Log::Filter filter;
    filter.ParseFilterString(Settings::values.log_filter.GetValue());
    Common::Log::SetGlobalFilter(filter);
    Common::Log::SetRegexFilter(Settings::values.log_regex_filter.GetValue());
    Settings::values.use_cpu_jit = ui->toggle_cpu_jit->isChecked();
    Settings::values.delay_start_for_lle_modules = ui->delay_start_for_lle_modules->isChecked();
    Settings::values.deterministic_async_operations =
        ui->deterministic_async_operations->isChecked();
    Settings::values.enable_rpc_server = ui->enable_rpc_server->isChecked();
    Settings::values.toggle_unique_data_console_type =
        ui->toggle_unique_data_console_type->isChecked();
    Settings::values.enable_exception_handler = ui->enable_exception_handler->isChecked();
    Settings::values.renderer_debug = ui->toggle_renderer_debug->isChecked();
    Settings::values.pica_debugging = ui->toggle_pica_debugging->isChecked();
    Settings::values.dump_command_buffers = ui->toggle_dump_command_buffers->isChecked();
    Settings::values.instant_debug_log = ui->instant_debug_log->isChecked();

    ConfigurationShared::ApplyPerGameSetting(
        &Settings::values.cpu_clock_percentage, ui->clock_speed_combo,
        [this](s32) { return SliderToSettings(ui->slider_clock_speed->value()); });
}

void ConfigureDebug::SetupPerGameUI() {
    // Block the global settings if a game is currently running that overrides them
    if (Settings::IsConfiguringGlobal()) {
        ui->slider_clock_speed->setEnabled(Settings::values.cpu_clock_percentage.UsingGlobal());
        return;
    }

    connect(ui->clock_speed_combo, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        ui->slider_clock_speed->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(ui->clock_speed_widget, index == 1);
    });

    ui->gdb_groupbox->setVisible(false);
    ui->groupBox_2->setVisible(false);
    ui->enable_rpc_server->setVisible(false);
    ui->toggle_unique_data_console_type->setVisible(false);
    ui->toggle_cpu_jit->setVisible(false);
}

void ConfigureDebug::RetranslateUI() {
    ui->retranslateUi(this);
}
