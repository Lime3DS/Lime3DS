// Copyright 2020-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <mutex>

#include "core/frontend/input.h"

namespace InputManager {

inline std::atomic<int> screen_rotation;

class NDKMotion;

class NDKMotionFactory final : public Input::Factory<Input::MotionDevice> {
public:
    /**
     * Creates a motion device that obtains data from device sensors
     */
    std::unique_ptr<Input::MotionDevice> Create(const Common::ParamPackage& params) override;

    void EnableSensors();
    void DisableSensors();

private:
    friend class NDKMotion;

    /// Called from ~NDKMotion() so the factory can never call into a device
    /// that has already been destroyed.
    void Unregister(NDKMotion* device);

    std::mutex device_mutex;
    NDKMotion* ndk_motion_device = nullptr;
};
} // namespace InputManager
