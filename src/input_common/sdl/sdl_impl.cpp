// Copyright 2018-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <SDL.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "common/threadsafe_queue.h"
#include "core/frontend/input.h"
#include "input_common/sdl/sdl_impl.h"

// These structures are not actually defined in the headers, so we need to define them here to use
// them.
typedef struct {
    SDL_GameControllerBindType inputType;
    union {
        int button;

        struct {
            int axis;
            int axis_min;
            int axis_max;
        } axis;

        struct {
            int hat;
            int hat_mask;
        } hat;

    } input;

    SDL_GameControllerBindType outputType;
    union {
        SDL_GameControllerButton button;

        struct {
            SDL_GameControllerAxis axis;
            int axis_min;
            int axis_max;
        } axis;

    } output;

} SDL_ExtendedGameControllerBind;

#if SDL_VERSION_ATLEAST(2, 26, 0)
/* our hard coded list of mapping support */
typedef enum {
    SDL_CONTROLLER_MAPPING_PRIORITY_DEFAULT,
    SDL_CONTROLLER_MAPPING_PRIORITY_API,
    SDL_CONTROLLER_MAPPING_PRIORITY_USER,
} SDL_ControllerMappingPriority;

typedef struct _ControllerMapping_t {
    SDL_JoystickGUID guid;
    char* name;
    char* mapping;
    SDL_ControllerMappingPriority priority;
    struct _ControllerMapping_t* next;
} ControllerMapping_t;
#endif

struct _SDL_GameController {
#if SDL_VERSION_ATLEAST(2, 26, 0)
    const void* magic;
#endif

    SDL_Joystick* joystick; /* underlying joystick device */
    int ref_count;

    const char* name;
#if SDL_VERSION_ATLEAST(2, 26, 0)
    ControllerMapping_t* mapping;
#endif
    int num_bindings;
    SDL_ExtendedGameControllerBind* bindings;
    SDL_ExtendedGameControllerBind** last_match_axis;
    Uint8* last_hat_mask;
    Uint32 guide_button_down;

    struct _SDL_GameController* next; /* pointer to next game controller we have allocated */
};

namespace InputCommon {

namespace SDL {

static std::string GetGUID(SDL_Joystick* joystick) {
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    char guid_str[33];
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
    return guid_str;
}

/// Creates a ParamPackage from an SDL_Event that can directly be used to create a ButtonDevice
static Common::ParamPackage SDLEventToButtonParamPackage(SDLState& state, const SDL_Event& event);

static int SDLEventWatcher(void* userdata, SDL_Event* event) {
    SDLState* sdl_state = reinterpret_cast<SDLState*>(userdata);
    switch (event->type) {
    case SDL_JOYAXISMOTION:
    case SDL_JOYHATMOTION:
    case SDL_JOYDEVICEREMOVED: {
        auto joystick = sdl_state->GetSDLJoystickBySDLID(event->jdevice.which);
        if (joystick && joystick->GetSDLGameController()) {
            return 0;
        }
        break;
    }

    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP: {
        // only let these through if they are nonstandard buttons, like back buttons
        auto joystick = sdl_state->GetSDLJoystickBySDLID(event->jdevice.which);
        if (joystick && joystick->GetSDLGameController() &&
            joystick->IsButtonMappedToController(event->jbutton.button)) {
            return 0;
        }
        break;
    }

    case SDL_JOYDEVICEADDED:
        if (SDL_IsGameController(event->jdevice.which)) {
            return 0;
        }
        break;
    }
    // deal
    if (sdl_state->polling) {
        if (sdl_state->event_queue.Size() >= sdl_state->MAX_EVENT_QUEUE_SIZE) {
            // safety - before pushing, clear if it has somehow ballooned in size
            sdl_state->event_queue.Clear();
        }
        sdl_state->event_queue.Push(*event);
    } else {
        sdl_state->HandleGameControllerEvent(*event);
    }
    return 0;
}

constexpr std::array<SDL_GameControllerButton, Settings::NativeButton::NumButtons>
    xinput_to_3ds_mapping = {{
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_GUIDE,
        SDL_CONTROLLER_BUTTON_INVALID,
    }};
constexpr std::array<SDL_GameControllerButton, Settings::NativeButton::NumButtons>
    nintendo_to_3ds_mapping = {{
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_INVALID,
        SDL_CONTROLLER_BUTTON_GUIDE,
        SDL_CONTROLLER_BUTTON_INVALID,
    }};

const std::map<Uint8, std::string> axis_names = {
    {SDL_CONTROLLER_AXIS_LEFTX, "Left Stick X"},
    {SDL_CONTROLLER_AXIS_LEFTY, "Left Stick Y"},
    {SDL_CONTROLLER_AXIS_RIGHTX, "Right Stick X"},
    {SDL_CONTROLLER_AXIS_RIGHTY, "Right Stick Y"},
    {SDL_CONTROLLER_AXIS_INVALID, ""},
    {SDL_CONTROLLER_AXIS_TRIGGERLEFT, "Left Trigger"},
    {SDL_CONTROLLER_AXIS_TRIGGERRIGHT, "Right Trigger"}};

const std::map<Uint8, std::string> button_names = {
    {SDL_CONTROLLER_BUTTON_A, "A / ✖"},
    {SDL_CONTROLLER_BUTTON_B, "B / ●"},
    {SDL_CONTROLLER_BUTTON_X, "X / ■"},
    {SDL_CONTROLLER_BUTTON_Y, "Y / ▲"},
    {SDL_CONTROLLER_BUTTON_BACK, "Back/Select"},
    {SDL_CONTROLLER_BUTTON_GUIDE, "Guide/Home"},
    {SDL_CONTROLLER_BUTTON_START, "Start"},
    {SDL_CONTROLLER_BUTTON_LEFTSTICK, "LS Click"},
    {SDL_CONTROLLER_BUTTON_RIGHTSTICK, "RS Click"},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, "LB"},
    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, "RB"},
    {SDL_CONTROLLER_BUTTON_DPAD_UP, "D-Pad Up"},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, "D-Pad Down"},
    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, "D-Pad Left"},
    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "D-Pad Right"},
    {SDL_CONTROLLER_BUTTON_MISC1, "Misc (Share/Mute)"},
    {SDL_CONTROLLER_BUTTON_PADDLE1, "Paddle 1"},
    {SDL_CONTROLLER_BUTTON_PADDLE2, "Paddle 2"},
    {SDL_CONTROLLER_BUTTON_PADDLE3, "Paddle 3"},
    {SDL_CONTROLLER_BUTTON_PADDLE4, "Paddle 4"},
    {SDL_CONTROLLER_BUTTON_TOUCHPAD, "Touchpad"},
    {SDL_CONTROLLER_BUTTON_INVALID, ""}};
