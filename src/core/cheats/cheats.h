// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <vector>
#include "common/common_types.h"

namespace Core {
class System;
struct TimingEventType;
} // namespace Core

namespace CoreTiming {
struct EventType;
}

namespace Cheats {

class CheatBase;

class CheatEngine {
public:
    explicit CheatEngine(Core::System& system);
    ~CheatEngine();

    /// Registers the cheat execution callback.
    void Connect();

    /// Returns a span of the currently active cheats.
    std::span<const std::shared_ptr<CheatBase>> GetCheats() const;

    /// Adds a cheat to the cheat engine.
    void AddCheat(std::shared_ptr<CheatBase>&& cheat);

    /// Removes a cheat at the specified index in the cheats list.
    void RemoveCheat(std::size_t index);

    /// Updates a cheat at the specified index in the cheats list.
    void UpdateCheat(std::size_t index, std::shared_ptr<CheatBase>&& new_cheat);

    /// Loads the cheat file from disk for the specified title id.
    void LoadCheatFile(u64 title_id);

    /// Saves currently active cheats to file for the specified title id.
    void SaveCheatFile(u64 title_id) const;

private:
    /// The cheat execution callback.
    void RunCallback(std::uintptr_t user_data, s64 cycles_late);

private:
    Core::System& system;
    Core::TimingEventType* event;
    std::optional<u64> loaded_title_id;
    std::vector<std::shared_ptr<CheatBase>> cheats_list;
    mutable std::shared_mutex cheats_list_mutex;
};
} // namespace Cheats
