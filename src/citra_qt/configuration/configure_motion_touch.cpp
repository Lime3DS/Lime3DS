// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <QCloseEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include "citra_qt/configuration/configure_motion_touch.h"
#include "citra_qt/configuration/configure_touch_from_button.h"
#include "common/logging/log.h"
#include "input_common/main.h"
#include "ui_configure_motion_touch.h"

CalibrationConfigurationDialog::CalibrationConfigurationDialog(QWidget* parent,
                                                               const std::string& host, u16 port,
                                                               u8 pad_index, u16 client_id)
    : QDialog(parent) {
    layout = new QVBoxLayout;
    status_label = new QLabel(tr("Communicating with the server..."));
    cancel_button = new QPushButton(tr("Cancel"));
    connect(cancel_button, &QPushButton::clicked, this, [this] {
        if (!completed) {
            job->Stop();
        }
        accept();
    });
    layout->addWidget(status_label);
    layout->addWidget(cancel_button);
    setLayout(layout);

    using namespace InputCommon::CemuhookUDP;
    job = std::make_unique<CalibrationConfigurationJob>(
        host, port, pad_index, client_id,
        [this](CalibrationConfigurationJob::Status status) {
            QString text;
            switch (status) {
            case CalibrationConfigurationJob::Status::Ready:
                text = tr("Touch the top left corner <br>of your touchpad.");
                break;
            case CalibrationConfigurationJob::Status::Stage1Completed:
                text = tr("Now touch the bottom right corner <br>of your touchpad.");
                break;
            case CalibrationConfigurationJob::Status::Completed:
                text = tr("Configuration completed!");
                break;
            default:
                LOG_ERROR(Frontend, "Unknown calibration status {}", status);
                break;
            }
            QMetaObject::invokeMethod(this, "UpdateLabelText", Q_ARG(QString, text));
            if (status == CalibrationConfigurationJob::Status::Completed) {
                QMetaObject::invokeMethod(this, "UpdateButtonText", Q_ARG(QString, tr("OK")));
            }
        },
        [this](u16 min_x_, u16 min_y_, u16 max_x_, u16 max_y_) {
            completed = true;
            min_x = min_x_;
            min_y = min_y_;
            max_x = max_x_;
            max_y = max_y_;
        });
}

CalibrationConfigurationDialog::~CalibrationConfigurationDialog() = default;

void CalibrationConfigurationDialog::UpdateLabelText(const QString& text) {
    status_label->setText(text);
}

void CalibrationConfigurationDialog::UpdateButtonText(const QString& text) {
    cancel_button->setText(text);
}

constexpr std::array<std::pair<const char*, const char*>, 3> MotionProviders = {{
    {"motion_emu", QT_TRANSLATE_NOOP("ConfigureMotionTouch", "Mouse (Right Click)")},
    {"cemuhookudp", QT_TRANSLATE_NOOP("ConfigureMotionTouch", "CemuhookUDP")},
    {"sdl", QT_TRANSLATE_NOOP("ConfigureMotionTouch", "SDL")},
}};

constexpr std::array<std::pair<const char*, const char*>, 2> TouchProviders = {{
    {"emu_window", QT_TRANSLATE_NOOP("ConfigureMotionTouch", "Emulator Window")},
    {"cemuhookudp", QT_TRANSLATE_NOOP("ConfigureMotionTouch", "CemuhookUDP")},
}};

