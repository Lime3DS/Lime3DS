// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "core/frontend/input.h"
#include "input_common/main.h"

union SDL_Event;

namespace Common {
class ParamPackage;
} // namespace Common

namespace InputCommon::Polling {
class DevicePoller;
enum class DeviceType;
} // namespace InputCommon::Polling

namespace InputCommon::SDL {

class State {
public:
    using Pollers = std::vector<std::unique_ptr<Polling::DevicePoller>>;

    /// Unregisters SDL device factories and shut them down.
    virtual ~State() = default;

    virtual Pollers GetPollers(Polling::DeviceType type) = 0;

    virtual void GetSystemBatteryState(float& percentage, bool& charging) = 0;
};

class NullState : public State {
public:
    Pollers GetPollers(Polling::DeviceType type) override {
        return {};
    }

    void GetSystemBatteryState(float& percentage, bool& charging) override {
        percentage = 1.0f;
        charging = true;
    }
};

std::unique_ptr<State> Init();

} // namespace InputCommon::SDL
