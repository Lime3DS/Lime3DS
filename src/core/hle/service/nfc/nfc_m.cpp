// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/nfc/nfc_m.h"

SERIALIZE_EXPORT_IMPL(Service::NFC::NFC_M)

namespace Service::NFC {

NFC_M::NFC_M(std::shared_ptr<Module> nfc) : Module::Interface(std::move(nfc), "nfc:m", 1) {
    static const FunctionInfo functions[] = {
        // nfc:u shared commands
        // clang-format off
        {0x0001, &NFC_M::Initialize, "Initialize"},
        {0x0002, &NFC_M::Finalize, "Finalize"},
        {0x0003, &NFC_M::Connect, "Connect"},
        {0x0004, &NFC_M::Disconnect, "Disconnect"},
        {0x0005, &NFC_M::StartDetection, "StartDetection"},
        {0x0006, &NFC_M::StopDetection, "StopDetection"},
        {0x0007, &NFC_M::Mount, "Mount"},
        {0x0008, &NFC_M::Unmount, "Unmount"},
        {0x0009, &NFC_M::Flush, "Flush"},
        {0x000A, nullptr, "Restore"},
        {0x000B, &NFC_M::GetActivateEvent, "GetActivateEvent"},
        {0x000C, &NFC_M::GetDeactivateEvent, "GetDeactivateEvent"},
        {0x000D, &NFC_M::GetStatus, "GetStatus"},
        {0x000E, nullptr, "Unknown0x0E"},
        {0x000F, &NFC_M::GetTargetConnectionStatus, "GetTargetConnectionStatus"},
        {0x0010, &NFC_M::GetTagInfo2, "GetTagInfo2"},
        {0x0011, &NFC_M::GetTagInfo, "GetTagInfo"},
        {0x0012, &NFC_M::GetConnectResult, "GetConnectResult"},
        {0x0013, &NFC_M::OpenApplicationArea, "OpenApplicationArea"},
        {0x0014, &NFC_M::CreateApplicationArea, "CreateApplicationArea"},
        {0x0015, &NFC_M::ReadApplicationArea, "ReadApplicationArea"},
        {0x0016, &NFC_M::WriteApplicationArea, "WriteApplicationArea"},
        {0x0017, &NFC_M::GetNfpRegisterInfo, "GetNfpRegisterInfo"},
        {0x0018, &NFC_M::GetNfpCommonInfo, "GetNfpCommonInfo"},
        {0x0019, &NFC_M::InitializeCreateInfo, "InitializeCreateInfo"},
        {0x001A, &NFC_M::MountRom, "MountRom"},
        {0x001B, &NFC_M::GetIdentificationBlock, "GetIdentificationBlock"},
        // nfc:m
        {0x0401, &NFC_M::Format, "Format"},
        {0x0402, &NFC_M::GetAdminInfo, "GetAdminInfo"},
        {0x0403, &NFC_M::GetEmptyRegisterInfo, "GetEmptyRegisterInfo"},
        {0x0404, &NFC_M::SetRegisterInfo, "SetRegisterInfo"},
        {0x0405, &NFC_M::DeleteRegisterInfo, "DeleteRegisterInfo"},
        {0x0406, &NFC_M::DeleteApplicationArea, "DeleteApplicationArea"},
        {0x0407, &NFC_M::ExistsApplicationArea, "ExistsApplicationArea"}
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NFC
