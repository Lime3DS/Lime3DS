// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/kernel.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/service.h"

namespace Service::NS {

/// Interface to "ns:c" service
class NS_C final : public Service::APT::Module::NSInterface {
public:
    explicit NS_C(std::shared_ptr<Service::APT::Module> apt);

private:
    SERVICE_SERIALIZATION(NS_C, apt, Service::APT::Module)
};

} // namespace Service::NS

BOOST_CLASS_EXPORT_KEY(Service::NS::NS_C)
BOOST_SERIALIZATION_CONSTRUCT(Service::NS::NS_C)
