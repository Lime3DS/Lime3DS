// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::QTM {

class QTM_U final : public ServiceFramework<QTM_U> {
public:
    QTM_U();
    ~QTM_U() = default;

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::QTM

BOOST_CLASS_EXPORT_KEY(Service::QTM::QTM_U)
