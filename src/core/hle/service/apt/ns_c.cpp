// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/apt/ns_c.h"

namespace Service::NS {

NS_C::NS_C(std::shared_ptr<Service::APT::Module> apt)
    : Service::APT::Module::NSInterface(std::move(apt), "ns:c", 1) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "LockSpecialContent"},
        {0x0002, nullptr, "UnlockSpecialContent"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NS

SERIALIZE_EXPORT_IMPL(Service::NS::NS_C)
