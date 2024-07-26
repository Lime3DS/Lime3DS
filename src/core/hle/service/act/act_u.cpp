// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/act/act_u.h"

namespace Service::ACT {

ACT_U::ACT_U(std::shared_ptr<Module> act) : Module::Interface(std::move(act), "act:u") {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &ACT_U::Initialize, "Initialize"},
        {0x0002, nullptr, "GetErrorCode"},
        {0x0006, &ACT_U::GetAccountDataBlock, "GetAccountDataBlock"},
        {0x000B, nullptr, "AcquireEulaList"},
        {0x000D, nullptr, "GenerateUuid"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::ACT

SERIALIZE_EXPORT_IMPL(Service::ACT::ACT_U)
