// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/cam/cam.h"

namespace Service::CAM {

class CAM_U final : public Module::Interface {
public:
    explicit CAM_U(std::shared_ptr<Module> cam);

private:
    SERVICE_SERIALIZATION(CAM_U, cam, Module)
};

} // namespace Service::CAM

BOOST_CLASS_EXPORT_KEY(Service::CAM::CAM_U)
BOOST_SERIALIZATION_CONSTRUCT(Service::CAM::CAM_U)
