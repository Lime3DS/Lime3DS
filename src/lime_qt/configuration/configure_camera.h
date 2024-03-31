// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>

namespace Camera {
class CameraInterface;
}

namespace Ui {
class ConfigureCamera;
}

class ConfigureCamera : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureCamera(QWidget* parent = nullptr);
    ~ConfigureCamera() override;

    void ApplyConfiguration();
    void RetranslateUI();

    void timerEvent(QTimerEvent*) override;

public slots:
    /// RecordConfig() and UpdateUiDisplay()
    void SetConfiguration();
    void OnToolButtonClicked();

private:
    enum class CameraPosition { RearRight, Front, RearLeft, RearBoth, Null };
    static const std::array<std::string, 3> Implementations;
    /// Record the current configuration
    void RecordConfig();
    /// Updates camera mode
    void UpdateCameraMode();
    /// Updates image source
    void UpdateImageSourceUI();
    void StartPreviewing();
    void StopPreviewing();
    void ConnectEvents();
    CameraPosition GetCameraSelection();
    int GetSelectedCameraIndex();

private:
    std::unique_ptr<Ui::ConfigureCamera> ui;
    std::array<std::string, 3> camera_name;
    std::array<std::string, 3> camera_config;
    std::array<int, 3> camera_flip;
    int timer_id = 0;
    int preview_width = 0;
    int preview_height = 0;
    CameraPosition current_selected = CameraPosition::Front;
    bool is_previewing = false;
    std::unique_ptr<Camera::CameraInterface> previewing_camera;
};
