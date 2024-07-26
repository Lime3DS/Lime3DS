// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/nfc/nfc.h"

namespace Service::NFC {

class NFC_M final : public Module::Interface {
public:
    explicit NFC_M(std::shared_ptr<Module> nfc);

private:
    SERVICE_SERIALIZATION(NFC_M, nfc, Module)
};

} // namespace Service::NFC

BOOST_CLASS_EXPORT_KEY(Service::NFC::NFC_M)
BOOST_SERIALIZATION_CONSTRUCT(Service::NFC::NFC_M)