ConfigureMotionTouch::ConfigureMotionTouch(QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigureMotionTouch>()),
      timeout_timer(std::make_unique<QTimer>()), poll_timer(std::make_unique<QTimer>()) {
    ui->setupUi(this);
    for (const auto& [provider, name] : MotionProviders) {
        ui->motion_provider->addItem(tr(name), QString::fromUtf8(provider));
    }
    for (const auto& [provider, name] : TouchProviders) {
        ui->touch_provider->addItem(tr(name), QString::fromUtf8(provider));
    }

    ui->udp_learn_more->setOpenExternalLinks(true);
    ui->udp_learn_more->setText(
        tr("<a "
           "href='https://citra-emu.org/wiki/"
           "using-a-controller-or-android-phone-for-motion-or-touch-input'><span "
           "style=\"text-decoration: underline; color:#039be5;\">Learn More</span></a>"));

    timeout_timer->setSingleShot(true);
    connect(timeout_timer.get(), &QTimer::timeout, this, [this]() { SetPollingResult({}, true); });

    connect(poll_timer.get(), &QTimer::timeout, this, [this]() {
        Common::ParamPackage params;
        for (auto& poller : device_pollers) {
            params = poller->GetNextInput();
            // We want all the input systems to be in a "polling" state, but we only care about the
            // input from SDL.
            if (params.Has("engine") && params.Get("engine", "") == "sdl") {
                SetPollingResult(params, false);
                return;
            }
        }
    });

    SetConfiguration();
    UpdateUiDisplay();
    ConnectEvents();
}

ConfigureMotionTouch::~ConfigureMotionTouch() = default;

void ConfigureMotionTouch::SetConfiguration() {
    const Common::ParamPackage motion_param(Settings::values.current_input_profile.motion_device);
    const Common::ParamPackage touch_param(Settings::values.current_input_profile.touch_device);
    const std::string motion_engine = motion_param.Get("engine", "motion_emu");
    const std::string touch_engine = touch_param.Get("engine", "emu_window");

    ui->motion_provider->setCurrentIndex(
        ui->motion_provider->findData(QString::fromStdString(motion_engine)));
    ui->touch_provider->setCurrentIndex(
        ui->touch_provider->findData(QString::fromStdString(touch_engine)));
    ui->touch_from_button_checkbox->setChecked(
        Settings::values.current_input_profile.use_touch_from_button);
    touch_from_button_maps = Settings::values.touch_from_button_maps;
    for (const auto& touch_map : touch_from_button_maps) {
        ui->touch_from_button_map->addItem(QString::fromStdString(touch_map.name));
    }
    ui->touch_from_button_map->setCurrentIndex(
        Settings::values.current_input_profile.touch_from_button_map_index);
    ui->motion_sensitivity->setValue(motion_param.Get("sensitivity", 0.01f));

    guid = motion_param.Get("guid", "0");
    port = motion_param.Get("port", 0);

    min_x = touch_param.Get("min_x", 100);
    min_y = touch_param.Get("min_y", 50);
    max_x = touch_param.Get("max_x", 1800);
    max_y = touch_param.Get("max_y", 850);

    ui->udp_server->setText(
        QString::fromStdString(Settings::values.current_input_profile.udp_input_address));
    ui->udp_port->setText(QString::number(Settings::values.current_input_profile.udp_input_port));
    ui->udp_pad_index->setCurrentIndex(Settings::values.current_input_profile.udp_pad_index);
}

void ConfigureMotionTouch::UpdateUiDisplay() {
    const std::string motion_engine = ui->motion_provider->currentData().toString().toStdString();
    const std::string touch_engine = ui->touch_provider->currentData().toString().toStdString();

    if (motion_engine == "motion_emu") {
        ui->motion_sensitivity_label->setVisible(true);
        ui->motion_sensitivity->setVisible(true);
    } else {
        ui->motion_sensitivity_label->setVisible(false);
        ui->motion_sensitivity->setVisible(false);
    }

    if (motion_engine == "sdl") {
        ui->motion_controller_label->setVisible(true);
        ui->motion_controller_button->setVisible(true);
    } else {
        ui->motion_controller_label->setVisible(false);
        ui->motion_controller_button->setVisible(false);
    }

    if (touch_engine == "cemuhookudp") {
        ui->touch_calibration->setVisible(true);
        ui->touch_calibration_config->setVisible(true);
        ui->touch_calibration_label->setVisible(true);
        ui->touch_calibration->setText(
            QStringLiteral("(%1, %2) - (%3, %4)").arg(min_x).arg(min_y).arg(max_x).arg(max_y));
    } else {
        ui->touch_calibration->setVisible(false);
        ui->touch_calibration_config->setVisible(false);
        ui->touch_calibration_label->setVisible(false);
    }

    if (motion_engine == "cemuhookudp" || touch_engine == "cemuhookudp") {
        ui->udp_config_group_box->setVisible(true);
    } else {
        ui->udp_config_group_box->setVisible(false);
    }
}

