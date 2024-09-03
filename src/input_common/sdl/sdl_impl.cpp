// Copyright 2018 Citra Emulator Project
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
    // Don't handle the event if we are configuring
    if (sdl_state->polling) {
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

struct SDLJoystickDeleter {
    void operator()(SDL_Joystick* object) {
        SDL_JoystickClose(object);
    }
};
class SDLJoystick {
public:
    SDLJoystick(std::string guid_, int port_, SDL_Joystick* joystick,
                SDL_GameController* game_controller)
        : guid{std::move(guid_)}, port{port_}, sdl_joystick{joystick, &SDL_JoystickClose},
          sdl_controller{game_controller, &SDL_GameControllerClose} {
        EnableMotion();
    }

    void EnableMotion() {
        if (!sdl_controller) {
            return;
        }
#if SDL_VERSION_ATLEAST(2, 0, 14)
        SDL_GameController* controller = sdl_controller.get();

        if (HasMotion()) {
            SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_FALSE);
            SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_FALSE);
        }
        has_accel = SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE;
        has_gyro = SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE;
        if (has_accel) {
            SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, SDL_TRUE);
        }
        if (has_gyro) {
            SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, SDL_TRUE);
        }
#endif
    }

    bool HasMotion() const {
        return has_gyro || has_accel;
    }

    void SetButton(int button, bool value) {
        std::lock_guard lock{mutex};
        state.buttons[button] = value;
    }

    bool GetButton(int button) const {
        std::lock_guard lock{mutex};
        return state.buttons.at(button);
    }

    void SetAxis(int axis, Sint16 value) {
        std::lock_guard lock{mutex};
        state.axes[axis] = value;
    }

    float GetAxis(int axis) const {
        std::lock_guard lock{mutex};
        return state.axes.at(axis) / 32767.0f;
    }

    std::tuple<float, float> GetAnalog(int axis_x, int axis_y) const {
        float x = GetAxis(axis_x);
        float y = GetAxis(axis_y);
        y = -y; // 3DS uses an y-axis inverse from SDL

        // Make sure the coordinates are in the unit circle,
        // otherwise normalize it.
        float r = x * x + y * y;
        if (r > 1.0f) {
            r = std::sqrt(r);
            x /= r;
            y /= r;
        }

        return std::make_tuple(x, y);
    }

    void SetHat(int hat, Uint8 direction) {
        std::lock_guard lock{mutex};
        state.hats[hat] = direction;
    }

    bool GetHatDirection(int hat, Uint8 direction) const {
        std::lock_guard lock{mutex};
        return (state.hats.at(hat) & direction) != 0;
    }

    void SetAccel(const float x, const float y, const float z) {
        std::lock_guard lock{mutex};
        state.accel.x = x;
        state.accel.y = y;
        state.accel.z = z;
    }
    void SetGyro(const float pitch, const float yaw, const float roll) {
        std::lock_guard lock{mutex};
        state.gyro.x = pitch;
        state.gyro.y = yaw;
        state.gyro.z = roll;
    }
    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetMotion() const {
        std::lock_guard lock{mutex};
        return std::make_tuple(state.accel, state.gyro);
    }

    /**
     * The guid of the joystick
     */
    const std::string& GetGUID() const {
        return guid;
    }

    /**
     * The number of joystick from the same type that were connected before this joystick
     */
    int GetPort() const {
        return port;
    }

    SDL_Joystick* GetSDLJoystick() const {
        return sdl_joystick.get();
    }

    SDL_GameController* GetSDLGameController() const {
        return sdl_controller.get();
    }

    void SetSDLJoystick(SDL_Joystick* joystick, SDL_GameController* controller) {
        sdl_joystick.reset(joystick);
        sdl_controller.reset(controller);
    }

private:
    struct State {
        std::unordered_map<int, bool> buttons;
        std::unordered_map<int, Sint16> axes;
        std::unordered_map<int, Uint8> hats;
        Common::Vec3<float> accel;
        Common::Vec3<float> gyro;
    } state;
    std::string guid;
    int port;
    bool has_gyro{false};
    bool has_accel{false};
    std::unique_ptr<SDL_Joystick, decltype(&SDL_JoystickClose)> sdl_joystick;
    std::unique_ptr<SDL_GameController, decltype(&SDL_GameControllerClose)> sdl_controller;
    mutable std::mutex mutex;
};

