// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/frontend/input.h"

namespace InputCommon {

/**
 * An analog device factory that takes direction button devices and combines them into a analog
 * device.
 */
class AnalogFromButton final : public Input::Factory<Input::AnalogDevice> {
public:
    /**
     * Creates an analog device from direction button devices
     * @param params contains parameters for creating the device:
     *     - "up": a serialized ParamPackage for creating a button device for up direction
     *     - "down": a serialized ParamPackage for creating a button device for down direction
     *     - "left": a serialized ParamPackage for creating a button device for left direction
     *     - "right": a serialized ParamPackage  for creating a button device for right direction
     *     - "modifier": a serialized ParamPackage for creating a button device as the modifier
     *     - "modifier_scale": a float for the multiplier the modifier gives to the position
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override;
};

} // namespace InputCommon