void ConfigureMotionTouch::ConnectEvents() {
    connect(ui->motion_provider, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this]([[maybe_unused]] int index) { UpdateUiDisplay(); });
    connect(ui->touch_provider, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this]([[maybe_unused]] int index) { UpdateUiDisplay(); });
    connect(ui->motion_controller_button, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::information(this, tr("Information"),
                                     tr("After pressing OK, press a button on the controller whose "
                                        "motion you want to track."),
                                     QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {
            ui->motion_controller_button->setText(tr("[press button]"));
            ui->motion_controller_button->setFocus();

            input_setter = [this](const Common::ParamPackage& params) {
                guid = params.Get("guid", "0");
                port = params.Get("port", 0);
            };

            device_pollers =
                InputCommon::Polling::GetPollers(InputCommon::Polling::DeviceType::Button);

            for (auto& poller : device_pollers) {
                poller->Start();
            }

            timeout_timer->start(5000); // Cancel after 5 seconds
            poll_timer->start(200);     // Check for new inputs every 200ms
        }
    });
    connect(ui->udp_test, &QPushButton::clicked, this, &ConfigureMotionTouch::OnCemuhookUDPTest);
    connect(ui->touch_calibration_config, &QPushButton::clicked, this,
            &ConfigureMotionTouch::OnConfigureTouchCalibration);
    connect(ui->touch_from_button_config_btn, &QPushButton::clicked, this,
            &ConfigureMotionTouch::OnConfigureTouchFromButton);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &ConfigureMotionTouch::ApplyConfiguration);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, [this] {
        if (CanCloseDialog()) {
            reject();
        }
    });
}

void ConfigureMotionTouch::SetPollingResult(const Common::ParamPackage& params, bool abort) {
    timeout_timer->stop();
    poll_timer->stop();
    for (auto& poller : device_pollers) {
        poller->Stop();
    }

    if (!abort && input_setter) {
        (*input_setter)(params);
    }

    ui->motion_controller_button->setText(tr("Configure"));
    input_setter.reset();
}

void ConfigureMotionTouch::OnCemuhookUDPTest() {
    ui->udp_test->setEnabled(false);
    ui->udp_test->setText(tr("Testing"));
    udp_test_in_progress = true;
    InputCommon::CemuhookUDP::TestCommunication(
        ui->udp_server->text().toStdString(), static_cast<u16>(ui->udp_port->text().toInt()),
        static_cast<u8>(ui->udp_pad_index->currentIndex()), 24872,
        [this] {
            LOG_INFO(Frontend, "UDP input test success");
            QMetaObject::invokeMethod(this, "ShowUDPTestResult", Q_ARG(bool, true));
        },
        [this] {
            LOG_ERROR(Frontend, "UDP input test failed");
            QMetaObject::invokeMethod(this, "ShowUDPTestResult", Q_ARG(bool, false));
        });
}

void ConfigureMotionTouch::OnConfigureTouchCalibration() {
    ui->touch_calibration_config->setEnabled(false);
    ui->touch_calibration_config->setText(tr("Configuring"));
    CalibrationConfigurationDialog dialog(
        this, ui->udp_server->text().toStdString(), static_cast<u16>(ui->udp_port->text().toUInt()),
        static_cast<u8>(ui->udp_pad_index->currentIndex()), 24872);
    dialog.exec();
    if (dialog.completed) {
        min_x = dialog.min_x;
        min_y = dialog.min_y;
        max_x = dialog.max_x;
        max_y = dialog.max_y;
        LOG_INFO(Frontend,
                 "UDP touchpad calibration config success: min_x={}, min_y={}, max_x={}, max_y={}",
                 min_x, min_y, max_x, max_y);
        UpdateUiDisplay();
    } else {
        LOG_ERROR(Frontend, "UDP touchpad calibration config failed");
    }
    ui->touch_calibration_config->setEnabled(true);
    ui->touch_calibration_config->setText(tr("Configure"));
}

