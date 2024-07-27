// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ptm/ptm_u.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_U)

namespace Service::PTM {

PTM_U::PTM_U(std::shared_ptr<Module> ptm) : Module::Interface(std::move(ptm), "ptm:u", 26) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "RegisterAlarmClient"},
        {0x0002, nullptr, "SetRtcAlarm"},
        {0x0003, nullptr, "GetRtcAlarm"},
        {0x0004, nullptr, "CancelRtcAlarm"},
        {0x0005, &PTM_U::GetAdapterState, "GetAdapterState"},
        {0x0006, &PTM_U::GetShellState, "GetShellState"},
        {0x0007, &PTM_U::GetBatteryLevel, "GetBatteryLevel"},
        {0x0008, &PTM_U::GetBatteryChargeState, "GetBatteryChargeState"},
        {0x0009, &PTM_U::GetPedometerState, "GetPedometerState"},
        {0x000A, nullptr, "GetStepHistoryEntry"},
        {0x000B, &PTM_U::GetStepHistory, "GetStepHistory"},
        {0x000C, &PTM_U::GetTotalStepCount, "GetTotalStepCount"},
        {0x000D, nullptr, "SetPedometerRecordingMode"},
        {0x000E, nullptr, "GetPedometerRecordingMode"},
        {0x000F, nullptr, "GetStepHistoryAll"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::PTM
