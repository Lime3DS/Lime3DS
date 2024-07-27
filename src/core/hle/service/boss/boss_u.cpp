// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/boss/boss_u.h"

namespace Service::BOSS {

BOSS_U::BOSS_U(std::shared_ptr<Module> boss)
    : Module::Interface(std::move(boss), "boss:U", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &BOSS_U::InitializeSession, "InitializeSession"},
        {0x0002, &BOSS_U::SetStorageInfo, "SetStorageInfo"},
        {0x0003, &BOSS_U::UnregisterStorage, "UnregisterStorage"},
        {0x0004, &BOSS_U::GetStorageInfo, "GetStorageInfo"},
        {0x0005, &BOSS_U::RegisterPrivateRootCa, "RegisterPrivateRootCa"},
        {0x0006, &BOSS_U::RegisterPrivateClientCert, "RegisterPrivateClientCert"},
        {0x0007, &BOSS_U::GetNewArrivalFlag, "GetNewArrivalFlag"},
        {0x0008, &BOSS_U::RegisterNewArrivalEvent, "RegisterNewArrivalEvent"},
        {0x0009, &BOSS_U::SetOptoutFlag, "SetOptoutFlag"},
        {0x000A, &BOSS_U::GetOptoutFlag, "GetOptoutFlag"},
        {0x000B, &BOSS_U::RegisterTask, "RegisterTask"},
        {0x000C, &BOSS_U::UnregisterTask, "UnregisterTask"},
        {0x000D, &BOSS_U::ReconfigureTask, "ReconfigureTask"},
        {0x000E, &BOSS_U::GetTaskIdList, "GetTaskIdList"},
        {0x000F, &BOSS_U::GetStepIdList, "GetStepIdList"},
        {0x0010, &BOSS_U::GetNsDataIdList, "GetNsDataIdList"},
        {0x0011, &BOSS_U::GetNsDataIdList1, "GetNsDataIdList1"},
        {0x0012, &BOSS_U::GetNsDataIdList2, "GetNsDataIdList2"},
        {0x0013, &BOSS_U::GetNsDataIdList3, "GetNsDataIdList3"},
        {0x0014, &BOSS_U::SendProperty, "SendProperty"},
        {0x0015, &BOSS_U::SendPropertyHandle, "SendPropertyHandle"},
        {0x0016, &BOSS_U::ReceiveProperty, "ReceiveProperty"},
        {0x0017, &BOSS_U::UpdateTaskInterval, "UpdateTaskInterval"},
        {0x0018, &BOSS_U::UpdateTaskCount, "UpdateTaskCount"},
        {0x0019, &BOSS_U::GetTaskInterval, "GetTaskInterval"},
        {0x001A, &BOSS_U::GetTaskCount, "GetTaskCount"},
        {0x001B, &BOSS_U::GetTaskServiceStatus, "GetTaskServiceStatus"},
        {0x001C, &BOSS_U::StartTask, "StartTask"},
        {0x001D, &BOSS_U::StartTaskImmediate, "StartTaskImmediate"},
        {0x001E, &BOSS_U::CancelTask, "CancelTask"},
        {0x001F, &BOSS_U::GetTaskFinishHandle, "GetTaskFinishHandle"},
        {0x0020, &BOSS_U::GetTaskState, "GetTaskState"},
        {0x0021, &BOSS_U::GetTaskResult, "GetTaskResult"},
        {0x0022, &BOSS_U::GetTaskCommErrorCode, "GetTaskCommErrorCode"},
        {0x0023, &BOSS_U::GetTaskStatus, "GetTaskStatus"},
        {0x0024, &BOSS_U::GetTaskError, "GetTaskError"},
        {0x0025, &BOSS_U::GetTaskInfo, "GetTaskInfo"},
        {0x0026, &BOSS_U::DeleteNsData, "DeleteNsData"},
        {0x0027, &BOSS_U::GetNsDataHeaderInfo, "GetNsDataHeaderInfo"},
        {0x0028, &BOSS_U::ReadNsData, "ReadNsData"},
        {0x0029, &BOSS_U::SetNsDataAdditionalInfo, "SetNsDataAdditionalInfo"},
        {0x002A, &BOSS_U::GetNsDataAdditionalInfo, "GetNsDataAdditionalInfo"},
        {0x002B, &BOSS_U::SetNsDataNewFlag, "SetNsDataNewFlag"},
        {0x002C, &BOSS_U::GetNsDataNewFlag, "GetNsDataNewFlag"},
        {0x002D, &BOSS_U::GetNsDataLastUpdate, "GetNsDataLastUpdate"},
        {0x002E, &BOSS_U::GetErrorCode, "GetErrorCode"},
        {0x002F, &BOSS_U::RegisterStorageEntry, "RegisterStorageEntry"},
        {0x0030, &BOSS_U::GetStorageEntryInfo, "GetStorageEntryInfo"},
        {0x0031, &BOSS_U::SetStorageOption, "SetStorageOption"},
        {0x0032, &BOSS_U::GetStorageOption, "GetStorageOption"},
        {0x0033, &BOSS_U::StartBgImmediate, "StartBgImmediate"},
        {0x0034, &BOSS_U::GetTaskProperty0, "GetTaskProperty0"},
        {0x0035, &BOSS_U::RegisterImmediateTask, "RegisterImmediateTask"},
        {0x0036, &BOSS_U::SetTaskQuery, "SetTaskQuery"},
        {0x0037, &BOSS_U::GetTaskQuery, "GetTaskQuery"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::BOSS

SERIALIZE_EXPORT_IMPL(Service::BOSS::BOSS_U)
