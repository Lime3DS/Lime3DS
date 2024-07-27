// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_u.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_U)

namespace Service::CFG {

CFG_U::CFG_U(std::shared_ptr<Module> cfg) : Module::Interface(std::move(cfg), "cfg:u", 23) {
    static const FunctionInfo functions[] = {
        // cfg common
        // clang-format off
        {0x0001, &CFG_U::GetConfig, "GetConfig"},
        {0x0002, &CFG_U::GetRegion, "GetRegion"},
        {0x0003, &CFG_U::GetTransferableId, "GetTransferableId"},
        {0x0004, &CFG_U::IsCoppacsSupported, "IsCoppacsSupported"},
        {0x0005, &CFG_U::GetSystemModel, "GetSystemModel"},
        {0x0006, &CFG_U::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {0x0007, nullptr, "WriteToFirstByteCfgSavegame"},
        {0x0008, nullptr, "TranslateCountryInfo"},
        {0x0009, &CFG_U::GetCountryCodeString, "GetCountryCodeString"},
        {0x000A, &CFG_U::GetCountryCodeID, "GetCountryCodeID"},
        {0x000B, nullptr, "IsFangateSupported"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
