// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
#include "common/logging/log.h"
#include "core/frontend/camera/blank_camera.h"
#include "core/frontend/camera/factory.h"

namespace Camera {

static std::unordered_map<std::string, std::unique_ptr<CameraFactory>> factories;

CameraFactory::~CameraFactory() = default;

void RegisterFactory(const std::string& name, std::unique_ptr<CameraFactory> factory) {
    factories[name] = std::move(factory);
}

std::unique_ptr<CameraInterface> CreateCamera(const std::string& name, const std::string& config,
                                              const Service::CAM::Flip& flip) {
    auto pair = factories.find(name);
    if (pair != factories.end()) {
        return pair->second->Create(config, flip);
    }

    if (name != "blank") {
        LOG_ERROR(Service_CAM, "Unknown camera {}", name);
    }
    return std::make_unique<BlankCamera>();
}

std::unique_ptr<CameraInterface> CreateCameraPreview(const std::string& name,
                                                     const std::string& config, int width,
                                                     int height, const Service::CAM::Flip& flip) {
    auto pair = factories.find(name);
    if (pair != factories.end()) {
        return pair->second->CreatePreview(config, width, height, flip);
    }

    if (name != "blank") {
        LOG_ERROR(Service_CAM, "Unknown camera {}", name);
    }
    return std::make_unique<BlankCamera>();
}

} // namespace Camera
