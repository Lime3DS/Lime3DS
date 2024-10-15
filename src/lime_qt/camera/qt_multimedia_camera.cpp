// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCamera>
#include <QImageReader>
#include <QMediaDevices>
#include <QThread>
#include "lime_qt/camera/qt_multimedia_camera.h"
#include "lime_qt/lime_qt.h"

#if defined(__APPLE__)
#include "common/apple_authorization.h"
#endif

namespace Camera {

std::shared_ptr<QtMultimediaCameraHandler> QtMultimediaCameraHandlerFactory::Create(
    const std::string& camera_name) {
    if (thread() == QThread::currentThread()) {
        std::shared_ptr<QtMultimediaCameraHandler> handler;
        if (!handlers.contains(camera_name) || !(handler = handlers[camera_name].lock())) {
            LOG_INFO(Service_CAM, "Creating new handler for camera '{}'", camera_name);
            handler = std::make_shared<QtMultimediaCameraHandler>(camera_name);
            handlers[camera_name] = handler;
        } else {
            LOG_INFO(Service_CAM, "Reusing existing handler for camera '{}'", camera_name);
        }
        return handler;
    } else {
        std::shared_ptr<QtMultimediaCameraHandler> handler;
        QMetaObject::invokeMethod(this, "Create", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(std::shared_ptr<QtMultimediaCameraHandler>, handler),
                                  Q_ARG(std::string, camera_name));
        return handler;
    }
}

void QtMultimediaCameraHandlerFactory::PauseCameras() {
    LOG_INFO(Service_CAM, "Pausing all cameras");
    for (auto& handler_pair : handlers) {
        auto handler = handler_pair.second.lock();
        if (handler && handler->IsActive()) {
            handler->PauseCapture();
        }
    }
}

void QtMultimediaCameraHandlerFactory::ResumeCameras() {
    LOG_INFO(Service_CAM, "Resuming all cameras");
    for (auto& handler_pair : handlers) {
        auto handler = handler_pair.second.lock();
        if (handler && handler->IsPaused()) {
            handler->StartCapture();
        }
    }
}

QtMultimediaCameraHandler::QtMultimediaCameraHandler(const std::string& camera_name) {
    auto cameras = QMediaDevices::videoInputs();
    auto requested_camera =
        std::find_if(cameras.begin(), cameras.end(), [camera_name](QCameraDevice& camera_info) {
            return camera_info.description().toStdString() == camera_name;
        });
    if (requested_camera != cameras.end()) {
        camera = std::make_unique<QCamera>(*requested_camera);
    } else {
        camera = std::make_unique<QCamera>();
    }
    camera_surface = std::make_unique<QVideoSink>();
    capture_session.setVideoSink(camera_surface.get());
    capture_session.setCamera(camera.get());
}

QtMultimediaCameraHandler::~QtMultimediaCameraHandler() {
    StopCapture();
}

void QtMultimediaCameraHandler::StartCapture() {
    if (!camera->isActive()) {
#if defined(__APPLE__)
        if (!AppleAuthorization::CheckAuthorizationForCamera()) {
            LOG_ERROR(Service_CAM, "Unable to start camera due to lack of authorization");
            return;
        }
#endif
        camera->start();
    }
    paused = false;
}

void QtMultimediaCameraHandler::StopCapture() {
    if (camera->isActive()) {
        camera->stop();
    }
    paused = false;
}

} // namespace Camera
