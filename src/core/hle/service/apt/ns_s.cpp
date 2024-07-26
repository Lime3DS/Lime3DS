// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/apt/ns_s.h"

namespace Service::NS {

NS_S::NS_S(std::shared_ptr<Service::APT::Module> apt)
    : Service::APT::Module::NSInterface(std::move(apt), "ns:s", 3) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "LaunchFIRM"},
        {0x0002, nullptr, "LaunchTitle"},
        {0x0003, nullptr, "TerminateApplication"},
        {0x0004, nullptr, "TerminateProcess"},
        {0x0005, nullptr, "LaunchApplicationFIRM"},
        {0x0006, &NS_S::SetWirelessRebootInfo, "SetWirelessRebootInfo"},
        {0x0007, nullptr, "CardUpdateInitialize"},
        {0x0008, nullptr, "CardUpdateShutdown"},
        {0x000D, nullptr, "SetTWLBannerHMAC"},
        {0x000E, &NS_S::ShutdownAsync, "ShutdownAsync"},
        {0x0010, &NS_S::RebootSystem, "RebootSystem"},
        {0x0011, nullptr, "TerminateTitle"},
        {0x0012, nullptr, "SetApplicationCpuTimeLimit"},
        {0x0015, nullptr, "LaunchApplication"},
        {0x0016, &NS_S::RebootSystemClean, "RebootSystemClean"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NS

SERIALIZE_EXPORT_IMPL(Service::NS::NS_S)
