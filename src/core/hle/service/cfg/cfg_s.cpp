// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_s.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_S)

namespace Service::CFG {

CFG_S::CFG_S(std::shared_ptr<Module> cfg) : Module::Interface(std::move(cfg), "cfg:s", 23) {
    static const FunctionInfo functions[] = {
        // cfg common
        // clang-format off
        {0x0001, &CFG_S::GetConfig, "GetConfig"},
        {0x0002, &CFG_S::GetRegion, "GetRegion"},
        {0x0003, &CFG_S::GetTransferableId, "GetTransferableId"},
        {0x0004, &CFG_S::IsCoppacsSupported, "IsCoppacsSupported"},
        {0x0005, &CFG_S::GetSystemModel, "GetSystemModel"},
        {0x0006, &CFG_S::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {0x0007, nullptr, "WriteToFirstByteCfgSavegame"},
        {0x0008, nullptr, "TranslateCountryInfo"},
        {0x0009, &CFG_S::GetCountryCodeString, "GetCountryCodeString"},
        {0x000A, &CFG_S::GetCountryCodeID, "GetCountryCodeID"},
        {0x000B, nullptr, "IsFangateSupported"},
        // cfg:s
        {0x0401, &CFG_S::GetSystemConfig, "GetSystemConfig"},
        {0x0402, &CFG_S::SetSystemConfig, "SetSystemConfig"},
        {0x0403, &CFG_S::UpdateConfigNANDSavegame, "UpdateConfigNANDSavegame"},
        {0x0404, &CFG_S::GetLocalFriendCodeSeedData, "GetLocalFriendCodeSeedData"},
        {0x0405, &CFG_S::GetLocalFriendCodeSeed, "GetLocalFriendCodeSeed"},
        {0x0406, &CFG_S::GetRegion, "GetRegion"},
        {0x0407, &CFG_S::SecureInfoGetByte101, "SecureInfoGetByte101"},
        {0x0408, &CFG_S::SecureInfoGetSerialNo, "SecureInfoGetSerialNo"},
        {0x0409, nullptr, "UpdateConfigBlk00040003"},
        {0x040D, &CFG_S::SetUUIDClockSequence, "SetUUIDClockSequence"},
        {0x040E, &CFG_S::GetUUIDClockSequence, "GetUUIDClockSequence"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