void ConfigureMotionTouch::closeEvent(QCloseEvent* event) {
    if (CanCloseDialog()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void ConfigureMotionTouch::ShowUDPTestResult(bool result) {
    udp_test_in_progress = false;
    if (result) {
        QMessageBox::information(this, tr("Test Successful"),
                                 tr("Successfully received data from the server."));
    } else {
        QMessageBox::warning(this, tr("Test Failed"),
                             tr("Could not receive valid data from the server.<br>Please verify "
                                "that the server is set up correctly and "
                                "the address and port are correct."));
    }
    ui->udp_test->setEnabled(true);
    ui->udp_test->setText(tr("Test"));
}

void ConfigureMotionTouch::OnConfigureTouchFromButton() {
    ConfigureTouchFromButton dialog{this, touch_from_button_maps,
                                    ui->touch_from_button_map->currentIndex()};
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    touch_from_button_maps = dialog.GetMaps();

    while (ui->touch_from_button_map->count() > 0) {
        ui->touch_from_button_map->removeItem(0);
    }
    for (const auto& touch_map : touch_from_button_maps) {
        ui->touch_from_button_map->addItem(QString::fromStdString(touch_map.name));
    }
    ui->touch_from_button_map->setCurrentIndex(dialog.GetSelectedIndex());
}

bool ConfigureMotionTouch::CanCloseDialog() {
    if (udp_test_in_progress) {
        QMessageBox::warning(this, tr("Citra"),
                             tr("UDP Test or calibration configuration is in progress.<br>Please "
                                "wait for them to finish."));
        return false;
    }
    return true;
}

void ConfigureMotionTouch::ApplyConfiguration() {
    if (!CanCloseDialog()) {
        return;
    }

    std::string motion_engine = ui->motion_provider->currentData().toString().toStdString();
    std::string touch_engine = ui->touch_provider->currentData().toString().toStdString();

    Common::ParamPackage motion_param{};
    motion_param.Set("engine", motion_engine);
    if (motion_engine == "motion_emu") {
        motion_param.Set("sensitivity", static_cast<float>(ui->motion_sensitivity->value()));
    } else if (motion_engine == "sdl") {
        motion_param.Set("guid", guid);
        motion_param.Set("port", port);
    }

    Common::ParamPackage touch_param{};
    touch_param.Set("engine", touch_engine);
    if (touch_engine == "cemuhookudp") {
        touch_param.Set("min_x", min_x);
        touch_param.Set("min_y", min_y);
        touch_param.Set("max_x", max_x);
        touch_param.Set("max_y", max_y);
    }

    Settings::values.current_input_profile.motion_device = motion_param.Serialize();
    Settings::values.current_input_profile.touch_device = touch_param.Serialize();
    Settings::values.current_input_profile.use_touch_from_button =
        ui->touch_from_button_checkbox->isChecked();
    Settings::values.current_input_profile.touch_from_button_map_index =
        ui->touch_from_button_map->currentIndex();
    Settings::values.touch_from_button_maps = touch_from_button_maps;
    Settings::values.current_input_profile.udp_input_address = ui->udp_server->text().toStdString();
    Settings::values.current_input_profile.udp_input_port =
        static_cast<u16>(ui->udp_port->text().toInt());
    Settings::values.current_input_profile.udp_pad_index =
        static_cast<u8>(ui->udp_pad_index->currentIndex());
    Settings::SaveProfile(Settings::values.current_input_profile_index);
    InputCommon::ReloadInputDevices();

    accept();
}
