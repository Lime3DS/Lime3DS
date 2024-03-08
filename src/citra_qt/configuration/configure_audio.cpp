// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QtGlobal>
#include "audio_core/input_details.h"
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_audio.h"
#include "common/settings.h"
#include "ui_configure_audio.h"

#if defined(__APPLE__)
#include "common/apple_authorization.h"
#endif

ConfigureAudio::ConfigureAudio(bool is_powered_on, QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureAudio>()) {
    ui->setupUi(this);

    ui->output_type_combo_box->clear();
    ui->output_type_combo_box->addItem(tr("Auto"), QVariant::fromValue(AudioCore::SinkType::Auto));
    for (const auto& sink : AudioCore::ListSinks()) {
        ui->output_type_combo_box->addItem(QString::fromUtf8(sink.name),
                                           QVariant::fromValue(sink.type));
    }

    ui->emulation_combo_box->setEnabled(!is_powered_on);

    connect(ui->volume_slider, &QSlider::valueChanged, this,
            &ConfigureAudio::SetVolumeIndicatorText);

    ui->input_type_combo_box->clear();
    ui->input_type_combo_box->addItem(tr("Auto"), QVariant::fromValue(AudioCore::InputType::Auto));
    for (const auto& input : AudioCore::ListInputs()) {
        ui->input_type_combo_box->addItem(QString::fromUtf8(input.name),
                                          QVariant::fromValue(input.type));
    }

    ui->volume_label->setVisible(Settings::IsConfiguringGlobal());
    ui->volume_combo_box->setVisible(!Settings::IsConfiguringGlobal());

    SetupPerGameUI();
    SetConfiguration();

    connect(ui->output_type_combo_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureAudio::UpdateAudioOutputDevices);
    connect(ui->input_type_combo_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureAudio::UpdateAudioInputDevices);
}

ConfigureAudio::~ConfigureAudio() {}

void ConfigureAudio::SetConfiguration() {
    SetOutputTypeFromSinkType();
    SetInputTypeFromInputType();

    // The device list cannot be pre-populated (nor listed) until the output sink is known.
    UpdateAudioOutputDevices(ui->output_type_combo_box->currentIndex());
    UpdateAudioInputDevices(ui->input_type_combo_box->currentIndex());
    SetOutputDeviceFromDeviceID();
    SetInputDeviceFromDeviceID();

    ui->toggle_audio_stretching->setChecked(Settings::values.enable_audio_stretching.GetValue());

    const s32 volume =
        static_cast<s32>(Settings::values.volume.GetValue() * ui->volume_slider->maximum());
    ui->volume_slider->setValue(volume);
    SetVolumeIndicatorText(ui->volume_slider->sliderPosition());

    if (!Settings::IsConfiguringGlobal()) {
        if (Settings::values.volume.UsingGlobal()) {
            ui->volume_combo_box->setCurrentIndex(0);
            ui->volume_slider->setEnabled(false);
        } else {
            ui->volume_combo_box->setCurrentIndex(1);
            ui->volume_slider->setEnabled(true);
        }
        ConfigurationShared::SetHighlight(ui->volume_layout,
                                          !Settings::values.volume.UsingGlobal());
        ConfigurationShared::SetHighlight(ui->widget_emulation,
                                          !Settings::values.audio_emulation.UsingGlobal());
        ConfigurationShared::SetPerGameSetting(ui->emulation_combo_box,
                                               &Settings::values.audio_emulation);
    } else {
        s32 selection = static_cast<s32>(Settings::values.audio_emulation.GetValue());
        ui->emulation_combo_box->setCurrentIndex(selection);
    }
}

void ConfigureAudio::SetOutputTypeFromSinkType() {
    int new_index = -1;

    for (int index = 0; index < ui->output_type_combo_box->count(); index++) {
        const auto sink_type =
            static_cast<AudioCore::SinkType>(ui->output_type_combo_box->itemData(index).toUInt());
        if (Settings::values.output_type.GetValue() == sink_type) {
            new_index = index;
            break;
        }
    }

    ui->output_type_combo_box->setCurrentIndex(new_index);
}

void ConfigureAudio::SetOutputDeviceFromDeviceID() {
    int new_device_index = -1;

    const QString device_id = QString::fromStdString(Settings::values.output_device.GetValue());
    for (int index = 0; index < ui->output_device_combo_box->count(); index++) {
        if (ui->output_device_combo_box->itemText(index) == device_id) {
            new_device_index = index;
            break;
        }
    }

    ui->output_device_combo_box->setCurrentIndex(new_device_index);
}

