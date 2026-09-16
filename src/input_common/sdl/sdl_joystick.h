// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include "common/vector_math.h"
#include "input_common/sdl/sdl.h"

union SDL_Event;
using SDL_Joystick = struct _SDL_Joystick;
using SDL_JoystickID = s32;
using SDL_GameController = struct _SDL_GameController;

namespace InputCommon::SDL {
class SDLJoystick {
public:
    SDLJoystick(std::string guid_, int port_, SDL_Joystick* joystick,
                SDL_GameController* game_controller);

    bool IsButtonMappedToController(int button) const;

    void EnableMotion();
    bool HasMotion() const;

    bool GetButton(int button, bool isController) const;
    float GetAxis(int axis, bool isController) const;
    std::tuple<float, float> GetAnalog(int axis_x, int axis_y, bool isController) const;
    bool GetHatDirection(int hat, uint8_t direction) const;

    void SetTouchpad(float x, float y, int touchpad, bool down);
    void SetAccel(const float x, const float y, const float z);
    void SetGyro(const float pitch, const float yaw, const float roll);

    std::tuple<Common::Vec3<float>, Common::Vec3<float>> GetMotion() const;

    /**
     * The guid of the joystick
     */
    const std::string& GetGUID() const;

    /**
     * The number of joystick from the same type that were connected before this joystick
     */
    int GetPort() const;

    std::tuple<float, float, float> GetTouch(int pad) const;

    SDL_Joystick* GetSDLJoystick() const;
    SDL_GameController* GetSDLGameController() const;

    void SetSDLJoystick(SDL_Joystick* joystick, SDL_GameController* controller);

    // Hat presses under the SDL Joystick API do not send press and release signals like
    // normal buttons, instead only sending directions up, down, left, right, and center.
    // Thus we need to store which hat direction was pressed so we can interpret the
    // center signal as a release for the appropriate direction.
    // the empty string means we do not have anything currently stored as the pressed direction.
    std::string current_hat_down = "";

private:
    struct State {
        Common::Vec3<float> accel;
        Common::Vec3<float> gyro;
        std::unordered_map<int, std::tuple<float, float, bool>> touchpad;
    } state;
    std::string guid;
    int port;
    bool has_gyro{false};
    bool has_accel{false};
    std::unique_ptr<SDL_Joystick, void (*)(SDL_Joystick*)> sdl_joystick;
    std::unique_ptr<SDL_GameController, void (*)(SDL_GameController*)> sdl_controller;
    std::unordered_map<int, int16_t> joystick_axis_centers;
    mutable std::mutex mutex;
    std::unordered_set<int> mapped_joystick_buttons;
    void CreateControllerButtonMap();
    void CalibrateJoystickAxes();
};
} // namespace InputCommon::SDL