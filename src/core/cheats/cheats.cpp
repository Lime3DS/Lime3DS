// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fstream>
#include <functional>
#include <fmt/format.h>
#include "common/file_util.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/core_timing.h"

namespace Cheats {

// Luma3DS uses this interval for applying cheats, so to keep consistent behavior
// we use the same value
constexpr u64 run_interval_ticks = 50'000'000;

CheatEngine::CheatEngine(Core::System& system_) : system{system_} {}

CheatEngine::~CheatEngine() {
    if (system.IsPoweredOn()) {
        system.CoreTiming().UnscheduleEvent(event, 0);
    }
}

void CheatEngine::Connect() {
    event = system.CoreTiming().RegisterEvent(
        "CheatCore::run_event",
        [this](u64 thread_id, s64 cycle_late) { RunCallback(thread_id, cycle_late); });
    system.CoreTiming().ScheduleEvent(run_interval_ticks, event);
}

std::span<const std::shared_ptr<CheatBase>> CheatEngine::GetCheats() const {
    std::shared_lock lock{cheats_list_mutex};
    return cheats_list;
}

void CheatEngine::AddCheat(std::shared_ptr<CheatBase>&& cheat) {
    std::unique_lock lock{cheats_list_mutex};
    cheats_list.push_back(std::move(cheat));
}

void CheatEngine::RemoveCheat(std::size_t index) {
    std::unique_lock lock{cheats_list_mutex};
    if (index < 0 || index >= cheats_list.size()) {
        LOG_ERROR(Core_Cheats, "Invalid index {}", index);
        return;
    }
    cheats_list.erase(cheats_list.begin() + index);
}

void CheatEngine::UpdateCheat(std::size_t index, std::shared_ptr<CheatBase>&& new_cheat) {
    std::unique_lock lock{cheats_list_mutex};
    if (index < 0 || index >= cheats_list.size()) {
        LOG_ERROR(Core_Cheats, "Invalid index {}", index);
        return;
    }
    cheats_list[index] = std::move(new_cheat);
}

void CheatEngine::SaveCheatFile(u64 title_id) const {
    const std::string cheat_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);
    const std::string filepath = fmt::format("{}{:016X}.txt", cheat_dir, title_id);

    if (!FileUtil::IsDirectory(cheat_dir)) {
        FileUtil::CreateDir(cheat_dir);
    }
    FileUtil::IOFile file(filepath, "w");

    auto cheats = GetCheats();
    for (const auto& cheat : cheats) {
        file.WriteString(cheat->ToString());
    }
}

void CheatEngine::LoadCheatFile(u64 title_id) {
    {
        std::unique_lock lock{cheats_list_mutex};
        if (loaded_title_id.has_value() && loaded_title_id == title_id) {
            return;
        }
    }

    const std::string cheat_dir = FileUtil::GetUserPath(FileUtil::UserPath::CheatsDir);
    const std::string filepath = fmt::format("{}{:016X}.txt", cheat_dir, title_id);

    if (!FileUtil::IsDirectory(cheat_dir)) {
        FileUtil::CreateDir(cheat_dir);
    }

    if (!FileUtil::Exists(filepath)) {
        return;
    }

    auto gateway_cheats = GatewayCheat::LoadFile(filepath);
    {
        std::unique_lock lock{cheats_list_mutex};
        loaded_title_id = title_id;
        cheats_list = std::move(gateway_cheats);
    }
}

void CheatEngine::RunCallback([[maybe_unused]] std::uintptr_t user_data, s64 cycles_late) {
    {
        std::shared_lock lock{cheats_list_mutex};
        for (const auto& cheat : cheats_list) {
            if (cheat->IsEnabled()) {
                cheat->Execute(system);
            }
        }
    }
    system.CoreTiming().ScheduleEvent(run_interval_ticks - cycles_late, event);
}

} // namespace Cheats
