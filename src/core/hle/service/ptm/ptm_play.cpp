// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ptm/ptm_play.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_Play)

namespace Service::PTM {

PTM_Play::PTM_Play(std::shared_ptr<Module> ptm)
    : Module::Interface(std::move(ptm), "ptm:play", 26) {
    static const FunctionInfo functions[] = {
        // ptm:u common commands
        // clang-format off
        {0x0001, nullptr, "RegisterAlarmClient"},
        {0x0002, nullptr, "SetRtcAlarm"},
        {0x0003, nullptr, "GetRtcAlarm"},
        {0x0004, nullptr, "CancelRtcAlarm"},
        {0x0005, &PTM_Play::GetAdapterState, "GetAdapterState"},
        {0x0006, &PTM_Play::GetShellState, "GetShellState"},
        {0x0007, &PTM_Play::GetBatteryLevel, "GetBatteryLevel"},
        {0x0008, &PTM_Play::GetBatteryChargeState, "GetBatteryChargeState"},
        {0x0009, nullptr, "GetPedometerState"},
        {0x000A, nullptr, "GetStepHistoryEntry"},
        {0x000B, &PTM_Play::GetStepHistory, "GetStepHistory"},
        {0x000C, &PTM_Play::GetTotalStepCount, "GetTotalStepCount"},
        {0x000D, nullptr, "SetPedometerRecordingMode"},
        {0x000E, nullptr, "GetPedometerRecordingMode"},
        {0x000F, nullptr, "GetStepHistoryAll"},
        // ptm:play
        {0x0807, nullptr, "GetPlayHistory"},
        {0x0808, nullptr, "GetPlayHistoryStart"},
        {0x0809, nullptr, "GetPlayHistoryLength"},
        {0x080B, nullptr, "CalcPlayHistoryStart"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::PTM
