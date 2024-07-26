// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nfc/nfc_u.h"

SERIALIZE_EXPORT_IMPL(Service::NFC::NFC_U)

namespace Service::NFC {

NFC_U::NFC_U(std::shared_ptr<Module> nfc) : Module::Interface(std::move(nfc), "nfc:u", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &NFC_U::Initialize, "Initialize"},
        {0x0002, &NFC_U::Finalize, "Finalize"},
        {0x0003, &NFC_U::Connect, "Connect"},
        {0x0004, &NFC_U::Disconnect, "Disconnect"},
        {0x0005, &NFC_U::StartDetection, "StartDetection"},
        {0x0006, &NFC_U::StopDetection, "StopDetection"},
        {0x0007, &NFC_U::Mount, "Mount"},
        {0x0008, &NFC_U::Unmount, "Unmount"},
        {0x0009, &NFC_U::Flush, "Flush"},
        {0x000A, nullptr, "Restore"},
        {0x000B, &NFC_U::GetActivateEvent, "GetActivateEvent"},
        {0x000C, &NFC_U::GetDeactivateEvent, "GetDeactivateEvent"},
        {0x000D, &NFC_U::GetStatus, "GetStatus"},
        {0x000E, nullptr, "Unknown0x0E"},
        {0x000F, &NFC_U::GetTargetConnectionStatus, "GetTargetConnectionStatus"},
        {0x0010, &NFC_U::GetTagInfo2, "GetTagInfo2"},
        {0x0011, &NFC_U::GetTagInfo, "GetTagInfo"},
        {0x0012, &NFC_U::GetConnectResult, "GetConnectResult"},
        {0x0013, &NFC_U::OpenApplicationArea, "OpenApplicationArea"},
        {0x0014, &NFC_U::CreateApplicationArea, "CreateApplicationArea"},
        {0x0015, &NFC_U::ReadApplicationArea, "ReadApplicationArea"},
        {0x0016, &NFC_U::WriteApplicationArea, "WriteApplicationArea"},
        {0x0017, &NFC_U::GetNfpRegisterInfo, "GetNfpRegisterInfo"},
        {0x0018, &NFC_U::GetNfpCommonInfo, "GetNfpCommonInfo"},
        {0x0019, &NFC_U::InitializeCreateInfo, "InitializeCreateInfo"},
        {0x001A, &NFC_U::MountRom, "MountRom"},
        {0x001B, &NFC_U::GetIdentificationBlock, "GetIdentificationBlock"},
        {0x001C, nullptr, "Unknown0x1C"},
        {0x001D, nullptr, "Unknown0x1D"},
        {0x001E, nullptr, "Unknown0x1E"},
        {0x001F, nullptr, "Unknown0x1F"},
        {0x0020, nullptr, "Unknown0x20"},
        {0x0021, nullptr, "Unknown0x21"},
        {0x0022, nullptr, "Unknown0x22"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NFC