struct SDLGameControllerDeleter {
    void operator()(SDL_GameController* object) {
        SDL_GameControllerClose(object);
    }
};
class SDLGameController {
public:
    SDLGameController(std::string guid_, int port_, SDL_GameController* controller)
        : guid{std::move(guid_)}, port{port_}, sdl_controller{controller} {}

    /**
     * The guid of the joystick/controller
     */
    const std::string& GetGUID() const {
        return guid;
    }

    /**
     * The number of joystick from the same type that were connected before this joystick
     */
    int GetPort() const {
        return port;
    }

    SDL_GameController* GetSDLGameController() const {
        return sdl_controller.get();
    }

    void SetSDLGameController(SDL_GameController* controller) {
        sdl_controller = std::unique_ptr<SDL_GameController, SDLGameControllerDeleter>(controller);
    }

private:
    std::string guid;
    int port;
    std::unique_ptr<SDL_GameController, SDLGameControllerDeleter> sdl_controller;
};

/**
 * Get the nth joystick with the corresponding GUID
 */
std::shared_ptr<SDLJoystick> SDLState::GetSDLJoystickByGUID(const std::string& guid, int port) {
    std::lock_guard lock{joystick_map_mutex};
    const auto it = joystick_map.find(guid);
    if (it != joystick_map.end()) {
        while (it->second.size() <= static_cast<std::size_t>(port)) {
            auto joystick = std::make_shared<SDLJoystick>(guid, static_cast<int>(it->second.size()),
                                                          nullptr, nullptr);
            it->second.emplace_back(std::move(joystick));
        }
        return it->second[static_cast<std::size_t>(port)];
    }
    auto joystick = std::make_shared<SDLJoystick>(guid, 0, nullptr, nullptr);
    return joystick_map[guid].emplace_back(std::move(joystick));
}

/**
 * Check how many identical joysticks (by guid) were connected before the one with sdl_id and so tie
 * it to a SDLJoystick with the same guid and that port
 */
std::shared_ptr<SDLJoystick> SDLState::GetSDLJoystickBySDLID(SDL_JoystickID sdl_id) {
    auto sdl_joystick = SDL_JoystickFromInstanceID(sdl_id);
    const std::string guid = GetGUID(sdl_joystick);

    std::lock_guard lock{joystick_map_mutex};
    auto map_it = joystick_map.find(guid);

    if (map_it == joystick_map.end()) {
        return nullptr;
    }

    const auto vec_it = std::find_if(map_it->second.begin(), map_it->second.end(),
                                     [&sdl_joystick](const auto& joystick) {
                                         return joystick->GetSDLJoystick() == sdl_joystick;
                                     });

    if (vec_it == map_it->second.end()) {
        return nullptr;
    }

    return *vec_it;
}

