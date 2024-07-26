// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/frontend/input.h"

namespace InputCommon {

class KeyButtonList;

/**
 * A button device factory representing a keyboard. It receives keyboard events and forward them
 * to all button devices it created.
 */
class Keyboard final : public Input::Factory<Input::ButtonDevice> {
public:
    Keyboard();

    /**
     * Creates a button device from a keyboard key
     * @param params contains parameters for creating the device:
     *     - "code": the code of the key to bind with the button
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override;

    /**
     * Sets the status of all buttons bound with the key to pressed
     * @param key_code the code of the key to press
     */
    void PressKey(int key_code);

    /**
     * Sets the status of all buttons bound with the key to released
     * @param key_code the code of the key to release
     */
    void ReleaseKey(int key_code);

    void ReleaseAllKeys();

private:
    std::shared_ptr<KeyButtonList> key_button_list;
};

} // namespace InputCommon
