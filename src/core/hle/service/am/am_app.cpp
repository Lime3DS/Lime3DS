// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/am/am_app.h"

namespace Service::AM {

AM_APP::AM_APP(std::shared_ptr<Module> am) : Module::Interface(std::move(am), "am:app", 5) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x1001, &AM_APP::GetDLCContentInfoCount, "GetDLCContentInfoCount"},
        {0x1002, &AM_APP::FindDLCContentInfos, "FindDLCContentInfos"},
        {0x1003, &AM_APP::ListDLCContentInfos, "ListDLCContentInfos"},
        {0x1004, &AM_APP::DeleteContents, "DeleteContents"},
        {0x1005, &AM_APP::GetDLCTitleInfos, "GetDLCTitleInfos"},
        {0x1006, nullptr, "GetNumDataTitleTickets"},
        {0x1007, &AM_APP::ListDataTitleTicketInfos, "ListDataTitleTicketInfos"},
        {0x1008, nullptr, "GetItemRights"},
        {0x1009, nullptr, "IsDataTitleInUse"},
        {0x100A, nullptr, "IsExternalTitleDatabaseInitialized"},
        {0x100B, nullptr, "GetNumExistingContentInfos"},
        {0x100C, nullptr, "ListExistingContentInfos"},
        {0x100D, &AM_APP::GetPatchTitleInfos, "GetPatchTitleInfos"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::AM

SERIALIZE_EXPORT_IMPL(Service::AM::AM_APP)
