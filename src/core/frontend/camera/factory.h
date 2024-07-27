// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "core/frontend/camera/interface.h"

namespace Camera {

class CameraFactory {
public:
    virtual ~CameraFactory();

    /**
     * Creates a camera object based on the configuration string.
     * @param config Configuration string to create the camera. The implementation can decide the
     *               meaning of this string.
     * @param flip The image flip to apply
     * @returns a unique_ptr to the created camera object.
     */
    virtual std::unique_ptr<CameraInterface> Create(const std::string& config,
                                                    const Service::CAM::Flip& flip) = 0;

    /**
     * Creates a camera object for preview based on the configuration string.
     * @param config Configuration string to create the camera. The implementation can decide the
     *               meaning of this string.
     * @param flip The image flip to apply
     * @returns a unique_ptr to the created camera object.
     * Note: The default implementation for this is to call Create(). Derived classes may have other
     *       Implementations. For example, A dialog may be used instead of LOG_ERROR when error
     * occurs.
     */
    virtual std::unique_ptr<CameraInterface> CreatePreview(const std::string& config, int width,
                                                           int height,
                                                           const Service::CAM::Flip& flip) {
        return Create(config, flip);
    }
};

/**
 * Registers an external camera factory.
 * @param name Identifier of the camera factory.
 * @param factory Camera factory to register.
 */
void RegisterFactory(const std::string& name, std::unique_ptr<CameraFactory> factory);

/**
 * Creates a camera from the factory.
 * @param name Identifier of the camera factory.
 * @param config Configuration string to create the camera. The meaning of this string is
 *     defined by the factory.
 */
std::unique_ptr<CameraInterface> CreateCamera(const std::string& name, const std::string& config,
                                              const Service::CAM::Flip& flip);

/**
 * Creates a camera from the factory for previewing.
 * @param name Identifier of the camera factory.
 * @param config Configuration string to create the camera. The meaning of this string is
 *     defined by the factory.
 */
std::unique_ptr<CameraInterface> CreateCameraPreview(const std::string& name,
                                                     const std::string& config, int width,
                                                     int height, const Service::CAM::Flip& flip);

} // namespace Camera
