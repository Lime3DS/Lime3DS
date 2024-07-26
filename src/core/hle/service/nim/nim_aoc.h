// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service::NIM {

class NIM_AOC final : public ServiceFramework<NIM_AOC> {
public:
    NIM_AOC();
    ~NIM_AOC();

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::NIM

BOOST_CLASS_EXPORT_KEY(Service::NIM::NIM_AOC)