Common::ParamPackage SDLState::GetSDLControllerButtonBindByGUID(
    const std::string& guid, int port, Settings::NativeButton::Values button) {
    Common::ParamPackage params({{"engine", "sdl"}});
    params.Set("guid", guid);
    params.Set("port", port);
    SDL_GameController* controller = GetSDLJoystickByGUID(guid, port)->GetSDLGameController();
    SDL_GameControllerButtonBind button_bind;

    if (!controller) {
        LOG_WARNING(Input, "failed to open controller {}", guid);
        return {{}};
    }

    auto mapped_button = xinput_to_3ds_mapping[static_cast<int>(button)];
    if (mapped_button == SDL_CONTROLLER_BUTTON_INVALID) {
        if (button == Settings::NativeButton::Values::ZL) {
            button_bind =
                SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        } else if (button == Settings::NativeButton::Values::ZR) {
            button_bind =
                SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        } else {
            return {{}};
        }
    } else {
        button_bind = SDL_GameControllerGetBindForButton(controller, mapped_button);
    }

    switch (button_bind.bindType) {
    case SDL_CONTROLLER_BINDTYPE_BUTTON:
        params.Set("button", button_bind.value.button);
        break;
    case SDL_CONTROLLER_BINDTYPE_HAT:
        params.Set("hat", button_bind.value.hat.hat);
        switch (button_bind.value.hat.hat_mask) {
        case SDL_HAT_UP:
            params.Set("direction", "up");
            break;
        case SDL_HAT_DOWN:
            params.Set("direction", "down");
            break;
        case SDL_HAT_LEFT:
            params.Set("direction", "left");
            break;
        case SDL_HAT_RIGHT:
            params.Set("direction", "right");
            break;
        default:
            return {{}};
        }
        break;
    case SDL_CONTROLLER_BINDTYPE_AXIS:
        params.Set("axis", button_bind.value.axis);

#if SDL_VERSION_ATLEAST(2, 0, 6)
        {
            if (mapped_button != SDL_CONTROLLER_BUTTON_INVALID) {
                const SDL_ExtendedGameControllerBind extended_bind =
                    controller->bindings[mapped_button];
                if (extended_bind.input.axis.axis_max < extended_bind.input.axis.axis_min) {
                    params.Set("direction", "-");
                } else {
                    params.Set("direction", "+");
                }
                params.Set("threshold", (extended_bind.input.axis.axis_min +
                                         (extended_bind.input.axis.axis_max -
                                          extended_bind.input.axis.axis_min) /
                                             2.0f) /
                                            SDL_JOYSTICK_AXIS_MAX);
            }
        }
#else
        params.Set("direction", "+"); // lacks extended_bind, so just a guess
#endif
        break;
    case SDL_CONTROLLER_BINDTYPE_NONE:
        LOG_WARNING(Input, "Button not bound: {}", Settings::NativeButton::mapping[button]);
        return {{}};
    default:
        LOG_WARNING(Input, "unknown SDL bind type {}", button_bind.bindType);
        return {{}};
    }

    return params;
}

Common::ParamPackage SDLState::GetSDLControllerAnalogBindByGUID(
    const std::string& guid, int port, Settings::NativeAnalog::Values analog) {
    Common::ParamPackage params({{"engine", "sdl"}});
    params.Set("guid", guid);
    params.Set("port", port);
    SDL_GameController* controller = GetSDLJoystickByGUID(guid, port)->GetSDLGameController();
    SDL_GameControllerButtonBind button_bind_x;
    SDL_GameControllerButtonBind button_bind_y;

    if (!controller) {
        LOG_WARNING(Input, "failed to open controller {}", guid);
        return {{}};
    }

    if (analog == Settings::NativeAnalog::Values::CirclePad) {
        button_bind_x = SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        button_bind_y = SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    } else if (analog == Settings::NativeAnalog::Values::CStick) {
        button_bind_x = SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        button_bind_y = SDL_GameControllerGetBindForAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);
    } else {
        LOG_WARNING(Input, "analog value out of range {}", analog);
        return {{}};
    }

    if (button_bind_x.bindType != SDL_CONTROLLER_BINDTYPE_AXIS ||
        button_bind_y.bindType != SDL_CONTROLLER_BINDTYPE_AXIS) {
        return {{}};
    }
    params.Set("axis_x", button_bind_x.value.axis);
    params.Set("axis_y", button_bind_y.value.axis);
    return params;
}

void SDLState::InitJoystick(int joystick_index) {
    SDL_Joystick* sdl_joystick = SDL_JoystickOpen(joystick_index);
    SDL_GameController* sdl_gamecontroller = nullptr;

    if (SDL_IsGameController(joystick_index)) {
        sdl_gamecontroller = SDL_GameControllerOpen(joystick_index);
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
        joystick->EnableMotion();
        joystick_map[guid].emplace_back(std::move(joystick));
        return;
    }

    auto& joystick_guid_list = joystick_map[guid];
    const auto it = std::find_if(joystick_guid_list.begin(), joystick_guid_list.end(),
                                 [](const auto& joystick) { return !joystick->GetSDLJoystick(); });
    if (it != joystick_guid_list.end()) {
        (*it)->SetSDLJoystick(sdl_joystick, sdl_gamecontroller);
        (*it)->EnableMotion();
        return;
    }
    const int port = static_cast<int>(joystick_guid_list.size());
    auto joystick = std::make_shared<SDLJoystick>(guid, port, sdl_joystick, sdl_gamecontroller);
    joystick->EnableMotion();
    joystick_guid_list.emplace_back(std::move(joystick));
}

