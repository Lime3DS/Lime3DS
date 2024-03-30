// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace DiscordRPC {

class DiscordInterface {
public:
    virtual ~DiscordInterface() = default;

    virtual void Pause() = 0;
    virtual void Update() = 0;
};

class NullImpl : public DiscordInterface {
public:
    ~NullImpl() = default;

    void Pause() override {}
    void Update() override {}
};

} // namespace DiscordRPC
