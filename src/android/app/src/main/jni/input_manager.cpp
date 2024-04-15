// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <list>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "input_common/main.h"
#include "input_common/sdl/sdl.h"
#include "jni/input_manager.h"
#include "jni/ndk_motion.h"

namespace InputManager {

static std::shared_ptr<ButtonFactory> button;
static std::shared_ptr<AnalogFactory> analog;
static std::shared_ptr<NDKMotionFactory> motion;

// Button Handler
class KeyButton final : public Input::ButtonDevice {
public:
    explicit KeyButton(std::shared_ptr<ButtonList> button_list_) : button_list(button_list_) {}

    ~KeyButton();

    bool GetStatus() const override {
        return status.load();
    }

    friend class ButtonList;

private:
    std::shared_ptr<ButtonList> button_list;
    std::atomic<bool> status{false};
};

struct KeyButtonPair {
    int button_id;
    KeyButton* key_button;
};

class ButtonList {
public:
    void AddButton(int button_id, KeyButton* key_button) {
        std::lock_guard<std::mutex> guard(mutex);
        list.push_back(KeyButtonPair{button_id, key_button});
    }

    void RemoveButton(const KeyButton* key_button) {
        std::lock_guard<std::mutex> guard(mutex);
        list.remove_if(
            [key_button](const KeyButtonPair& pair) { return pair.key_button == key_button; });
    }

    bool ChangeButtonStatus(int button_id, bool pressed) {
        std::lock_guard<std::mutex> guard(mutex);
        bool button_found = false;
        for (const KeyButtonPair& pair : list) {
            if (pair.button_id == button_id) {
                pair.key_button->status.store(pressed);
                button_found = true;
            }
        }
        // If we don't find the button don't consume the button press event
        return button_found;
    }

    void ChangeAllButtonStatus(bool pressed) {
        std::lock_guard<std::mutex> guard(mutex);
        for (const KeyButtonPair& pair : list) {
            pair.key_button->status.store(pressed);
        }
    }

private:
    std::mutex mutex;
    std::list<KeyButtonPair> list;
};

KeyButton::~KeyButton() {
    button_list->RemoveButton(this);
}

// Analog Button
class AnalogButton final : public Input::ButtonDevice {
public:
    explicit AnalogButton(std::shared_ptr<AnalogButtonList> button_list_, float threshold_,
                          bool trigger_if_greater_)
        : button_list(button_list_), threshold(threshold_),
          trigger_if_greater(trigger_if_greater_) {}

    ~AnalogButton();

    bool GetStatus() const override {
        if (trigger_if_greater)
            return axis_val.load() > threshold;
        return axis_val.load() < threshold;
    }

    friend class AnalogButtonList;

private:
    std::shared_ptr<AnalogButtonList> button_list;
    std::atomic<float> axis_val{0.0f};
    float threshold;
    bool trigger_if_greater;
};

struct AnalogButtonPair {
    int axis_id;
    AnalogButton* key_button;
};

class AnalogButtonList {
public:
    void AddAnalogButton(int button_id, AnalogButton* key_button) {
        std::lock_guard<std::mutex> guard(mutex);
        list.push_back(AnalogButtonPair{button_id, key_button});
    }

    void RemoveButton(const AnalogButton* key_button) {
        std::lock_guard<std::mutex> guard(mutex);
        list.remove_if(
            [key_button](const AnalogButtonPair& pair) { return pair.key_button == key_button; });
    }

    bool ChangeAxisValue(int axis_id, float axis) {
        std::lock_guard<std::mutex> guard(mutex);
        bool button_found = false;
        for (const AnalogButtonPair& pair : list) {
            if (pair.axis_id == axis_id) {
                pair.key_button->axis_val.store(axis);
                button_found = true;
            }
        }
        // If we don't find the button don't consume the button press event
        return button_found;
    }

private:
    std::mutex mutex;
    std::list<AnalogButtonPair> list;
};

AnalogButton::~AnalogButton() {
    button_list->RemoveButton(this);
}

// Joystick Handler
class Joystick final : public Input::AnalogDevice {
public:
    explicit Joystick(std::shared_ptr<AnalogList> analog_list_) : analog_list(analog_list_) {}

    ~Joystick();

    std::tuple<float, float> GetStatus() const override {
        return std::make_tuple(x_axis.load(), y_axis.load());
    }

    friend class AnalogList;

private:
    std::shared_ptr<AnalogList> analog_list;
    std::atomic<float> x_axis{0.0f};
    std::atomic<float> y_axis{0.0f};
};

struct AnalogPair {
    int analog_id;
    Joystick* key_button;
};

class AnalogList {
public:
    void AddButton(int analog_id, Joystick* key_button) {
        std::lock_guard<std::mutex> guard(mutex);
        list.push_back(AnalogPair{analog_id, key_button});
    }

