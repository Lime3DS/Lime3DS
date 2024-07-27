// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::MVD {

class MVD_STD final : public ServiceFramework<MVD_STD> {
public:
    MVD_STD();
    ~MVD_STD() = default;

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::MVD

BOOST_CLASS_EXPORT_KEY(Service::MVD::MVD_STD)
