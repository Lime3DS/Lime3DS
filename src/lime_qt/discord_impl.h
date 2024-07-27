// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "lime_qt/discord.h"

namespace Core {
class System;
}

namespace DiscordRPC {

class DiscordImpl : public DiscordInterface {
public:
    DiscordImpl(const Core::System& system);
    ~DiscordImpl() override;

    void Pause() override;
    void Update() override;

private:
    const Core::System& system;
};

} // namespace DiscordRPC