void SDLState::CloseJoystick(SDL_Joystick* sdl_joystick) {
    const auto guid = GetGUID(sdl_joystick);

    std::scoped_lock lock{joystick_map_mutex};
    // This call to guid is safe since the joystick is guaranteed to be in the map
    const auto& joystick_guid_list = joystick_map[guid];
    const auto joystick_it = std::find_if(joystick_guid_list.begin(), joystick_guid_list.end(),
                                          [&sdl_joystick](const auto& joystick) {
                                              return joystick->GetSDLJoystick() == sdl_joystick;
                                          });

    if (joystick_it != joystick_guid_list.end()) {
        (*joystick_it)->SetSDLJoystick(nullptr, nullptr);
    }
}

void SDLState::HandleGameControllerEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_JOYBUTTONUP: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jbutton.which)) {
            joystick->SetButton(event.jbutton.button, false);
        }
        break;
    }
    case SDL_JOYBUTTONDOWN: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jbutton.which)) {
            joystick->SetButton(event.jbutton.button, true);
        }
        break;
    }
    case SDL_JOYHATMOTION: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jhat.which)) {
            joystick->SetHat(event.jhat.hat, event.jhat.value);
        }
        break;
    }
    case SDL_JOYAXISMOTION: {
        if (auto joystick = GetSDLJoystickBySDLID(event.jaxis.which)) {
            joystick->SetAxis(event.jaxis.axis, event.jaxis.value);
        }
        break;
    }
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
#endif
    case SDL_JOYDEVICEREMOVED:
        LOG_DEBUG(Input, "Joystick removed with Instance_ID {}", event.jdevice.which);
        CloseJoystick(SDL_JoystickFromInstanceID(event.jdevice.which));
        break;
    case SDL_JOYDEVICEADDED:
        LOG_DEBUG(Input, "Joystick connected with device index {}", event.jdevice.which);
        InitJoystick(event.jdevice.which);
        break;
    }
}

void SDLState::CloseJoysticks() {
    std::lock_guard lock{joystick_map_mutex};
    joystick_map.clear();
}

class SDLButton final : public Input::ButtonDevice {
public:
    explicit SDLButton(std::shared_ptr<SDLJoystick> joystick_, int button_)
        : joystick(std::move(joystick_)), button(button_) {}

    bool GetStatus() const override {
        return joystick->GetButton(button);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int button;
};

class SDLDirectionButton final : public Input::ButtonDevice {
public:
    explicit SDLDirectionButton(std::shared_ptr<SDLJoystick> joystick_, int hat_, Uint8 direction_)
        : joystick(std::move(joystick_)), hat(hat_), direction(direction_) {}

    bool GetStatus() const override {
        return joystick->GetHatDirection(hat, direction);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int hat;
    Uint8 direction;
};

class SDLAxisButton final : public Input::ButtonDevice {
public:
    explicit SDLAxisButton(std::shared_ptr<SDLJoystick> joystick_, int axis_, float threshold_,
                           bool trigger_if_greater_)
        : joystick(std::move(joystick_)), axis(axis_), threshold(threshold_),
          trigger_if_greater(trigger_if_greater_) {}

    bool GetStatus() const override {
        float axis_value = joystick->GetAxis(axis);
        if (trigger_if_greater)
            return axis_value > threshold;
        return axis_value < threshold;
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int axis;
    float threshold;
    bool trigger_if_greater;
};

class SDLAnalog final : public Input::AnalogDevice {
public:
    SDLAnalog(std::shared_ptr<SDLJoystick> joystick_, int axis_x_, int axis_y_, float deadzone_)
        : joystick(std::move(joystick_)), axis_x(axis_x_), axis_y(axis_y_), deadzone(deadzone_) {}

    std::tuple<float, float> GetStatus() const override {
        const auto [x, y] = joystick->GetAnalog(axis_x, axis_y);
        const float r = std::sqrt((x * x) + (y * y));
        if (r > deadzone) {
            return std::make_tuple(x / r * (r - deadzone) / (1 - deadzone),
                                   y / r * (r - deadzone) / (1 - deadzone));
        }
        return std::make_tuple<float, float>(0.0f, 0.0f);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    const int axis_x;
    const int axis_y;
    const float deadzone;
};

class SDLMotion final : public Input::MotionDevice {
public:
    explicit SDLMotion(std::shared_ptr<SDLJoystick> joystick_) : joystick(std::move(joystick_)) {}

    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetStatus() const override {
        return joystick->GetMotion();
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
};

/// A button device factory that creates button devices from SDL joystick
class SDLButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    explicit SDLButtonFactory(SDLState& state_) : state(state_) {}

