// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <QImage>
#include "core/frontend/camera/interface.h"
#include "lime_qt/camera/camera_util.h"
#include "lime_qt/camera/qt_camera_base.h"

namespace Camera {

class StillImageCamera final : public QtCameraInterface {
public:
    StillImageCamera(QImage image, const Service::CAM::Flip& flip);
    ~StillImageCamera();
    void StartCapture() override;
    void StopCapture() override;
    void SetFrameRate(Service::CAM::FrameRate frame_rate) override {}
    QImage QtReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    QImage image;
};

class StillImageCameraFactory final : public QObject, public QtCameraFactory {
    Q_OBJECT

public:
    std::unique_ptr<CameraInterface> Create(const std::string& config,
                                            const Service::CAM::Flip& flip) override;

    Q_INVOKABLE const std::string GetFilePath() const;

private:
    /// Record the path chosen to avoid multiple prompt problem
    static std::string last_path;

    friend class StillImageCamera;
};

} // namespace Camera
