// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/hid/hid_user.h"

SERIALIZE_EXPORT_IMPL(Service::HID::User)

namespace Service::HID {

User::User(std::shared_ptr<Module> hid) : Module::Interface(std::move(hid), "hid:USER", 6) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "CalibrateTouchScreen"},
        {0x0002, nullptr, "UpdateTouchConfig"},
        {0x000A, &User::GetIPCHandles, "GetIPCHandles"},
        {0x000B, nullptr, "StartAnalogStickCalibration"},
        {0x000E, nullptr, "GetAnalogStickCalibrateParam"},
        {0x0011, &User::EnableAccelerometer, "EnableAccelerometer"},
        {0x0012, &User::DisableAccelerometer, "DisableAccelerometer"},
        {0x0013, &User::EnableGyroscopeLow, "EnableGyroscopeLow"},
        {0x0014, &User::DisableGyroscopeLow, "DisableGyroscopeLow"},
        {0x0015, &User::GetGyroscopeLowRawToDpsCoefficient, "GetGyroscopeLowRawToDpsCoefficient"},
        {0x0016, &User::GetGyroscopeLowCalibrateParam, "GetGyroscopeLowCalibrateParam"},
        {0x0017, &User::GetSoundVolume, "GetSoundVolume"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::HID
