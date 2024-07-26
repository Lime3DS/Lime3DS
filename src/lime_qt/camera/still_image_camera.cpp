// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QThread>
#include "common/logging/log.h"
#include "lime_qt/camera/still_image_camera.h"

namespace Camera {

StillImageCamera::StillImageCamera(QImage image_, const Service::CAM::Flip& flip)
    : QtCameraInterface(flip), image(std::move(image_)) {}

StillImageCamera::~StillImageCamera() {
    StillImageCameraFactory::last_path.clear();
}

void StillImageCamera::StartCapture() {}

void StillImageCamera::StopCapture() {}

QImage StillImageCamera::QtReceiveFrame() {
    return image;
}

bool StillImageCamera::IsPreviewAvailable() {
    return !image.isNull();
}

std::string StillImageCameraFactory::last_path;

const std::string StillImageCameraFactory::GetFilePath() const {
    if (!last_path.empty()) {
        return last_path;
    }
    QList<QByteArray> types = QImageReader::supportedImageFormats();
    QStringList temp_filters;
    for (QByteArray type : types) {
        temp_filters << QStringLiteral("*.%1").arg(QString::fromUtf8(type));
    }

    QString filter =
        QObject::tr("Supported image files (%1)").arg(temp_filters.join(QLatin1Char{' '}));
    last_path =
        QFileDialog::getOpenFileName(nullptr, QObject::tr("Open File"), QStringLiteral("."), filter)
            .toStdString();
    return last_path;
}

std::unique_ptr<CameraInterface> StillImageCameraFactory::Create(const std::string& config,
                                                                 const Service::CAM::Flip& flip) {
    std::string real_config = config;
    if (config.empty()) {
        // call GetFilePath() in UI thread (note: StillImageCameraFactory itself is initialized in
        // UI thread, so we can just pass in "this" here)
        if (thread() == QThread::currentThread()) {
            real_config = GetFilePath();
        } else {
            QMetaObject::invokeMethod(this, "GetFilePath", Qt::BlockingQueuedConnection,
                                      Q_RETURN_ARG(std::string, real_config));
        }
    }
    QImage image(QString::fromStdString(real_config));
    if (image.isNull()) {
        LOG_ERROR(Service_CAM, "Couldn't load image \"{}\"", real_config.c_str());
    }
    return std::make_unique<StillImageCamera>(image, flip);
}

} // namespace Camera
