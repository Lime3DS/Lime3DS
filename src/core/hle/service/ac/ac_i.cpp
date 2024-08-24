// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ac/ac_i.h"

namespace Service::AC {

AC_I::AC_I(std::shared_ptr<Module> ac) : Module::Interface(std::move(ac), "ac:i", 10) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &AC_I::CreateDefaultConfig, "CreateDefaultConfig"},
        {0x0004, &AC_I::ConnectAsync, "ConnectAsync"},
        {0x0005, &AC_I::GetConnectResult, "GetConnectResult"},
        {0x0007, &AC_I::CancelConnectAsync, "CancelConnectAsync"},
        {0x0008, &AC_I::CloseAsync, "CloseAsync"},
        {0x0009, &AC_I::GetCloseResult, "GetCloseResult"},
        {0x000A, nullptr, "GetLastErrorCode"},
        {0x000C, &AC_I::GetStatus, "GetStatus"},
        {0x000D, &AC_I::GetWifiStatus, "GetWifiStatus"},
        {0x000E, &AC_I::GetCurrentAPInfo, "GetCurrentAPInfo"},
        {0x000F, &AC_I::GetConnectingInfraPriority, "GetConnectingInfraPriority"},
        {0x0010, nullptr, "GetCurrentNZoneInfo"},
        {0x0011, nullptr, "GetNZoneApNumService"},
        {0x001D, &AC_I::ScanAPs, "ScanAPs"},
        {0x0024, nullptr, "AddDenyApType"},
        {0x0027, &AC_I::GetInfraPriority, "GetInfraPriority"},
        {0x002C, &AC_I::SetFromApplication, "SetFromApplication"},
        {0x002D, &AC_I::SetRequestEulaVersion, "SetRequestEulaVersion"},
        {0x002F, &AC_I::GetNZoneBeaconNotFoundEvent, "GetNZoneBeaconNotFoundEvent"},
        {0x0030, &AC_I::RegisterDisconnectEvent, "RegisterDisconnectEvent"},
        {0x0036, &AC_I::GetConnectingProxyEnable, "GetConnectingProxyEnable"},
        {0x003C, nullptr, "GetAPSSIDList"},
        {0x003E, &AC_I::IsConnected, "IsConnected"},
        {0x0040, &AC_I::SetClientVersion, "SetClientVersion"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::AC

SERIALIZE_EXPORT_IMPL(Service::AC::AC_I)