    void RemoveButton(const Joystick* key_button) {
        std::lock_guard<std::mutex> guard(mutex);
        list.remove_if(
            [key_button](const AnalogPair& pair) { return pair.key_button == key_button; });
    }

    bool ChangeJoystickStatus(int analog_id, float x, float y) {
        std::lock_guard<std::mutex> guard(mutex);
        bool button_found = false;
        for (const AnalogPair& pair : list) {
            if (pair.analog_id == analog_id) {
                pair.key_button->x_axis.store(x);
                pair.key_button->y_axis.store(y);
                button_found = true;
            }
        }
        return button_found;
    }

private:
    std::mutex mutex;
    std::list<AnalogPair> list;
};

AnalogFactory::AnalogFactory() : analog_list{std::make_shared<AnalogList>()} {}

Joystick::~Joystick() {
    analog_list->RemoveButton(this);
}

ButtonFactory::ButtonFactory()
    : button_list{std::make_shared<ButtonList>()},
      analog_button_list{std::make_shared<AnalogButtonList>()} {}

std::unique_ptr<Input::ButtonDevice> ButtonFactory::Create(const Common::ParamPackage& params) {
    if (params.Has("axis")) {
        const int axis_id = params.Get("axis", 0);
        const float threshold = params.Get("threshold", 0.5f);
        const std::string direction_name = params.Get("direction", "");
        bool trigger_if_greater;
        if (direction_name == "+") {
            trigger_if_greater = true;
        } else if (direction_name == "-") {
            trigger_if_greater = false;
        } else {
            trigger_if_greater = true;
            LOG_ERROR(Input, "Unknown direction {}", direction_name);
        }
        std::unique_ptr<AnalogButton> analog_button =
            std::make_unique<AnalogButton>(analog_button_list, threshold, trigger_if_greater);
        analog_button_list->AddAnalogButton(axis_id, analog_button.get());
        return std::move(analog_button);
    }

    int button_id = params.Get("code", 0);
    std::unique_ptr<KeyButton> key_button = std::make_unique<KeyButton>(button_list);
    button_list->AddButton(button_id, key_button.get());
    return std::move(key_button);
}

bool ButtonFactory::PressKey(int button_id) {
    return button_list->ChangeButtonStatus(button_id, true);
}

bool ButtonFactory::ReleaseKey(int button_id) {
    return button_list->ChangeButtonStatus(button_id, false);
}

bool ButtonFactory::AnalogButtonEvent(int axis_id, float axis_val) {
    return analog_button_list->ChangeAxisValue(axis_id, axis_val);
}

std::unique_ptr<Input::AnalogDevice> AnalogFactory::Create(const Common::ParamPackage& params) {
    int analog_id = params.Get("code", 0);
    std::unique_ptr<Joystick> analog = std::make_unique<Joystick>(analog_list);
    analog_list->AddButton(analog_id, analog.get());
    return std::move(analog);
}

bool AnalogFactory::MoveJoystick(int analog_id, float x, float y) {
    return analog_list->ChangeJoystickStatus(analog_id, x, y);
}

ButtonFactory* ButtonHandler() {
    return button.get();
}

AnalogFactory* AnalogHandler() {
    return analog.get();
}

std::string GenerateButtonParamPackage(int button) {
    Common::ParamPackage param{
        {"engine", "gamepad"},
        {"code", std::to_string(button)},
    };
    return param.Serialize();
}

std::string GenerateAnalogButtonParamPackage(int axis, float axis_val) {
    Common::ParamPackage param{
        {"engine", "gamepad"},
        {"axis", std::to_string(axis)},
    };
    if (axis_val > 0) {
        param.Set("direction", "+");
        param.Set("threshold", "0.5");
    } else {
        param.Set("direction", "-");
        param.Set("threshold", "-0.5");
    }

    return param.Serialize();
}

std::string GenerateAnalogParamPackage(int axis_id) {
    Common::ParamPackage param{
        {"engine", "gamepad"},
        {"code", std::to_string(axis_id)},
    };
    return param.Serialize();
}

NDKMotionFactory* NDKMotionHandler() {
    return motion.get();
}

void Init() {
    button = std::make_shared<ButtonFactory>();
    analog = std::make_shared<AnalogFactory>();
    motion = std::make_shared<NDKMotionFactory>();
    Input::RegisterFactory<Input::ButtonDevice>("gamepad", button);
    Input::RegisterFactory<Input::AnalogDevice>("gamepad", analog);
    Input::RegisterFactory<Input::MotionDevice>("motion_emu", motion);
}

void Shutdown() {
    Input::UnregisterFactory<Input::ButtonDevice>("gamepad");
    Input::UnregisterFactory<Input::AnalogDevice>("gamepad");
    Input::UnregisterFactory<Input::MotionDevice>("motion_emu");
    button.reset();
    analog.reset();
    motion.reset();
}

} // namespace InputManager
