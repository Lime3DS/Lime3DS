// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureAudio;
}

namespace ConfigurationShared {
enum class CheckState;
}

class ConfigureAudio : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureAudio(bool is_powered_on, QWidget* parent = nullptr);
    ~ConfigureAudio() override;

    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();

private:
    void UpdateAudioOutputDevices(int sink_index);
    void UpdateAudioInputDevices(int index);

    void SetOutputTypeFromSinkType();
    void SetOutputDeviceFromDeviceID();
    void SetInputTypeFromInputType();
    void SetInputDeviceFromDeviceID();
    void SetVolumeIndicatorText(int percentage);

    void SetupPerGameUI();

    ConfigurationShared::CheckState audio_stretching;
    std::unique_ptr<Ui::ConfigureAudio> ui;
};