void ConfigureAudio::SetInputTypeFromInputType() {
    int new_index = -1;

    for (int index = 0; index < ui->input_type_combo_box->count(); index++) {
        const auto input_type =
            static_cast<AudioCore::InputType>(ui->input_type_combo_box->itemData(index).toUInt());
        if (Settings::values.input_type.GetValue() == input_type) {
            new_index = index;
            break;
        }
    }

    ui->input_type_combo_box->setCurrentIndex(new_index);
}

void ConfigureAudio::SetInputDeviceFromDeviceID() {
    int new_device_index = -1;

    const QString device_id = QString::fromStdString(Settings::values.input_device.GetValue());
    for (int index = 0; index < ui->input_device_combo_box->count(); index++) {
        if (ui->input_device_combo_box->itemText(index) == device_id) {
            new_device_index = index;
            break;
        }
    }

    ui->input_device_combo_box->setCurrentIndex(new_device_index);
}

void ConfigureAudio::SetVolumeIndicatorText(int percentage) {
    ui->volume_indicator->setText(tr("%1%", "Volume percentage (e.g. 50%)").arg(percentage));
}

void ConfigureAudio::ApplyConfiguration() {
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.enable_audio_stretching,
                                             ui->toggle_audio_stretching, audio_stretching);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.audio_emulation,
                                             ui->emulation_combo_box);
    ConfigurationShared::ApplyPerGameSetting(
        &Settings::values.volume, ui->volume_combo_box, [this](s32) {
            return static_cast<float>(ui->volume_slider->value()) / ui->volume_slider->maximum();
        });

    if (Settings::IsConfiguringGlobal()) {
        Settings::values.output_type =
            static_cast<AudioCore::SinkType>(ui->output_type_combo_box->currentData().toUInt());
        Settings::values.output_device = ui->output_device_combo_box->currentText().toStdString();
        Settings::values.input_type =
            static_cast<AudioCore::InputType>(ui->input_type_combo_box->currentData().toUInt());
        Settings::values.input_device = ui->input_device_combo_box->currentText().toStdString();
    }
}

void ConfigureAudio::UpdateAudioOutputDevices(int sink_index) {
    auto sink_type =
        static_cast<AudioCore::SinkType>(ui->output_type_combo_box->itemData(sink_index).toUInt());
    auto& sink_details = AudioCore::GetSinkDetails(sink_type);

    ui->output_device_combo_box->clear();
    ui->output_device_combo_box->addItem(QString::fromUtf8(AudioCore::auto_device_name));

    for (const auto& device : sink_details.list_devices()) {
        ui->output_device_combo_box->addItem(QString::fromStdString(device));
    }
}

void ConfigureAudio::UpdateAudioInputDevices(int input_index) {
    auto input_type =
        static_cast<AudioCore::InputType>(ui->input_type_combo_box->itemData(input_index).toUInt());
    auto& input_details = AudioCore::GetInputDetails(input_type);

#if defined(__APPLE__)
    if (input_details.real) {
        AppleAuthorization::CheckAuthorizationForMicrophone();
    }
#endif

    ui->input_device_combo_box->clear();
    ui->input_device_combo_box->addItem(QString::fromUtf8(AudioCore::auto_device_name));

    for (const auto& device : input_details.list_devices()) {
        ui->input_device_combo_box->addItem(QString::fromStdString(device));
    }
}

void ConfigureAudio::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureAudio::SetupPerGameUI() {
    if (Settings::IsConfiguringGlobal()) {
        ui->volume_slider->setEnabled(Settings::values.volume.UsingGlobal());
        return;
    }

    ui->output_type_combo_box->setVisible(false);
    ui->output_type_label->setVisible(false);
    ui->output_device_combo_box->setVisible(false);
    ui->output_device_label->setVisible(false);
    ui->input_type_label->setVisible(false);
    ui->input_type_combo_box->setVisible(false);
    ui->input_device_label->setVisible(false);
    ui->input_device_combo_box->setVisible(false);
    ui->input_layout->setVisible(false);

    connect(ui->volume_combo_box, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        ui->volume_slider->setEnabled(index == 1);
        ConfigurationShared::SetHighlight(ui->volume_layout, index == 1);
    });

    ConfigurationShared::SetColoredComboBox(
        ui->emulation_combo_box, ui->widget_emulation,
        static_cast<u32>(Settings::values.audio_emulation.GetValue(true)));

    ConfigurationShared::SetColoredTristate(
        ui->toggle_audio_stretching, Settings::values.enable_audio_stretching, audio_stretching);
}
