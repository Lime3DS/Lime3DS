// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nim/nim_s.h"

SERIALIZE_EXPORT_IMPL(Service::NIM::NIM_S)

namespace Service::NIM {

NIM_S::NIM_S() : ServiceFramework("nim:s", 1) {
    const FunctionInfo functions[] = {
        // clang-format off
        {0x000A, nullptr, "CheckSysupdateAvailableSOAP"},
        {0x0016, nullptr, "ListTitles"},
        {0x0029, nullptr, "AccountCheckBalanceSOAP"},
        {0x002D, nullptr, "DownloadTickets"},
        {0x0042, nullptr, "StartDownload"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

NIM_S::~NIM_S() = default;

} // namespace Service::NIM
