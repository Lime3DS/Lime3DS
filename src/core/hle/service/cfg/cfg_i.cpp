// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/cfg/cfg_i.h"

SERIALIZE_EXPORT_IMPL(Service::CFG::CFG_I)

namespace Service::CFG {

CFG_I::CFG_I(std::shared_ptr<Module> cfg) : Module::Interface(std::move(cfg), "cfg:i", 23) {
    static const FunctionInfo functions[] = {
        // cfg common
        // clang-format off
        {0x0001, &CFG_I::GetConfig, "GetConfig"},
        {0x0002, &CFG_I::GetRegion, "GetRegion"},
        {0x0003, &CFG_I::GetTransferableId, "GetTransferableId"},
        {0x0004, &CFG_I::IsCoppacsSupported, "IsCoppacsSupported"},
        {0x0005, &CFG_I::GetSystemModel, "GetSystemModel"},
        {0x0006, &CFG_I::GetModelNintendo2DS, "GetModelNintendo2DS"},
        {0x0007, nullptr, "WriteToFirstByteCfgSavegame"},
        {0x0008, nullptr, "TranslateCountryInfo"},
        {0x0009, &CFG_I::GetCountryCodeString, "GetCountryCodeString"},
        {0x000A, &CFG_I::GetCountryCodeID, "GetCountryCodeID"},
        {0x000B, nullptr, "IsFangateSupported"},
        // cfg:s
        {0x0401, &CFG_I::GetSystemConfig, "GetSystemConfig"},
        {0x0402, &CFG_I::SetSystemConfig, "SetSystemConfig"},
        {0x0403, &CFG_I::UpdateConfigNANDSavegame, "UpdateConfigNANDSavegame"},
        {0x0404, &CFG_I::GetLocalFriendCodeSeedData, "GetLocalFriendCodeSeedData"},
        {0x0405, &CFG_I::GetLocalFriendCodeSeed, "GetLocalFriendCodeSeed"},
        {0x0406, &CFG_I::GetRegion, "GetRegion"},
        {0x0407, &CFG_I::SecureInfoGetByte101, "SecureInfoGetByte101"},
        {0x0408, &CFG_I::SecureInfoGetSerialNo, "SecureInfoGetSerialNo"},
        {0x0409, nullptr, "UpdateConfigBlk00040003"},
        // cfg:i
        {0x0801, &CFG_I::GetSystemConfig, "GetSystemConfig"},
        {0x0802, &CFG_I::SetSystemConfig, "SetSystemConfig"},
        {0x0803, &CFG_I::UpdateConfigNANDSavegame, "UpdateConfigNANDSavegame"},
        {0x0804, nullptr, "CreateConfigInfoBlk"},
        {0x0805, nullptr, "DeleteConfigNANDSavefile"},
        {0x0806, &CFG_I::FormatConfig, "FormatConfig"},
        {0x0808, nullptr, "UpdateConfigBlk1"},
        {0x0809, nullptr, "UpdateConfigBlk2"},
        {0x080A, nullptr, "UpdateConfigBlk3"},
        {0x080B, nullptr, "SetGetLocalFriendCodeSeedData"},
        {0x080C, nullptr, "SetLocalFriendCodeSeedSignature"},
        {0x080D, nullptr, "DeleteCreateNANDLocalFriendCodeSeed"},
        {0x080E, nullptr, "VerifySigLocalFriendCodeSeed"},
        {0x080F, nullptr, "GetLocalFriendCodeSeedData"},
        {0x0810, nullptr, "GetLocalFriendCodeSeed"},
        {0x0811, nullptr, "SetSecureInfo"},
        {0x0812, nullptr, "DeleteCreateNANDSecureInfo"},
        {0x0813, nullptr, "VerifySigSecureInfo"},
        {0x0814, nullptr, "SecureInfoGetData"},
        {0x0815, nullptr, "SecureInfoGetSignature"},
        {0x0816, &CFG_I::GetRegion, "GetRegion"},
        {0x0817, &CFG_I::SecureInfoGetByte101, "SecureInfoGetByte101"},
        {0x0818, nullptr, "SecureInfoGetSerialNo"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::CFG
