// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/act/act_a.h"

namespace Service::ACT {

ACT_A::ACT_A(std::shared_ptr<Module> act) : Module::Interface(std::move(act), "act:a") {
    const FunctionInfo functions[] = {
        // act:u shared commands
        // clang-format off
        {0x0001, &ACT_A::Initialize, "Initialize"},
        {0x0002, nullptr, "GetErrorCode"},
        {0x0006, &ACT_A::GetAccountDataBlock, "GetAccountDataBlock"},
        {0x000B, nullptr, "AcquireEulaList"},
        {0x000D, nullptr, "GenerateUuid"},
        // act:a
        {0x0413, nullptr, "UpdateMiiImage"},
        {0x041B, nullptr, "AgreeEula"},
        {0x0421, nullptr, "UploadMii"},
        {0x0423, nullptr, "ValidateMailAddress"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::ACT

SERIALIZE_EXPORT_IMPL(Service::ACT::ACT_A)
