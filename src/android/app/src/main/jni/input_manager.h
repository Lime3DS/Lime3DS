// Copyright 2013 Dolphin Emulator Project / 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "core/frontend/input.h"
#include "jni/ndk_motion.h"

namespace InputManager {

enum ButtonType {
    // 3DS Controls
    N3DS_BUTTON_A = 700,
    N3DS_BUTTON_B = 701,
    N3DS_BUTTON_X = 702,
    N3DS_BUTTON_Y = 703,
    N3DS_BUTTON_START = 704,
    N3DS_BUTTON_SELECT = 705,
    N3DS_BUTTON_HOME = 706,
    N3DS_BUTTON_ZL = 707,
    N3DS_BUTTON_ZR = 708,
    N3DS_DPAD_UP = 709,
    N3DS_DPAD_DOWN = 710,
    N3DS_DPAD_LEFT = 711,
    N3DS_DPAD_RIGHT = 712,
    N3DS_CIRCLEPAD = 713,
    N3DS_CIRCLEPAD_UP = 714,
    N3DS_CIRCLEPAD_DOWN = 715,
    N3DS_CIRCLEPAD_LEFT = 716,
    N3DS_CIRCLEPAD_RIGHT = 717,
    N3DS_STICK_C = 718,
    N3DS_STICK_C_UP = 719,
    N3DS_STICK_C_DOWN = 720,
    N3DS_STICK_C_LEFT = 771,
    N3DS_STICK_C_RIGHT = 772,
    N3DS_TRIGGER_L = 773,
    N3DS_TRIGGER_R = 774,
    N3DS_BUTTON_DEBUG = 781,
    N3DS_BUTTON_GPIO14 = 782
};

class ButtonList;
class AnalogButtonList;
class AnalogList;

/**
 * A button device factory representing a gamepad. It receives input events and forward them
 * to all button devices it created.
 */
class ButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    ButtonFactory();

    /**
     * Creates a button device from a gamepad button
     * @param params contains parameters for creating the device:
     *     - "code": the code of the key to bind with the button
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override;

    /**
     * Sets the status of all buttons bound with the key to pressed
     * @param key_code the code of the key to press
     * @return whether the key event is consumed or not
     */
    bool PressKey(int button_id);

    /**
     * Sets the status of all buttons bound with the key to released
     * @param key_code the code of the key to release
     * @return whether the key event is consumed or not
     */
    bool ReleaseKey(int button_id);

    /**
     * Sets the status of all buttons bound with the key to released
     * @param axis_id the code of the axis
     * @param axis_val the value of the axis
     * @return whether the key event is consumed or not
     */
    bool AnalogButtonEvent(int axis_id, float axis_val);

    void ReleaseAllKeys();

private:
    std::shared_ptr<ButtonList> button_list;
    std::shared_ptr<AnalogButtonList> analog_button_list;
};

/**
 * An analog device factory representing a gamepad(virtual or physical). It receives input events
 * and forward them to all analog devices it created.
 */
class AnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    AnalogFactory();

    /**
     * Creates an analog device from the gamepad joystick
     * @param params contains parameters for creating the device:
     *     - "code": the code of the key to bind with the button
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override;

    /**
     * Sets the status of all buttons bound with the key to pressed
     * @param key_code the code of the analog stick
     * @param x the x-axis value of the analog stick
     * @param y the y-axis value of the analog stick
     */
    bool MoveJoystick(int analog_id, float x, float y);

private:
    std::shared_ptr<AnalogList> analog_list;
};

/// Initializes and registers all built-in input device factories.
void Init();

/// Deregisters all built-in input device factories and shuts them down.
void Shutdown();

/// Gets the gamepad button device factory.
ButtonFactory* ButtonHandler();

/// Gets the gamepad analog device factory.
AnalogFactory* AnalogHandler();

/// Gets the NDK Motion device factory.
NDKMotionFactory* NDKMotionHandler();

std::string GenerateButtonParamPackage(int type);

std::string GenerateAnalogButtonParamPackage(int axis, float axis_val);

std::string GenerateAnalogParamPackage(int type);
} // namespace InputManager
