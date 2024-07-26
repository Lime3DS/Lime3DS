// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/hid/hid.h"

// This service is used for interfacing to physical user controls.
// Uses include game pad controls, touchscreen, accelerometers, gyroscopes, and debug pad.

namespace Service::HID {

class User final : public Module::Interface {
public:
    explicit User(std::shared_ptr<Module> hid);

private:
    SERVICE_SERIALIZATION(User, hid, Module)
};

} // namespace Service::HID

BOOST_CLASS_EXPORT_KEY(Service::HID::User)
BOOST_SERIALIZATION_CONSTRUCT(Service::HID::User)
