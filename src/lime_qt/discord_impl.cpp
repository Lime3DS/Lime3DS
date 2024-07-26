// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <string>
#include <discord_rpc.h>
#include "common/common_types.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "lime_qt/discord_impl.h"
#include "lime_qt/uisettings.h"

namespace DiscordRPC {

DiscordImpl::DiscordImpl(const Core::System& system_) : system{system_} {
    DiscordEventHandlers handlers{};

    // The number is the client ID for Lime3DS, it's used for images and the
    // application name. rustygrape238 on discord is the RPC maintainer.
    Discord_Initialize("1222229231367487539", &handlers, 1, nullptr);
}

DiscordImpl::~DiscordImpl() {
    Discord_ClearPresence();
    Discord_Shutdown();
}

void DiscordImpl::Pause() {
    Discord_ClearPresence();
}

void DiscordImpl::Update() {
    s64 start_time = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    std::string title;
    const bool is_powered_on = system.IsPoweredOn();
    if (is_powered_on) {
        system.GetAppLoader().ReadTitleLong(title);
    }

    DiscordRichPresence presence{};
    presence.largeImageKey = "lime_large_icon";
    if (is_powered_on) {
        presence.largeImageText = title.c_str();
        presence.state = title.c_str();
        presence.details = "Currently in game";
    } else {
        presence.largeImageText = "Not in game";
        presence.details = "Not in game";
    }
    presence.startTimestamp = start_time;
    Discord_UpdatePresence(&presence);
}
} // namespace DiscordRPC
