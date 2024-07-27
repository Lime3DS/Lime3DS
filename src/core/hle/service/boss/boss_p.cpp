// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/boss/boss_p.h"

namespace Service::BOSS {

BOSS_P::BOSS_P(std::shared_ptr<Module> boss)
    : Module::Interface(std::move(boss), "boss:P", DefaultMaxSessions) {
    static const FunctionInfo functions[] = {
        // boss:u shared commands
        // clang-format off
        {0x0001, &BOSS_P::InitializeSession, "InitializeSession"},
        {0x0002, &BOSS_P::SetStorageInfo, "RegisterStorage"},
        {0x0003, &BOSS_P::UnregisterStorage, "UnregisterStorage"},
        {0x0004, &BOSS_P::GetStorageInfo, "GetStorageInfo"},
        {0x0005, &BOSS_P::RegisterPrivateRootCa, "RegisterPrivateRootCa"},
        {0x0006, &BOSS_P::RegisterPrivateClientCert, "RegisterPrivateClientCert"},
        {0x0007, &BOSS_P::GetNewArrivalFlag, "GetNewArrivalFlag"},
        {0x0008, &BOSS_P::RegisterNewArrivalEvent, "RegisterNewArrivalEvent"},
        {0x0009, &BOSS_P::SetOptoutFlag, "SetOptoutFlag"},
        {0x000A, &BOSS_P::GetOptoutFlag, "GetOptoutFlag"},
        {0x000B, &BOSS_P::RegisterTask, "RegisterTask"},
        {0x000C, &BOSS_P::UnregisterTask, "UnregisterTask"},
        {0x000D, &BOSS_P::ReconfigureTask, "ReconfigureTask"},
        {0x000E, &BOSS_P::GetTaskIdList, "GetTaskIdList"},
        {0x000F, &BOSS_P::GetStepIdList, "GetStepIdList"},
        {0x0010, &BOSS_P::GetNsDataIdList, "GetNsDataIdList"},
        {0x0011, &BOSS_P::GetNsDataIdList1, "GetNsDataIdList1"},
        {0x0012, &BOSS_P::GetNsDataIdList2, "GetNsDataIdList2"},
        {0x0013, &BOSS_P::GetNsDataIdList3, "GetNsDataIdList3"},
        {0x0014, &BOSS_P::SendProperty, "SendProperty"},
        {0x0015, &BOSS_P::SendPropertyHandle, "SendPropertyHandle"},
        {0x0016, &BOSS_P::ReceiveProperty, "ReceiveProperty"},
        {0x0017, &BOSS_P::UpdateTaskInterval, "UpdateTaskInterval"},
        {0x0018, &BOSS_P::UpdateTaskCount, "UpdateTaskCount"},
        {0x0019, &BOSS_P::GetTaskInterval, "GetTaskInterval"},
        {0x001A, &BOSS_P::GetTaskCount, "GetTaskCount"},
        {0x001B, &BOSS_P::GetTaskServiceStatus, "GetTaskServiceStatus"},
        {0x001C, &BOSS_P::StartTask, "StartTask"},
        {0x001D, &BOSS_P::StartTaskImmediate, "StartTaskImmediate"},
        {0x001E, &BOSS_P::CancelTask, "CancelTask"},
        {0x001F, &BOSS_P::GetTaskFinishHandle, "GetTaskFinishHandle"},
        {0x0020, &BOSS_P::GetTaskState, "GetTaskState"},
        {0x0021, &BOSS_P::GetTaskResult, "GetTaskResult"},
        {0x0022, &BOSS_P::GetTaskCommErrorCode, "GetTaskCommErrorCode"},
        {0x0023, &BOSS_P::GetTaskStatus, "GetTaskStatus"},
        {0x0024, &BOSS_P::GetTaskError, "GetTaskError"},
        {0x0025, &BOSS_P::GetTaskInfo, "GetTaskInfo"},
        {0x0026, &BOSS_P::DeleteNsData, "DeleteNsData"},
        {0x0027, &BOSS_P::GetNsDataHeaderInfo, "GetNsDataHeaderInfo"},
        {0x0028, &BOSS_P::ReadNsData, "ReadNsData"},
        {0x0029, &BOSS_P::SetNsDataAdditionalInfo, "SetNsDataAdditionalInfo"},
        {0x002A, &BOSS_P::GetNsDataAdditionalInfo, "GetNsDataAdditionalInfo"},
        {0x002B, &BOSS_P::SetNsDataNewFlag, "SetNsDataNewFlag"},
        {0x002C, &BOSS_P::GetNsDataNewFlag, "GetNsDataNewFlag"},
        {0x002D, &BOSS_P::GetNsDataLastUpdate, "GetNsDataLastUpdate"},
        {0x002E, &BOSS_P::GetErrorCode, "GetErrorCode"},
        {0x002F, &BOSS_P::RegisterStorageEntry, "RegisterStorageEntry"},
        {0x0030, &BOSS_P::GetStorageEntryInfo, "GetStorageEntryInfo"},
        {0x0031, &BOSS_P::SetStorageOption, "SetStorageOption"},
        {0x0032, &BOSS_P::GetStorageOption, "GetStorageOption"},
        {0x0033, &BOSS_P::StartBgImmediate, "StartBgImmediate"},
        {0x0034, &BOSS_P::GetTaskProperty0, "GetTaskProperty0"},
        {0x0035, &BOSS_P::RegisterImmediateTask, "RegisterImmediateTask"},
        {0x0036, &BOSS_P::SetTaskQuery, "SetTaskQuery"},
        {0x0037, &BOSS_P::GetTaskQuery, "GetTaskQuery"},
        // boss:p
        {0x0401, &BOSS_P::InitializeSessionPrivileged, "InitializeSessionPrivileged"},
        {0x0404, &BOSS_P::GetAppNewFlag, "GetAppNewFlag"},
        {0x040D, &BOSS_P::GetNsDataIdListPrivileged, "GetNsDataIdListPrivileged"},
        {0x040E, &BOSS_P::GetNsDataIdListPrivileged1, "GetNsDataIdListPrivileged1"},
        {0x0413, &BOSS_P::SendPropertyPrivileged, "SendPropertyPrivileged"},
        {0x0415, &BOSS_P::DeleteNsDataPrivileged, "DeleteNsDataPrivileged"},
        {0x0416, &BOSS_P::GetNsDataHeaderInfoPrivileged, "GetNsDataHeaderInfoPrivileged"},
        {0x0417, &BOSS_P::ReadNsDataPrivileged, "ReadNsDataPrivileged"},
        {0x041A, &BOSS_P::SetNsDataNewFlagPrivileged, "SetNsDataNewFlagPrivileged"},
        {0x041B, &BOSS_P::GetNsDataNewFlagPrivileged, "GetNsDataNewFlagPrivileged"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

} // namespace Service::BOSS

SERIALIZE_EXPORT_IMPL(Service::BOSS::BOSS_P)
