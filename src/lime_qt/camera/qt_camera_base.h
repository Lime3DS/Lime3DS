// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "core/frontend/camera/factory.h"

namespace Camera {

// Base class for camera interfaces of lime_qt
class QtCameraInterface : public CameraInterface {
public:
    QtCameraInterface(const Service::CAM::Flip& flip);
    void SetResolution(const Service::CAM::Resolution&) override;
    void SetFlip(Service::CAM::Flip) override;
    void SetEffect(Service::CAM::Effect) override;
    void SetFormat(Service::CAM::OutputFormat) override;
    std::vector<u16> ReceiveFrame() override;
    virtual QImage QtReceiveFrame() = 0;

private:
    int width, height;
    bool output_rgb;
    bool flip_horizontal, flip_vertical;
    bool basic_flip_horizontal, basic_flip_vertical;
};

// Base class for camera factories of lime_qt
class QtCameraFactory : public CameraFactory {
    std::unique_ptr<CameraInterface> CreatePreview(const std::string& config, int width, int height,
                                                   const Service::CAM::Flip& flip) override;
};

} // namespace Camera
