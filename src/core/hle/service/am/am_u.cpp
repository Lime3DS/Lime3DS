// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/am/am_u.h"

namespace Service::AM {

AM_U::AM_U(std::shared_ptr<Module> am) : Module::Interface(std::move(am), "am:u", 5) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &AM_U::GetNumPrograms, "GetNumPrograms"},
        {0x0002, &AM_U::GetProgramList, "GetProgramList"},
        {0x0003, &AM_U::GetProgramInfos, "GetProgramInfos"},
        {0x0004, &AM_U::DeleteUserProgram, "DeleteUserProgram"},
        {0x0005, &AM_U::GetProductCode, "GetProductCode"},
        {0x0006, nullptr, "GetStorageId"},
        {0x0007, &AM_U::DeleteTicket, "DeleteTicket"},
        {0x0008, &AM_U::GetNumTickets, "GetNumTickets"},
        {0x0009, &AM_U::GetTicketList, "GetTicketList"},
        {0x000A, &AM_U::GetDeviceID, "GetDeviceID"},
        {0x000B, nullptr, "GetNumImportTitleContexts"},
        {0x000C, nullptr, "GetImportTitleContextList"},
        {0x000D, nullptr, "GetImportTitleContexts"},
        {0x000E, nullptr, "DeleteImportTitleContext"},
        {0x000F, nullptr, "GetNumImportContentContexts"},
        {0x0010, nullptr, "GetImportContentContextList"},
        {0x0011, nullptr, "GetImportContentContexts"},
        {0x0012, nullptr, "DeleteImportContentContexts"},
        {0x0013, &AM_U::NeedsCleanup, "NeedsCleanup"},
        {0x0014, nullptr, "DoCleanup"},
        {0x0015, nullptr, "DeleteAllImportContexts"},
        {0x0016, nullptr, "DeleteAllTemporaryPrograms"},
        {0x0017, nullptr, "ImportTwlBackupLegacy"},
        {0x0018, nullptr, "InitializeTitleDatabase"},
        {0x0019, nullptr, "QueryAvailableTitleDatabase"},
        {0x001A, nullptr, "CalcTwlBackupSize"},
        {0x001B, nullptr, "ExportTwlBackup"},
        {0x001C, nullptr, "ImportTwlBackup"},
        {0x001D, nullptr, "DeleteAllTwlUserPrograms"},
        {0x001E, nullptr, "ReadTwlBackupInfo"},
        {0x001F, nullptr, "DeleteAllExpiredUserPrograms"},
        {0x0020, nullptr, "GetTwlArchiveResourceInfo"},
        {0x0021, nullptr, "GetPersonalizedTicketInfoList"},
        {0x0022, nullptr, "DeleteAllImportContextsFiltered"},
        {0x0023, nullptr, "GetNumImportTitleContextsFiltered"},
        {0x0024, nullptr, "GetImportTitleContextListFiltered"},
        {0x0025, nullptr, "CheckContentRights"},
        {0x0026, nullptr, "GetTicketLimitInfos"},
        {0x0027, nullptr, "GetDemoLaunchInfos"},
        {0x0028, nullptr, "ReadTwlBackupInfoEx"},
        {0x0029, nullptr, "DeleteUserProgramsAtomically"},
        {0x002A, nullptr, "GetNumExistingContentInfosSystem"},
        {0x002B, nullptr, "ListExistingContentInfosSystem"},
        {0x002C, nullptr, "GetProgramInfosIgnorePlatform"},
        {0x002D, nullptr, "CheckContentRightsIgnorePlatform"},
        {0x0401, nullptr, "UpdateFirmwareTo"},
        {0x0402, &AM_U::BeginImportProgram, "BeginImportProgram"},
        {0x0403, &AM_U::BeginImportProgramTemporarily, "BeginImportProgramTemporarily"},
        {0x0404, nullptr, "CancelImportProgram"},
        {0x0405, &AM_U::EndImportProgram, "EndImportProgram"},
        {0x0406, &AM_U::EndImportProgramWithoutCommit, "EndImportProgramWithoutCommit"},
        {0x0407, &AM_U::CommitImportPrograms, "CommitImportPrograms"},
        {0x0408, &AM_U::GetProgramInfoFromCia, "GetProgramInfoFromCia"},
        {0x0409, &AM_U::GetSystemMenuDataFromCia, "GetSystemMenuDataFromCia"},
        {0x040A, &AM_U::GetDependencyListFromCia, "GetDependencyListFromCia"},
        {0x040B, &AM_U::GetTransferSizeFromCia, "GetTransferSizeFromCia"},
        {0x040C, &AM_U::GetCoreVersionFromCia, "GetCoreVersionFromCia"},
        {0x040D, &AM_U::GetRequiredSizeFromCia, "GetRequiredSizeFromCia"},
        {0x040E, nullptr, "CommitImportProgramsAndUpdateFirmwareAuto"},
        {0x040F, nullptr, "UpdateFirmwareAuto"},
        {0x0410, &AM_U::DeleteProgram, "DeleteProgram"},
        {0x0411, nullptr, "GetTwlProgramListForReboot"},
        {0x0412, &AM_U::GetSystemUpdaterMutex, "GetSystemUpdaterMutex"},
        {0x0413, &AM_U::GetMetaSizeFromCia, "GetMetaSizeFromCia"},
        {0x0414, &AM_U::GetMetaDataFromCia, "GetMetaDataFromCia"},
        {0x0415, nullptr, "CheckDemoLaunchRights"},
        {0x0416, nullptr, "GetInternalTitleLocationInfo"},
        {0x0417, nullptr, "PerpetuateAgbSaveData"},
        {0x0418, nullptr, "BeginImportProgramForOverWrite"},
        {0x0419, nullptr, "BeginImportSystemProgram"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::AM

SERIALIZE_EXPORT_IMPL(Service::AM::AM_U)