struct SDLJoystickDeleter {
    void operator()(SDL_Joystick* object) {
        SDL_JoystickClose(object);
    }
};
/**
 * Return a list of matching joysticks. All joysticks with GUID are reported, port will be handled
 * elsewhere.
 */
std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> SDLState::GetJoysticksByGUID(
    const std::string& guid) {
    std::lock_guard lock{joystick_map_mutex};
    if (guid.empty() || guid == "0") {
        return joystick_vector;
    } else {
        const auto it = joystick_map.find(guid);
        if (it != joystick_map.end()) {
            return it->second;
        }
        // if no joysticks with this GUID exists, add a fake one to avoid crashes (yuck)
        auto vec = std::make_shared<std::vector<std::shared_ptr<SDLJoystick>>>();
        auto joystick = std::make_shared<SDLJoystick>(guid, 0, nullptr, nullptr);
        vec->emplace_back(joystick);
        joystick_map[guid] = vec;
        return vec;
    }
}

/**
 * Find the most up-to-date SDLJoystick object from an instance id
 */
std::shared_ptr<SDLJoystick> SDLState::GetSDLJoystickBySDLID(SDL_JoystickID sdl_id) {
    // rewriting this method, the old version was more fragile
    std::lock_guard lock{joystick_map_mutex};

    for (auto& [guid, joystick_list] : joystick_map) {
        for (auto& joystick : *joystick_list) {
            SDL_Joystick* sdl_joy = joystick->GetSDLJoystick();
            if (sdl_joy && SDL_JoystickInstanceID(sdl_joy) == sdl_id) {
                return joystick;
            }
        }
    }

    return nullptr;
}

/**
 * Return the button binds param package for the button assuming the params passed in is for the
 * n3ds "A" button to determine whether this is xbox or nintendo layout. If the passed in button
 * is neither A nor B, default to xbox layout as the most common on desktop.
 */
