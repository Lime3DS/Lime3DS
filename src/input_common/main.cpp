// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <thread>
#include "common/param_package.h"
#include "input_common/analog_from_button.h"
#ifdef ENABLE_GCADAPTER
#include "input_common/gcadapter/gc_adapter.h"
#include "input_common/gcadapter/gc_poller.h"
#endif
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "input_common/sdl/sdl.h"
#include "input_common/sdl/sdl_impl.h"
#include "input_common/touch_from_button.h"
#include "input_common/udp/udp.h"

namespace InputCommon {

#ifdef ENABLE_GCADAPTER
std::shared_ptr<GCButtonFactory> gcbuttons;
std::shared_ptr<GCAnalogFactory> gcanalog;
std::shared_ptr<GCAdapter::Adapter> gcadapter;
#endif
static std::shared_ptr<Keyboard> keyboard;
static std::shared_ptr<MotionEmu> motion_emu;
static std::unique_ptr<CemuhookUDP::State> udp;
static std::unique_ptr<SDL::State> sdl;

void Init() {
#ifdef ENABLE_GCADAPTER
    gcadapter = std::make_shared<GCAdapter::Adapter>();
    gcbuttons = std::make_shared<GCButtonFactory>(gcadapter);
    Input::RegisterFactory<Input::ButtonDevice>("gcpad", gcbuttons);
    gcanalog = std::make_shared<GCAnalogFactory>(gcadapter);
    Input::RegisterFactory<Input::AnalogDevice>("gcpad", gcanalog);
#endif
    keyboard = std::make_shared<Keyboard>();
    Input::RegisterFactory<Input::ButtonDevice>("keyboard", keyboard);
    Input::RegisterFactory<Input::AnalogDevice>("analog_from_button",
                                                std::make_shared<AnalogFromButton>());
    motion_emu = std::make_shared<MotionEmu>();
    Input::RegisterFactory<Input::MotionDevice>("motion_emu", motion_emu);
    Input::RegisterFactory<Input::TouchDevice>("touch_from_button",
                                               std::make_shared<TouchFromButtonFactory>());

    sdl = SDL::Init();

    udp = CemuhookUDP::Init();
}

void Shutdown() {
#ifdef ENABLE_GCADAPTER
    Input::UnregisterFactory<Input::ButtonDevice>("gcpad");
    Input::UnregisterFactory<Input::AnalogDevice>("gcpad");
    gcbuttons.reset();
    gcanalog.reset();
#endif
    Input::UnregisterFactory<Input::ButtonDevice>("keyboard");
    keyboard.reset();
    Input::UnregisterFactory<Input::AnalogDevice>("analog_from_button");
    Input::UnregisterFactory<Input::MotionDevice>("motion_emu");
    motion_emu.reset();
    Input::UnregisterFactory<Input::TouchDevice>("emu_window");
    Input::UnregisterFactory<Input::TouchDevice>("touch_from_button");
    sdl.reset();
    udp.reset();
}

Keyboard* GetKeyboard() {
    return keyboard.get();
}

MotionEmu* GetMotionEmu() {
    return motion_emu.get();
}

std::string GenerateKeyboardParam(int key_code) {
    Common::ParamPackage param{
        {"engine", "keyboard"},
        {"code", std::to_string(key_code)},
    };
    return param.Serialize();
}

std::string GenerateAnalogParamFromKeys(int key_up, int key_down, int key_left, int key_right,
                                        int key_modifier, float modifier_scale) {
    Common::ParamPackage circle_pad_param{
        {"engine", "analog_from_button"},
        {"up", GenerateKeyboardParam(key_up)},
        {"down", GenerateKeyboardParam(key_down)},
        {"left", GenerateKeyboardParam(key_left)},
        {"right", GenerateKeyboardParam(key_right)},
        {"modifier", GenerateKeyboardParam(key_modifier)},
        {"modifier_scale", std::to_string(modifier_scale)},
    };
    return circle_pad_param.Serialize();
}

Common::ParamPackage GetControllerButtonBinds(const Common::ParamPackage& params, int button) {
    const auto native_button{static_cast<Settings::NativeButton::Values>(button)};
    const auto engine{params.Get("engine", "")};
    if (engine == "sdl") {
        return dynamic_cast<SDL::SDLState*>(sdl.get())->GetSDLControllerButtonBindByGUID(
            params.Get("guid", "0"), params.Get("port", 0), native_button);
    }
#ifdef ENABLE_GCADAPTER
    if (engine == "gcpad") {
        return gcbuttons->GetGcTo3DSMappedButton(params.Get("port", 0), native_button);
    }
#endif
    return {};
}

Common::ParamPackage GetControllerAnalogBinds(const Common::ParamPackage& params, int analog) {
    const auto native_analog{static_cast<Settings::NativeAnalog::Values>(analog)};
    const auto engine{params.Get("engine", "")};
    if (engine == "sdl") {
        return dynamic_cast<SDL::SDLState*>(sdl.get())->GetSDLControllerAnalogBindByGUID(
            params.Get("guid", "0"), params.Get("port", 0), native_analog);
    }
#ifdef ENABLE_GCADAPTER
    if (engine == "gcpad") {
        return gcanalog->GetGcTo3DSMappedAnalog(params.Get("port", 0), native_analog);
    }
#endif
    return {};
}

void ReloadInputDevices() {
    if (!udp) {
        return;
    }
    udp->ReloadUDPClient();
}

namespace Polling {

std::vector<std::unique_ptr<DevicePoller>> GetPollers(DeviceType type) {
    std::vector<std::unique_ptr<DevicePoller>> pollers;

#ifdef HAVE_SDL2
    pollers = sdl->GetPollers(type);
#endif
#ifdef ENABLE_GCADAPTER
    switch (type) {
    case DeviceType::Analog:
        pollers.push_back(std::make_unique<GCAnalogFactory>(*gcanalog));
        break;
    case DeviceType::Button:
        pollers.push_back(std::make_unique<GCButtonFactory>(*gcbuttons));
        break;
    default:
        break;
    }
#endif

    return pollers;
}

} // namespace Polling
} // namespace InputCommon
