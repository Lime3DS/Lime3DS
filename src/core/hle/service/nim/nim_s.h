// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service::NIM {

class NIM_S final : public ServiceFramework<NIM_S> {
public:
    NIM_S();
    ~NIM_S();

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::NIM

BOOST_CLASS_EXPORT_KEY(Service::NIM::NIM_S)
