// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ptm/ptm_gets.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_Gets)

namespace Service::PTM {

PTM_Gets::PTM_Gets(std::shared_ptr<Module> ptm)
    : Module::Interface(std::move(ptm), "ptm:gets", 26) {
    static const FunctionInfo functions[] = {
        // ptm:u common commands
        // clang-format off
        {0x0001, nullptr, "RegisterAlarmClient"},
        {0x0002, nullptr, "SetRtcAlarm"},
        {0x0003, nullptr, "GetRtcAlarm"},
        {0x0004, nullptr, "CancelRtcAlarm"},
        {0x0005, &PTM_Gets::GetAdapterState, "GetAdapterState"},
        {0x0006, &PTM_Gets::GetShellState, "GetShellState"},
        {0x0007, &PTM_Gets::GetBatteryLevel, "GetBatteryLevel"},
        {0x0008, &PTM_Gets::GetBatteryChargeState, "GetBatteryChargeState"},
        {0x0009, nullptr, "GetPedometerState"},
        {0x000A, nullptr, "GetStepHistoryEntry"},
        {0x000B, &PTM_Gets::GetStepHistory, "GetStepHistory"},
        {0x000C, &PTM_Gets::GetTotalStepCount, "GetTotalStepCount"},
        {0x000D, nullptr, "SetPedometerRecordingMode"},
        {0x000E, nullptr, "GetPedometerRecordingMode"},
        {0x000F, nullptr, "GetStepHistoryAll"},
        // ptm:gets
        {0x0401, &PTM_Gets::GetSystemTime, "GetSystemTime"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::PTM