Common::ParamPackage SDLState::GetSDLControllerButtonBind(
    const Common::ParamPackage a_button_params, Settings::NativeButton::Values button) {
    auto guid = a_button_params.Get("guid", "0");
    auto port = a_button_params.Get("port", 0);
    auto a_button = a_button_params.Get("button", -1);
    auto api = a_button_params.Get("api", "joystick");
    // for xinputs, the "A" or right button would normally register as the B button. But if the
    // user is either using a nintendo-layout controller or using xinput but would rather the labels
    // match than the icons, they can press the button that appears as "A" and trigger nintendo
    // automap
    bool is_nintendo_layout =
        api == "controller" && a_button == SDL_GameControllerButton::SDL_CONTROLLER_BUTTON_A;

    Common::ParamPackage params({{"engine", "sdl"}});
    params.Set("guid", guid);
    params.Set("port", port);

    auto mapped_button = is_nintendo_layout ? nintendo_to_3ds_mapping[static_cast<int>(button)]
                                            : xinput_to_3ds_mapping[static_cast<int>(button)];

    params.Set("api", "controller");
    if (button == Settings::NativeButton::Values::ZL) {
        params.Set("axis", SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        params.Set("name", axis_names.at(SDL_CONTROLLER_AXIS_TRIGGERLEFT));
        params.Set("direction", "+");
        params.Set("threshold", 0.5f);
    } else if (button == Settings::NativeButton::Values::ZR) {
        params.Set("axis", SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        params.Set("name", axis_names.at(SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
        params.Set("direction", "+");
        params.Set("threshold", 0.5f);
    } else if (mapped_button == SDL_CONTROLLER_BUTTON_INVALID) {
        return {{}};
    } else {
        params.Set("button", mapped_button);
        params.Set("name", button_names.at(mapped_button));
    }

    return params;
}

Common::ParamPackage SDLState::GetSDLControllerAnalogBindByGUID(
    const std::string& guid, int port, Settings::NativeAnalog::Values analog) {
    Common::ParamPackage params({{"engine", "sdl"}});
    params.Set("guid", guid);
    params.Set("port", port);

    SDL_GameControllerAxis button_bind_x;
    SDL_GameControllerAxis button_bind_y;

    if (analog == Settings::NativeAnalog::Values::CirclePad) {
        button_bind_x = SDL_CONTROLLER_AXIS_LEFTX;
        button_bind_y = SDL_CONTROLLER_AXIS_LEFTY;
    } else if (analog == Settings::NativeAnalog::Values::CStick) {
        button_bind_x = SDL_CONTROLLER_AXIS_RIGHTX;
        button_bind_y = SDL_CONTROLLER_AXIS_RIGHTY;
    } else {
        LOG_WARNING(Input, "analog value out of range {}", analog);
        return {{}};
    }
    params.Set("api", "controller");
    params.Set("axis_x", button_bind_x);
    params.Set("name_x", axis_names.at(button_bind_x));
    params.Set("axis_y", button_bind_y);
    params.Set("name_y", axis_names.at(button_bind_y));
    return params;
}

void SDLState::InitJoystick(int joystick_index) {
    SDL_Joystick* sdl_joystick = nullptr;
    SDL_GameController* sdl_gamecontroller = nullptr;
    if (SDL_IsGameController(joystick_index)) {
        sdl_gamecontroller = SDL_GameControllerOpen(joystick_index);
        if (sdl_gamecontroller) {
            sdl_joystick = SDL_GameControllerGetJoystick(sdl_gamecontroller);
        }
    } else {
        sdl_joystick = SDL_JoystickOpen(joystick_index);
    }
    if (!sdl_joystick) {
        LOG_ERROR(Input, "failed to open joystick {}, with error: {}", joystick_index,
                  SDL_GetError());
        return;
    }
    const std::string guid = GetGUID(sdl_joystick);

    std::lock_guard lock{joystick_map_mutex};
    if (joystick_map.find(guid) == joystick_map.end()) {
        auto joystick = std::make_shared<SDLJoystick>(guid, 0, sdl_joystick, sdl_gamecontroller);
        auto vec = std::make_shared<std::vector<std::shared_ptr<SDLJoystick>>>();
        vec->emplace_back(joystick);
        joystick_map[guid] = vec;
        joystick_vector->emplace_back(joystick);
        return;
    }

    auto& joystick_guid_list = joystick_map[guid];
    const auto it = std::find_if(joystick_guid_list->begin(), joystick_guid_list->end(),
                                 [](const auto& joystick) { return !joystick->GetSDLJoystick(); });
    if (it != joystick_guid_list->end()) {
        (*it)->SetSDLJoystick(sdl_joystick, sdl_gamecontroller);
        joystick_vector->emplace_back(*it);
        return;
    }
    const int port = static_cast<int>(joystick_guid_list->size());
    auto joystick = std::make_shared<SDLJoystick>(guid, port, sdl_joystick, sdl_gamecontroller);
    joystick_guid_list->emplace_back(joystick);
    joystick_vector->emplace_back(joystick);
}

void SDLState::CloseJoystick(SDL_JoystickID instance_id) {
    auto joystick = GetSDLJoystickBySDLID(instance_id);
    if (joystick) {
        LOG_DEBUG(Input, "Closing joystick with instance ID {}", instance_id);
        joystick->SetSDLJoystick(nullptr, nullptr);
    } else {
        LOG_DEBUG(Input, "Joystick with instance ID {} already closed or not found", instance_id);
    }
}

void SDLState::HandleGameControllerEvent(const SDL_Event& event) {
    switch (event.type) {

#if SDL_VERSION_ATLEAST(2, 0, 14)
    case SDL_CONTROLLERSENSORUPDATE: {
        if (auto joystick = GetSDLJoystickBySDLID(event.csensor.which)) {
            switch (event.csensor.sensor) {
            case SDL_SENSOR_ACCEL:
                joystick->SetAccel(event.csensor.data[0] / SDL_STANDARD_GRAVITY,
                                   -event.csensor.data[1] / SDL_STANDARD_GRAVITY,
                                   event.csensor.data[2] / SDL_STANDARD_GRAVITY);
                break;
            case SDL_SENSOR_GYRO:
                joystick->SetGyro(-event.csensor.data[0] * (180.0f / Common::PI),
                                  event.csensor.data[1] * (180.0f / Common::PI),
                                  -event.csensor.data[2] * (180.0f / Common::PI));
                break;
            }
        }
        break;
    }
    case SDL_CONTROLLERTOUCHPADDOWN:
    case SDL_CONTROLLERTOUCHPADMOTION:
        if (auto joystick = GetSDLJoystickBySDLID(event.ctouchpad.which)) {
            joystick->SetTouchpad(event.ctouchpad.x, event.ctouchpad.y, event.ctouchpad.touchpad,
                                  true);
        }
        break;
    case SDL_CONTROLLERTOUCHPADUP:
        if (auto joystick = GetSDLJoystickBySDLID(event.ctouchpad.which)) {
            joystick->SetTouchpad(event.ctouchpad.x, event.ctouchpad.y, event.ctouchpad.touchpad,
                                  false);
        }
        break;
#endif
    // this event will get called twice
    case SDL_CONTROLLERDEVICEREMOVED:
    case SDL_JOYDEVICEREMOVED:
        LOG_DEBUG(Input, "Device removed with Instance_ID {}", event.jdevice.which);
        CloseJoystick(event.jdevice.which);
        break;

    case SDL_CONTROLLERDEVICEADDED:
    case SDL_JOYDEVICEADDED:
        LOG_DEBUG(Input, "Device added with index {}", event.jdevice.which);
        InitJoystick(event.jdevice.which);
        break;
    }
}

void SDLState::CloseJoysticks() {
    std::lock_guard lock{joystick_map_mutex};
    joystick_map.clear();
    joystick_vector->clear();
}

class SDLButton final : public Input::ButtonDevice {
public:
    explicit SDLButton(std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks_,
                       int button_, int port_ = -1, bool isController_ = true)
        : joysticks(joysticks_), button(button_), isController(isController_), port(port_) {}

    bool GetStatus() const override {
        if (port >= 0 && joysticks && static_cast<int>(joysticks->size()) > port &&
            joysticks->at(port)) {
            return joysticks->at(port)->GetButton(button, isController);
        }
        for (const auto& joystick : *joysticks) {
            if (joystick && joystick->GetButton(button, isController)) {
                return true;
            }
        }
        return false;
    }

private:
    std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks;
    int button;
    bool isController = true;
    int port;
};

class SDLDirectionButton final : public Input::ButtonDevice {
public:
    explicit SDLDirectionButton(
        std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks_, int hat_,
        Uint8 direction_, int port_ = -1)
        : joysticks(joysticks_), hat(hat_), direction(direction_), port(port_) {}

    bool GetStatus() const override {
        if (port >= 0 && joysticks && static_cast<int>(joysticks->size()) > port &&
            joysticks->at(port)) {
            return joysticks->at(port)->GetHatDirection(hat, direction);
        }
        for (const auto& joystick : *joysticks) {
            if (joystick && joystick->GetHatDirection(hat, direction))
                return true;
        }
        return false;
    }

private:
    std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks;
    int hat;
    Uint8 direction;
    int port;
};

class SDLAxisButton final : public Input::ButtonDevice {
public:
    explicit SDLAxisButton(std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks_,
                           int axis_, float threshold_, bool trigger_if_greater_, int port_ = -1,
                           bool isController_ = true)
        : joysticks(joysticks_), axis(axis_), threshold(threshold_),
          trigger_if_greater(trigger_if_greater_), isController(isController_), port(port_) {}

    bool GetStatus() const override {
        if (port >= 0 && joysticks && static_cast<int>(joysticks->size()) > port &&
            joysticks->at(port)) {

            float axis_value = joysticks->at(port)->GetAxis(axis, isController);
            if (trigger_if_greater && axis_value > threshold)
                return true;
            else if (!trigger_if_greater && axis_value < threshold)
                return true;
            else
                return false;
        }
        for (const auto& joystick : *joysticks) {
            if (!joystick)
                continue;
            float axis_value = joystick->GetAxis(axis, isController);
            if (trigger_if_greater && axis_value > threshold)
                return true;
            else if (!trigger_if_greater && axis_value < threshold)
                return true;
        }
        return false;
    }

private:
    std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks;
    int axis;
    float threshold;
    bool trigger_if_greater;
    bool isController = true;
    int port;
};

class SDLAnalog final : public Input::AnalogDevice {
public:
    SDLAnalog(std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks_, int axis_x_,
              int axis_y_, float deadzone_, int port_ = -1, bool isController_ = true)
        : joysticks(joysticks_), axis_x(axis_x_), axis_y(axis_y_), deadzone(deadzone_),
          isController(isController_), port(port_) {}

    std::tuple<float, float> GetStatus() const override {
        float rMax = 0.0f, xMax = 0.0f, yMax = 0.0f;
        if (port >= 0 && joysticks && static_cast<int>(joysticks->size()) > port &&
            joysticks->at(port)) {
            const auto [x, y] = joysticks->at(port)->GetAnalog(axis_x, axis_y, isController);
            const float r = std::sqrt((x * x) + (y * y));
            if (r > deadzone) {
                return std::make_tuple(x / r * (r - deadzone) / (1 - deadzone),
                                       y / r * (r - deadzone) / (1 - deadzone));
            }
            return std::make_tuple<float, float>(0.0f, 0.0f);
        }
        // if more than 1, return the value of whichever joystick is greatest
        for (const auto& joystick : *joysticks) {
            if (!joystick)
                continue;
            const auto [x, y] = joystick->GetAnalog(axis_x, axis_y, isController);
            const float r = std::sqrt((x * x) + (y * y));
            if (r > rMax) {
                xMax = x;
                yMax = y;
                rMax = r;
            }
        }
        if (rMax > deadzone) {
            return std::make_tuple(xMax / rMax * (rMax - deadzone) / (1 - deadzone),
                                   yMax / rMax * (rMax - deadzone) / (1 - deadzone));
        }
        return std::make_tuple<float, float>(0.0f, 0.0f);
    }

private:
    std::shared_ptr<std::vector<std::shared_ptr<SDLJoystick>>> joysticks;
    const int axis_x;
    const int axis_y;
    const float deadzone;
    bool isController;
    int port;
};

class SDLMotion final : public Input::MotionDevice {
public:
    explicit SDLMotion(std::shared_ptr<SDLJoystick> joystick_) : joystick(std::move(joystick_)) {}

    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetStatus() const override {
        if (!joystick)
            return std::make_tuple(Common::Vec3<float>(0, 0, 0), Common::Vec3<float>(0, 0, 0));
        return joystick->GetMotion();
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
};

class SDLTouch final : public Input::TouchDevice {
public:
    explicit SDLTouch(std::shared_ptr<SDLJoystick> joystick_, int pad_)
        : joystick(std::move(joystick_)), pad(pad_) {}

    std::tuple<float, float, bool> GetStatus() const override {
        return joystick->GetTouch(pad);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    const int pad;
};

/// A button device factory that creates button devices from SDL joystick
class SDLButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    explicit SDLButtonFactory(SDLState& state_) : state(state_) {}

    /**
     * Creates a button device from a joystick button
     * @param params contains parameters for creating the device:
     *     - "api": either "controller" or "joystick" depending on API used
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type to bind
     *     - "button"(optional): the index of the joystick button to bind
     *     - "maptype" (optional): can be "guid+port, "guid", "all" - which components should be
     *          used for binding
     *     - "hat"(optional): the index of the hat to bind as direction buttons
     *     - "axis"(optional): the index of the joystick or controller axis to bind
     *     - "direction"(only used for hat): the direction name of the hat to bind. Can be "up",
     *         "down", "left" or "right"
     *     - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
     *         triggered if the axis value crosses
     *     - "direction"(only used for axis): "+" means the button is triggered when the axis
     *          value is greater than the threshold; "-" means the button is triggered when the axis
     *          value is smaller than the threshold
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        const std::string maptype = params.Get("maptype", "guid+port");
        const int port = maptype == "guid+port" ? params.Get("port", -1) : -1;
        const bool controller = params.Get("api", "joystick") == "controller";
        const std::string guid = (controller && maptype == "all") ? "" : params.Get("guid", "");

        auto joysticks = state.GetJoysticksByGUID(guid);
        if (params.Has("hat")) {
            const int hat = params.Get("hat", 0);
            const std::string direction_name = params.Get("direction", "");
            Uint8 direction;
            if (direction_name == "up") {
                direction = SDL_HAT_UP;
            } else if (direction_name == "down") {
                direction = SDL_HAT_DOWN;
            } else if (direction_name == "left") {
                direction = SDL_HAT_LEFT;
            } else if (direction_name == "right") {
                direction = SDL_HAT_RIGHT;
            } else {
                direction = 0;
            }
            return std::make_unique<SDLDirectionButton>(joysticks, hat, direction, port);
        }

        if (params.Has("axis")) {
            const int axis = params.Get("axis", 0);
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
            return std::make_unique<SDLAxisButton>(joysticks, axis, threshold, trigger_if_greater,
                                                   port, controller);
        }
        const int button = params.Get("button", 0);
        return std::make_unique<SDLButton>(joysticks, button, port, controller);
    }

private:
    SDLState& state;
};

/// An analog device factory that creates analog devices from SDL joystick
class SDLAnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    explicit SDLAnalogFactory(SDLState& state_) : state(state_) {}
    /**
     * Creates analog device from joystick axes
     * @param params contains parameters for creating the device:
     *     - "api": either "controller" or "joystick" based on API used
     *     - "guid": the guid of the joystick to bind or all controllers (when possible))
     *     - "port": the nth joystick of the same type
     *     - "maptype": could be "guid+port", "guid", or "all"
     *     - "axis_x": the index of the axis to be bind as x-axis
     *     - "axis_y": the index of the axis to be bind as y-axis
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        const std::string maptype = params.Get("maptype", "guid+port");
        const bool controller = params.Get("api", "joystick") == "controller";
        const std::string guid = (controller && maptype == "all") ? "" : params.Get("guid", "0");
        const int axis_x = params.Get("axis_x", 0);
        const int axis_y = params.Get("axis_y", 1);
        const int port = maptype == "guid+port" ? params.Get("port", -1) : -1;

        float deadzone = std::clamp(params.Get("deadzone", 0.0f), 0.0f, .99f);

        auto joysticks = state.GetJoysticksByGUID(guid);

        return std::make_unique<SDLAnalog>(joysticks, axis_x, axis_y, deadzone, port, controller);
    }

private:
    SDLState& state;
};

class SDLMotionFactory final : public Input::Factory<Input::MotionDevice> {
public:
    explicit SDLMotionFactory(SDLState& state_) : state(state_) {}

    std::unique_ptr<Input::MotionDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);
        auto joysticks = state.GetJoysticksByGUID(guid);
        if (joysticks->empty())
            return std::make_unique<SDLMotion>(nullptr);
        auto joystick =
            static_cast<int>(joysticks->size()) > port ? joysticks->at(port) : joysticks->at(0);
        return std::make_unique<SDLMotion>(joystick);
    }

private:
    SDLState& state;
};
/**
 * A factory that creates a TouchDevice from an SDL Touchpad
 */
class SDLTouchFactory final : public Input::Factory<Input::TouchDevice> {
public:
    explicit SDLTouchFactory(SDLState& state_) : state(state_) {}
    /**
     * Creates touch device from touchpad
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type
     *     - "touchpad": which touchpad to bind
     */
    std::unique_ptr<Input::TouchDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);
        const int touchpad = params.Get("touchpad", 0);
        auto joysticks = state.GetJoysticksByGUID(guid);
        auto joystick =
            static_cast<int>(joysticks->size()) > port ? joysticks->at(port) : joysticks->at(0);
        return std::make_unique<SDLTouch>(joystick, touchpad);
    }

private:
    SDLState& state;
};

SDLState::SDLState() {
    using namespace Input;
    RegisterFactory<ButtonDevice>("sdl", std::make_shared<SDLButtonFactory>(*this));
    RegisterFactory<AnalogDevice>("sdl", std::make_shared<SDLAnalogFactory>(*this));
    RegisterFactory<MotionDevice>("sdl", std::make_shared<SDLMotionFactory>(*this));
    RegisterFactory<TouchDevice>("sdl", std::make_shared<SDLTouchFactory>(*this));
    // If the frontend is going to manage the event loop, then we dont start one here
    start_thread = !SDL_WasInit(SDL_INIT_GAMECONTROLLER);
    if (start_thread && SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        LOG_CRITICAL(Input, "SDL_Init(SDL_INIT_GAMECONTROLLER) failed with: {}", SDL_GetError());
        return;
    }
    if (SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1") == SDL_FALSE) {
        LOG_ERROR(Input, "Failed to set Hint for background events: {}", SDL_GetError());
    }
// these hints are only defined on sdl2.0.9 or higher
#if SDL_VERSION_ATLEAST(2, 0, 9)
#if !SDL_VERSION_ATLEAST(2, 0, 12)
    // There are also hints to toggle the individual drivers if needed.
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
#endif
#endif

    // Prevent SDL from adding undesired axis
#ifdef SDL_HINT_ACCELEROMETER_AS_JOYSTICK
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
#endif

    // Enable HIDAPI rumble. This prevents SDL from disabling motion on PS4 and PS5 controllers
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
#endif

    SDL_AddEventWatch(&SDLEventWatcher, this);

    initialized = true;
    if (start_thread) {
        poll_thread = std::thread([this] {
            using namespace std::chrono_literals;
            while (initialized) {
                SDL_PumpEvents();
                std::this_thread::sleep_for(10ms);
            }
        });
    }
    // Because the events for joystick connection happens before we have our event watcher
    // added, we can just open all the joysticks right here
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        InitJoystick(i);
    }
}

SDLState::~SDLState() {
    using namespace Input;
    UnregisterFactory<ButtonDevice>("sdl");
    UnregisterFactory<AnalogDevice>("sdl");
    UnregisterFactory<MotionDevice>("sdl");
    UnregisterFactory<TouchDevice>("sdl");
    CloseJoysticks();
    SDL_DelEventWatch(&SDLEventWatcher, this);

    initialized = false;
    if (start_thread) {
        poll_thread.join();
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    }
}

Common::ParamPackage SDLEventToButtonParamPackage(SDLState& state, const SDL_Event& event,
                                                  const bool down = false) {
    Common::ParamPackage params({{"engine", "sdl"}});
    // we need to track if this is a press or release
    if (down) {
        params.Set("down", "1");
    } else {
        params.Erase("down");
    }

    auto joystick = state.GetSDLJoystickBySDLID(event.jhat.which);
    if (!joystick)
        return {};
    params.Set("port", joystick->GetPort());
    params.Set("guid", joystick->GetGUID());

    switch (event.type) {
    case SDL_CONTROLLERAXISMOTION: {
        params.Set("api", "controller");
        if (axis_names.contains(event.caxis.axis)) {
            params.Set("name", axis_names.at(event.caxis.axis));
        }
        params.Set("axis", event.caxis.axis);
        if (event.caxis.value > 0) {
            params.Set("direction", "+");
            params.Set("threshold", "0.5");
        } else {
            params.Set("direction", "-");
            params.Set("threshold", "-0.5");
        }
        break;
    }
    case SDL_JOYAXISMOTION: {
        params.Set("api", "joystick");
        params.Set("axis", event.jaxis.axis);
        if (event.jaxis.value > 0) {
            params.Set("direction", "+");
            params.Set("threshold", "0.5");
        } else {
            params.Set("direction", "-");
            params.Set("threshold", "-0.5");
        }
        break;
    }
    case SDL_CONTROLLERBUTTONUP:
    case SDL_CONTROLLERBUTTONDOWN: {
        if (button_names.contains(event.cbutton.button)) {
            params.Set("name", button_names.at(event.cbutton.button));
        }
        params.Set("api", "controller");
        params.Set("button", event.cbutton.button);
        break;
    }
    case SDL_JOYBUTTONUP:
    case SDL_JOYBUTTONDOWN: {
        params.Set("api", "joystick");
        params.Set("button", event.jbutton.button);
        break;
    }
    case SDL_JOYHATMOTION: {
        params.Set("port", joystick->GetPort());
        params.Set("guid", joystick->GetGUID());
        params.Set("hat", event.jhat.hat);
        switch (event.jhat.value) {
        // if the received motion is up, down, left, or right
        // then it will be marked as "down" above and we need to store which
        // button is pressed
        case SDL_HAT_UP:
            params.Set("direction", "up");
            joystick->current_hat_down = "up";
            break;
        case SDL_HAT_DOWN:
            params.Set("direction", "down");
            joystick->current_hat_down = "down";
            break;
        case SDL_HAT_LEFT:
            params.Set("direction", "left");
            joystick->current_hat_down = "left";
            break;
        case SDL_HAT_RIGHT:
            params.Set("direction", "right");
            joystick->current_hat_down = "right";
            break;
        // if the hat is now centered, we will interpret that as releasing
        // the previously pressed button and clear the hat state
        case SDL_HAT_CENTERED:
            params.Set("direction", joystick->current_hat_down);
            joystick->current_hat_down = "";
            break;
        default:
            return {};
        }
        break;
    }
    }

    return params;
}

namespace Polling {

class SDLPoller : public InputCommon::Polling::DevicePoller {
public:
    explicit SDLPoller(SDLState& state_) : state(state_) {}

    void Start() override {
        state.event_queue.Clear();

        state.polling = true;
    }

    void Stop() override {
        state.event_queue.Clear();
        state.polling = false;
    }

protected:
    SDLState& state;
};

class SDLTouchpadPoller final : public SDLPoller {
public:
    explicit SDLTouchpadPoller(SDLState& state_) : SDLPoller(state_) {}

    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        Common::ParamPackage params;
        while (state.event_queue.Pop(event)) {
            if (event.type != SDL_CONTROLLERTOUCHPADDOWN) {
                continue;
            }
            switch (event.type) {
            case SDL_CONTROLLERTOUCHPADDOWN:
                auto joystick = state.GetSDLJoystickBySDLID(event.ctouchpad.which);
                params.Set("engine", "sdl");
                params.Set("touchpad", event.ctouchpad.touchpad);
                params.Set("port", joystick->GetPort());
                params.Set("guid", joystick->GetGUID());
            }
        }
        return params;
    }
};

class SDLButtonPoller final : public SDLPoller {
public:
    explicit SDLButtonPoller(SDLState& state_) : SDLPoller(state_) {}

    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (state.event_queue.Pop(event)) {
            auto axis = event.jaxis.axis;
            auto id = event.jaxis.which;
            auto value = event.jaxis.value;
            auto timestamp = event.jaxis.timestamp;
            bool controller = false;
            switch (event.type) {
            case SDL_CONTROLLERAXISMOTION: {
                axis = event.caxis.axis;
                value = event.caxis.value;
                timestamp = event.caxis.timestamp;
                value = event.caxis.value;
                axis_center_value[id][axis] = 0;
                controller = true;
                [[fallthrough]];
            }
            case SDL_JOYAXISMOTION: {
                // if a button has been pressed down within 50ms of this axis movement,
                // assume they are actually the same thing and skip this axis
                if (buttonDownTimestamp &&
                    ((timestamp >= buttonDownTimestamp && timestamp - buttonDownTimestamp <= 50) ||
                     (timestamp < buttonDownTimestamp && buttonDownTimestamp - timestamp <= 50))) {
                    axis_skip[id][axis] = true;
                    break;
                }

                // skipping this axis
                if (axis_skip[id][axis])
                    break;
                if (!axis_memory.count(id) || !axis_memory[id].count(axis)) {
                    // starting a new movement.
                    axisStartTimestamps[id][axis] = event.jaxis.timestamp;
                    axis_event_count[id][axis] = 1;
                    if (axis_center_value.contains(id) && axis_center_value[id].contains(axis) &&
                        IsAxisAtExtreme(value)) {
                        // a single event with a value at the extreme after we
                        // have established the center value. Must be a digital axis press.
                        event.caxis.value = std::copysign(32767, value);
                        return SDLEventToButtonParamPackage(state, event, true);
                    }
                    // otherwise, this is our first event ever on this axis,
                    // so assume we are near the center
                    if (value < -28000)
                        axis_center_value[id][axis] = -32768;
                    else if (value > 28000)
                        axis_center_value[id][axis] = 32767;
                    else
                        axis_center_value[id][axis] = 0;

                    axis_memory[id][axis] = axis_center_value[id][axis];
                    break;
                } else {
                    axis_event_count[id][axis]++;
                    if (axis_event_count[id][axis] == 2 && IsAxisAtPole(value) &&
                        IsAxisAtPole(axis_memory[id][axis])) {
                        // if there are two events and both are at poles, the only reasonable
                        // way that happens is if this is the second half of a digital axis release.
                        // So treat this as a release and make sure this is recorded as the center
                        axis_event_count[id][axis] = 0;
                        if (!IsAxisAtCenter(value, id, axis)) {
                            // fix the center value
                            if (value > 28000)
                                axis_center_value[id][axis] = 32767;
                            else if (value < -28000)
                                axis_center_value[id][axis] = -32768;
                            else
                                axis_center_value[id][axis] = 0;
                        }
                        return SDLEventToButtonParamPackage(state, event, false);
                    }
                    if (IsAxisAtCenter(value, id, axis) &&
                        IsAxisPastThreshold(axis_memory[id][axis], id, axis)) {
                        // returned to center, send the up signal
                        if (controller) {
                            event.caxis.value = static_cast<Sint16>(std::copysign(
                                32767, axis_memory[id][axis] - axis_center_value[id][axis]));
                        } else {
                            event.jaxis.value = static_cast<Sint16>(std::copysign(
                                32767, axis_memory[id][axis] - axis_center_value[id][axis]));
                        }
                        axis_memory[id][axis] = axis_center_value[id][axis];
                        axis_event_count[id][axis] = 0;
                        return SDLEventToButtonParamPackage(state, event, false);
                    } else if (IsAxisAtCenter(axis_memory[id][axis], id, axis) &&
                               IsAxisPastThreshold(event.jaxis.value, id, axis)) {
                        if (controller) {
                            event.caxis.value = static_cast<Sint16>(
                                std::copysign(32767, value - axis_center_value[id][axis]));
                            axis_memory[id][axis] = event.caxis.value;
                        } else {
                            event.jaxis.value = static_cast<Sint16>(
                                std::copysign(32767, value - axis_center_value[id][axis]));
                            axis_memory[id][axis] = event.jaxis.value;
                        }
                        return SDLEventToButtonParamPackage(state, event, true);
                    }
                }
                break;
            }
            case SDL_CONTROLLERBUTTONDOWN: {
                buttonDownTimestamp = event.cbutton.timestamp;
                return SDLEventToButtonParamPackage(state, event, true);
            }
            case SDL_JOYBUTTONDOWN: {
                buttonDownTimestamp = event.jbutton.timestamp;
                return SDLEventToButtonParamPackage(state, event, true);
                break;
            }
            case SDL_CONTROLLERBUTTONUP: {
                return SDLEventToButtonParamPackage(state, event, false);
                break;
            }
            case SDL_JOYBUTTONUP: {
                return SDLEventToButtonParamPackage(state, event, false);
                break;
            }
            case SDL_JOYHATMOTION: {
                // send this as a pressed signal for every motion but SDL_HAT_CENTERED, which
                // we will interpret as a release
                return SDLEventToButtonParamPackage(state, event,
                                                    event.jhat.value != SDL_HAT_CENTERED);
                break;
            }
            }
        }
        return {};
    }

private:
    bool IsAxisAtCenter(int16_t value, SDL_JoystickID id, uint8_t axis) {
        return axis_center_value[id].contains(axis) &&
               std::abs(value - axis_center_value[id][axis]) < 367;
    }

    bool IsAxisPastThreshold(int16_t value, SDL_JoystickID id, uint8_t axis) {
        return std::abs(value - axis_center_value[id][axis]) > 32767 / 2;
    }

    bool IsAxisAtExtreme(int16_t value) {
        return std::abs(value) > 32300;
    }

    bool IsAxisAtPole(int16_t value) {
        return IsAxisAtExtreme(value) || std::abs(value) < 367;
    }

    /** Holds the first received value for the axis. Used to
     *  identify situations where "released" is -32768 (some triggers)
     */
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, int16_t>> axis_center_value;
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, int16_t>> axis_memory;
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, uint32_t>> axis_event_count;
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, bool>> axis_skip;
    int buttonDownTimestamp = 0;
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, int>> axisStartTimestamps;
};

class SDLAnalogPoller final : public SDLPoller {
public:
    explicit SDLAnalogPoller(SDLState& state_) : SDLPoller(state_) {}

    void Start() override {
        SDLPoller::Start();

        // Reset stored axes
        analog_xaxis = -1;
        analog_yaxis = -1;
        analog_axes_joystick = -1;
    }

    Common::ParamPackage GetNextInput() override {
        SDL_Event event{};
        SDL_JoystickID which = -1;
        while (state.event_queue.Pop(event)) {
            if ((event.type != SDL_JOYAXISMOTION && event.type != SDL_CONTROLLERAXISMOTION)) {
                continue;
            }
            which = event.type == SDL_JOYAXISMOTION ? event.jaxis.which : event.caxis.which;
            auto value = event.type == SDL_JOYAXISMOTION ? event.jaxis.value : event.caxis.value;

            if (std::abs(value / 32767.0) < 0.5)
                continue;
            // An analog device needs two axes, so we need to store the axis for later and wait
            // for a second SDL event. The axes also must be from the same joystick.
            int axis = event.type == SDL_JOYAXISMOTION ? event.jaxis.axis : event.caxis.axis;

            if (analog_xaxis == -1) {
                analog_xaxis = axis;
                analog_axes_joystick = which;
            } else if (analog_yaxis == -1 && analog_xaxis != axis &&
                       analog_axes_joystick == which) {
                analog_yaxis = axis;
            }
        }
        Common::ParamPackage params;
        if (which != -1 && analog_xaxis != -1 && analog_yaxis != -1) {
            auto joystick = state.GetSDLJoystickBySDLID(which);
            params.Set("engine", "sdl");
            params.Set("port", joystick->GetPort());
            params.Set("guid", joystick->GetGUID());
            params.Set("axis_x", analog_xaxis);
            params.Set("axis_y", analog_yaxis);
            if (event.type == SDL_JOYAXISMOTION) {
                params.Set("api", "joystick");
            } else {
                params.Set("api", "controller");
                params.Set("name_x", axis_names.at(analog_xaxis));
                params.Set("name_y", axis_names.at(analog_yaxis));
            }
            analog_xaxis = -1;
            analog_yaxis = -1;
            analog_axes_joystick = -1;
            return params;
        }
        return params;
    }

private:
    int analog_xaxis = -1;
    int analog_yaxis = -1;
    SDL_JoystickID analog_axes_joystick = -1;
};
} // namespace Polling

SDLState::Pollers SDLState::GetPollers(InputCommon::Polling::DeviceType type) {
    Pollers pollers;

    switch (type) {
    case InputCommon::Polling::DeviceType::Analog:
        pollers.emplace_back(std::make_unique<Polling::SDLAnalogPoller>(*this));
        break;
    case InputCommon::Polling::DeviceType::Button:
        pollers.emplace_back(std::make_unique<Polling::SDLButtonPoller>(*this));
        break;
    case InputCommon::Polling::DeviceType::Touchpad:
        pollers.emplace_back(std::make_unique<Polling::SDLTouchpadPoller>(*this));
        break;
    }

    return pollers;
}

} // namespace SDL
} // namespace InputCommon
