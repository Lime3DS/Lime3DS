// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ptm/ptm_sysm.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_S)
SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_Sysm)

namespace Service::PTM {

PTM_S_Common::PTM_S_Common(std::shared_ptr<Module> ptm, const char* name)
    : Module::Interface(std::move(ptm), name, 26) {
    static const FunctionInfo functions[] = {
        // ptm:u common commands
        // clang-format off
        {0x0001, nullptr, "RegisterAlarmClient"},
        {0x0002, nullptr, "SetRtcAlarm"},
        {0x0003, nullptr, "GetRtcAlarm"},
        {0x0004, nullptr, "CancelRtcAlarm"},
        {0x0005, &PTM_S_Common::GetAdapterState, "GetAdapterState"},
        {0x0006, &PTM_S_Common::GetShellState, "GetShellState"},
        {0x0007, &PTM_S_Common::GetBatteryLevel, "GetBatteryLevel"},
        {0x0008, &PTM_S_Common::GetBatteryChargeState, "GetBatteryChargeState"},
        {0x0009, nullptr, "GetPedometerState"},
        {0x000A, nullptr, "GetStepHistoryEntry"},
        {0x000B, &PTM_S_Common::GetStepHistory, "GetStepHistory"},
        {0x000C, &PTM_S_Common::GetTotalStepCount, "GetTotalStepCount"},
        {0x000D, nullptr, "SetPedometerRecordingMode"},
        {0x000E, nullptr, "GetPedometerRecordingMode"},
        {0x000F, nullptr, "GetStepHistoryAll"},
        // ptm:sysm & ptm:s
        {0x0401, nullptr, "SetRtcAlarmEx"},
        {0x0402, nullptr, "ReplySleepQuery"},
        {0x0403, nullptr, "NotifySleepPreparationComplete"},
        {0x0404, nullptr, "SetWakeupTrigger"},
        {0x0405, nullptr, "GetAwakeReason"},
        {0x0406, nullptr, "RequestSleep"},
        {0x0407, nullptr, "ShutdownAsync"},
        {0x0408, nullptr, "Awake"},
        {0x0409, nullptr, "RebootAsync"},
        {0x040A, &PTM_S_Common::CheckNew3DS, "CheckNew3DS"},
        {0x0801, nullptr, "SetInfoLEDPattern"},
        {0x0802, nullptr, "SetInfoLEDPatternHeader"},
        {0x0803, nullptr, "GetInfoLEDStatus"},
        {0x0804, nullptr, "SetBatteryEmptyLEDPattern"},
        {0x0805, nullptr, "ClearStepHistory"},
        {0x0806, nullptr, "SetStepHistory"},
        {0x0807, nullptr, "GetPlayHistory"},
        {0x0808, nullptr, "GetPlayHistoryStart"},
        {0x0809, nullptr, "GetPlayHistoryLength"},
        {0x080A, nullptr, "ClearPlayHistory"},
        {0x080B, nullptr, "CalcPlayHistoryStart"},
        {0x080C, nullptr, "SetUserTime"},
        {0x080D, nullptr, "InvalidateSystemTime"},
        {0x080E, nullptr, "NotifyPlayEvent"},
        {0x080F, &PTM_S_Common::GetSoftwareClosedFlag, "GetSoftwareClosedFlag"},
        {0x0810, nullptr, "ClearSoftwareClosedFlag"},
        {0x0811, &PTM_S_Common::GetShellState, "GetShellState"},
        {0x0812, nullptr, "IsShutdownByBatteryEmpty"},
        {0x0813, nullptr, "FormatSavedata"},
        {0x0814, nullptr, "GetLegacyJumpProhibitedFlag"},
        {0x0818, &PTM_S_Common::ConfigureNew3DSCPU, "ConfigureNew3DSCPU"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

PTM_S::PTM_S(std::shared_ptr<Module> ptm) : PTM_S_Common(std::move(ptm), "ptm:s") {}

PTM_Sysm::PTM_Sysm(std::shared_ptr<Module> ptm) : PTM_S_Common(std::move(ptm), "ptm:sysm") {}

} // namespace Service::PTM
