// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <random>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::SSL {

class SSL_C final : public ServiceFramework<SSL_C> {
public:
    SSL_C();

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void GenerateRandomData(Kernel::HLERequestContext& ctx);

    SERVICE_SERIALIZATION_SIMPLE
};

void InstallInterfaces(Core::System& system);

void GenerateRandomData(std::vector<u8>& out);

} // namespace Service::SSL

BOOST_CLASS_EXPORT_KEY(Service::SSL::SSL_C)