    /**
     * Creates a button device from a joystick button
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type to bind
     *     - "button"(optional): the index of the button to bind
     *     - "hat"(optional): the index of the hat to bind as direction buttons
     *     - "axis"(optional): the index of the axis to bind
     *     - "direction"(only used for hat): the direction name of the hat to bind. Can be "up",
     *         "down", "left" or "right"
     *     - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
     *         triggered if the axis value crosses
     *     - "direction"(only used for axis): "+" means the button is triggered when the axis
     * value is greater than the threshold; "-" means the button is triggered when the axis
     * value is smaller than the threshold
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);

        auto joystick = state.GetSDLJoystickByGUID(guid, port);

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
            // This is necessary so accessing GetHat with hat won't crash
            joystick->SetHat(hat, SDL_HAT_CENTERED);
            return std::make_unique<SDLDirectionButton>(joystick, hat, direction);
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
            // This is necessary so accessing GetAxis with axis won't crash
            joystick->SetAxis(axis, 0);
            return std::make_unique<SDLAxisButton>(joystick, axis, threshold, trigger_if_greater);
        }

        const int button = params.Get("button", 0);
        // This is necessary so accessing GetButton with button won't crash
        joystick->SetButton(button, false);
        return std::make_unique<SDLButton>(joystick, button);
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
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type
     *     - "axis_x": the index of the axis to be bind as x-axis
     *     - "axis_y": the index of the axis to be bind as y-axis
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);
        const int axis_x = params.Get("axis_x", 0);
        const int axis_y = params.Get("axis_y", 1);
        float deadzone = std::clamp(params.Get("deadzone", 0.0f), 0.0f, .99f);

        auto joystick = state.GetSDLJoystickByGUID(guid, port);

        // This is necessary so accessing GetAxis with axis_x and axis_y won't crash
        joystick->SetAxis(axis_x, 0);
        joystick->SetAxis(axis_y, 0);
        return std::make_unique<SDLAnalog>(joystick, axis_x, axis_y, deadzone);
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

        auto joystick = state.GetSDLJoystickByGUID(guid, port);

        return std::make_unique<SDLMotion>(joystick);
    }

private:
    SDLState& state;
};

SDLState::SDLState() {
    using namespace Input;
    RegisterFactory<ButtonDevice>("sdl", std::make_shared<SDLButtonFactory>(*this));
    RegisterFactory<AnalogDevice>("sdl", std::make_shared<SDLAnalogFactory>(*this));
    RegisterFactory<MotionDevice>("sdl", std::make_shared<SDLMotionFactory>(*this));

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
    // Because the events for joystick connection happens before we have our event watcher added, we
    // can just open all the joysticks right here
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        InitJoystick(i);
    }
}

SDLState::~SDLState() {
    using namespace Input;
    UnregisterFactory<ButtonDevice>("sdl");
    UnregisterFactory<AnalogDevice>("sdl");
    UnregisterFactory<MotionDevice>("sdl");

    CloseJoysticks();
    SDL_DelEventWatch(&SDLEventWatcher, this);

    initialized = false;
    if (start_thread) {
        poll_thread.join();
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    }
}

Common::ParamPackage SDLEventToButtonParamPackage(SDLState& state, const SDL_Event& event) {
    Common::ParamPackage params({{"engine", "sdl"}});

    switch (event.type) {
    case SDL_JOYAXISMOTION: {
        auto joystick = state.GetSDLJoystickBySDLID(event.jaxis.which);
        params.Set("port", joystick->GetPort());
        params.Set("guid", joystick->GetGUID());
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
    case SDL_JOYBUTTONUP: {
        auto joystick = state.GetSDLJoystickBySDLID(event.jbutton.which);
        params.Set("port", joystick->GetPort());
        params.Set("guid", joystick->GetGUID());
        params.Set("button", event.jbutton.button);
        break;
    }
    case SDL_JOYHATMOTION: {
        auto joystick = state.GetSDLJoystickBySDLID(event.jhat.which);
        params.Set("port", joystick->GetPort());
        params.Set("guid", joystick->GetGUID());
        params.Set("hat", event.jhat.hat);
        switch (event.jhat.value) {
        case SDL_HAT_UP:
            params.Set("direction", "up");
            break;
        case SDL_HAT_DOWN:
            params.Set("direction", "down");
            break;
        case SDL_HAT_LEFT:
            params.Set("direction", "left");
            break;
        case SDL_HAT_RIGHT:
            params.Set("direction", "right");
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
        state.polling = false;
    }

protected:
    SDLState& state;
};

class SDLButtonPoller final : public SDLPoller {
public:
    explicit SDLButtonPoller(SDLState& state_) : SDLPoller(state_) {}

    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (state.event_queue.Pop(event)) {
            switch (event.type) {
            case SDL_JOYAXISMOTION:
                if (!axis_memory.count(event.jaxis.which) ||
                    !axis_memory[event.jaxis.which].count(event.jaxis.axis)) {
                    axis_memory[event.jaxis.which][event.jaxis.axis] = event.jaxis.value;
                    axis_event_count[event.jaxis.which][event.jaxis.axis] = 1;
                    break;
                } else {
                    axis_event_count[event.jaxis.which][event.jaxis.axis]++;
                    // The joystick and axis exist in our map if we take this branch, so no checks
                    // needed
                    if (std::abs(
                            (event.jaxis.value - axis_memory[event.jaxis.which][event.jaxis.axis]) /
                            32767.0) < 0.5) {
                        break;
                    } else {
                        if (axis_event_count[event.jaxis.which][event.jaxis.axis] == 2 &&
                            IsAxisAtPole(event.jaxis.value) &&
                            IsAxisAtPole(axis_memory[event.jaxis.which][event.jaxis.axis])) {
                            // If we have exactly two events and both are near a pole, this is
                            // likely a digital input masquerading as an analog axis; Instead of
                            // trying to look at the direction the axis travelled, assume the first
                            // event was press and the second was release; This should handle most
                            // digital axes while deferring to the direction of travel for analog
                            // axes
                            event.jaxis.value = static_cast<Sint16>(std::copysign(
                                32767, axis_memory[event.jaxis.which][event.jaxis.axis]));
                        } else {
                            // There are more than two events, so this is likely a true analog axis,
                            // check the direction it travelled
                            event.jaxis.value = static_cast<Sint16>(std::copysign(
                                32767, event.jaxis.value -
                                           axis_memory[event.jaxis.which][event.jaxis.axis]));
                        }
                        axis_memory.clear();
                        axis_event_count.clear();
                    }
                }
            case SDL_JOYBUTTONUP:
            case SDL_JOYHATMOTION:
                return SDLEventToButtonParamPackage(state, event);
            }
        }
        return {};
    }

private:
    // Determine whether an axis value is close to an extreme or center
    // Some controllers have a digital D-Pad as a pair of analog sticks, with 3 possible values per
    // axis, which is why the center must be considered a pole
    bool IsAxisAtPole(int16_t value) {
        return std::abs(value) >= 32767 || std::abs(value) < 327;
    }
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, int16_t>> axis_memory;
    std::unordered_map<SDL_JoystickID, std::unordered_map<uint8_t, uint32_t>> axis_event_count;
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
        while (state.event_queue.Pop(event)) {
            if (event.type != SDL_JOYAXISMOTION || std::abs(event.jaxis.value / 32767.0) < 0.5) {
                continue;
            }
            // An analog device needs two axes, so we need to store the axis for later and wait for
            // a second SDL event. The axes also must be from the same joystick.
            int axis = event.jaxis.axis;
            if (analog_xaxis == -1) {
                analog_xaxis = axis;
                analog_axes_joystick = event.jaxis.which;
            } else if (analog_yaxis == -1 && analog_xaxis != axis &&
                       analog_axes_joystick == event.jaxis.which) {
                analog_yaxis = axis;
            }
        }
        Common::ParamPackage params;
        if (analog_xaxis != -1 && analog_yaxis != -1) {
            auto joystick = state.GetSDLJoystickBySDLID(event.jaxis.which);
            params.Set("engine", "sdl");
            params.Set("port", joystick->GetPort());
            params.Set("guid", joystick->GetGUID());
            params.Set("axis_x", analog_xaxis);
            params.Set("axis_y", analog_yaxis);
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
    }

    return pollers;
}

} // namespace SDL
} // namespace InputCommon
