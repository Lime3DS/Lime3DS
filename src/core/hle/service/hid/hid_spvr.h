// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid/hid.h"

namespace Service::HID {

class Spvr final : public Module::Interface {
public:
    explicit Spvr(std::shared_ptr<Module> hid);

private:
    SERVICE_SERIALIZATION(Spvr, hid, Module)
};

} // namespace Service::HID

BOOST_CLASS_EXPORT_KEY(Service::HID::Spvr)
BOOST_SERIALIZATION_CONSTRUCT(Service::HID::Spvr)
