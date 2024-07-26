// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/boss/boss.h"

namespace Service::BOSS {

class BOSS_U final : public Module::Interface {
public:
    explicit BOSS_U(std::shared_ptr<Module> boss);

private:
    SERVICE_SERIALIZATION(BOSS_U, boss, Module)
};

} // namespace Service::BOSS

BOOST_CLASS_EXPORT_KEY(Service::BOSS::BOSS_U)
BOOST_SERIALIZATION_CONSTRUCT(Service::BOSS::BOSS_U)
