// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/nfc/nfc.h"

namespace Service::NFC {

class NFC_U final : public Module::Interface {
public:
    explicit NFC_U(std::shared_ptr<Module> nfc);

private:
    SERVICE_SERIALIZATION(NFC_U, nfc, Module)
};

} // namespace Service::NFC

BOOST_CLASS_EXPORT_KEY(Service::NFC::NFC_U)
BOOST_SERIALIZATION_CONSTRUCT(Service::NFC::NFC_U)
