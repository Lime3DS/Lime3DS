// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>
#include <camera/NdkCameraManager.h>
#include "common/common_types.h"
#include "core/frontend/camera/factory.h"
#include "core/frontend/camera/interface.h"
#include "core/hle/service/cam/cam.h"

namespace Camera::NDK {

struct CaptureSession;
class Factory;

class Interface : public CameraInterface {
public:
    Interface(Factory& factory, const std::string& id, const Service::CAM::Flip& flip);
    ~Interface() override;
    void StartCapture() override;
    void StopCapture() override;
    void SetResolution(const Service::CAM::Resolution& resolution) override;
    void SetFlip(Service::CAM::Flip flip) override;
    void SetEffect(Service::CAM::Effect effect) override {};
    void SetFormat(Service::CAM::OutputFormat format) override;
    void SetFrameRate(Service::CAM::FrameRate frame_rate) override {};
    std::vector<u16> ReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    Factory& factory;
    std::shared_ptr<CaptureSession> session;
    std::string id;

    Service::CAM::Resolution resolution;

    // Flipping parameters. mirror = horizontal, invert = vertical.
    bool base_mirror{};
    bool base_invert{};
    bool mirror{};
    bool invert{};

    Service::CAM::OutputFormat format;
};

// Placeholders to mean 'use any front/back camera'
constexpr std::string_view FrontCameraPlaceholder = "_front";
constexpr std::string_view BackCameraPlaceholder = "_back";

class Factory final : public CameraFactory {
public:
    explicit Factory();
    ~Factory() override;

    std::unique_ptr<CameraInterface> Create(const std::string& config,
                                            const Service::CAM::Flip& flip) override;

    // Request the reopening of all previously disconnected camera devices.
    // Called when the application is brought to foreground (i.e. we have priority with the camera)
    void ReloadCameraDevices();

private:
    // Avoid requesting for permission more than once on each call
    bool camera_permission_requested = false;
    bool camera_permission_granted = false;

    std::shared_ptr<CaptureSession> CreateCaptureSession(const std::string& id);

    // The session is cached, to avoid opening the same camera twice.
    // This is weak_ptr so that the session is destructed when all cameras are closed
    std::unordered_map<std::string, std::weak_ptr<CaptureSession>> opened_camera_map;

    struct ACameraManagerDeleter {
        void operator()(ACameraManager* manager) {
            ACameraManager_delete(manager);
        }
    };
    std::unique_ptr<ACameraManager, ACameraManagerDeleter> manager;

    friend class Interface;
};

// Device rotation. Updated in native.cpp.
inline int g_rotation = 0;

} // namespace Camera::NDK
