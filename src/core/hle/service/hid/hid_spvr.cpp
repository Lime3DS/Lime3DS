// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/hid/hid_spvr.h"

SERIALIZE_EXPORT_IMPL(Service::HID::Spvr)

namespace Service::HID {

Spvr::Spvr(std::shared_ptr<Module> hid) : Module::Interface(std::move(hid), "hid:SPVR", 6) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "CalibrateTouchScreen"},
        {0x0002, nullptr, "UpdateTouchConfig"},
        {0x000A, &Spvr::GetIPCHandles, "GetIPCHandles"},
        {0x000B, nullptr, "StartAnalogStickCalibration"},
        {0x000E, nullptr, "GetAnalogStickCalibrateParam"},
        {0x0011, &Spvr::EnableAccelerometer, "EnableAccelerometer"},
        {0x0012, &Spvr::DisableAccelerometer, "DisableAccelerometer"},
        {0x0013, &Spvr::EnableGyroscopeLow, "EnableGyroscopeLow"},
        {0x0014, &Spvr::DisableGyroscopeLow, "DisableGyroscopeLow"},
        {0x0015, &Spvr::GetGyroscopeLowRawToDpsCoefficient, "GetGyroscopeLowRawToDpsCoefficient"},
        {0x0016, &Spvr::GetGyroscopeLowCalibrateParam, "GetGyroscopeLowCalibrateParam"},
        {0x0017, &Spvr::GetSoundVolume, "GetSoundVolume"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::HID
