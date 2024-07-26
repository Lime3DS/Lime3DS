// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/cam/cam.h"

namespace Service::CAM {

class CAM_C final : public Module::Interface {
public:
    explicit CAM_C(std::shared_ptr<Module> cam);

private:
    SERVICE_SERIALIZATION(CAM_C, cam, Module)
};

} // namespace Service::CAM

BOOST_CLASS_EXPORT_KEY(Service::CAM::CAM_C)
BOOST_SERIALIZATION_CONSTRUCT(Service::CAM::CAM_C)
