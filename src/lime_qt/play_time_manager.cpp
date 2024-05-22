// SPDX-FileCopyrightText: 2024 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include "common/alignment.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/thread.h"
#include "lime_qt/play_time_manager.h"

namespace PlayTime {

namespace {

struct PlayTimeElement {
    ProgramId program_id;
    PlayTime play_time;
};

std::string GetCurrentUserPlayTimePath() {
    return FileUtil::GetUserPath(FileUtil::UserPath::PlayTimeDir) + DIR_SEP + "play_time.bin";
}

[[nodiscard]] bool ReadPlayTimeFile(PlayTimeDatabase& out_play_time_db) {
    const auto filename = GetCurrentUserPlayTimePath();

    out_play_time_db.clear();

    if (FileUtil::Exists(filename)) {
        FileUtil::IOFile file{filename, "rb"};
        if (!file.IsOpen()) {
            LOG_ERROR(Frontend, "Failed to open play time file: {}", filename);
            return false;
        }

        const size_t num_elements = file.GetSize() / sizeof(PlayTimeElement);
        std::vector<PlayTimeElement> elements(num_elements);

        if (file.ReadSpan<PlayTimeElement>(elements) != num_elements) {
            return false;
        }

        for (const auto& [program_id, play_time] : elements) {
            if (program_id != 0) {
                out_play_time_db[program_id] = play_time;
            }
        }
    }

    return true;
}

[[nodiscard]] bool WritePlayTimeFile(const PlayTimeDatabase& play_time_db) {
    const auto filename = GetCurrentUserPlayTimePath();

    FileUtil::IOFile file{filename, "wb"};
    if (!file.IsOpen()) {
        LOG_ERROR(Frontend, "Failed to open play time file: {}", filename);
        return false;
    }

    std::vector<PlayTimeElement> elements;
    elements.reserve(play_time_db.size());

    for (auto& [program_id, play_time] : play_time_db) {
        if (program_id != 0) {
            elements.push_back(PlayTimeElement{program_id, play_time});
        }
    }

    return file.WriteSpan<PlayTimeElement>(elements) == elements.size();
}

} // namespace

PlayTimeManager::PlayTimeManager() {
    if (!ReadPlayTimeFile(database)) {
        LOG_ERROR(Frontend, "Failed to read play time database! Resetting to default.");
    }
}

PlayTimeManager::~PlayTimeManager() {
    Save();
}

void PlayTimeManager::SetProgramId(u64 program_id) {
    running_program_id = program_id;
}

void PlayTimeManager::Start() {
    play_time_thread = std::jthread([&](std::stop_token stop_token) { AutoTimestamp(stop_token); });
}

void PlayTimeManager::Stop() {
    play_time_thread = {};
}

void PlayTimeManager::AutoTimestamp(std::stop_token stop_token) {
    Common::SetCurrentThreadName("PlayTimeReport");

    using namespace std::literals::chrono_literals;
    using std::chrono::seconds;
    using std::chrono::steady_clock;

    auto timestamp = steady_clock::now();

    const auto GetDuration = [&]() -> u64 {
        const auto last_timestamp = std::exchange(timestamp, steady_clock::now());
        const auto duration = std::chrono::duration_cast<seconds>(timestamp - last_timestamp);
        return static_cast<u64>(duration.count());
    };

    while (!stop_token.stop_requested()) {
        Common::StoppableTimedWait(stop_token, 30s);

        database[running_program_id] += GetDuration();
        Save();
    }
}

void PlayTimeManager::Save() {
    if (!WritePlayTimeFile(database)) {
        LOG_ERROR(Frontend, "Failed to update play time database!");
    }
}

u64 PlayTimeManager::GetPlayTime(u64 program_id) const {
    auto it = database.find(program_id);
    if (it != database.end()) {
        return it->second;
    } else {
        return 0;
    }
}

void PlayTimeManager::ResetProgramPlayTime(u64 program_id) {
    database.erase(program_id);
    Save();
}

QString ReadablePlayTime(qulonglong time_seconds) {
    if (time_seconds == 0) {
        return {};
    }
    const auto time_minutes = std::max(static_cast<double>(time_seconds) / 60, 1.0);
    const auto time_hours = static_cast<double>(time_seconds) / 3600;
    const bool is_minutes = time_minutes < 60;
    const char* unit = is_minutes ? "m" : "h";
    const auto value = is_minutes ? time_minutes : time_hours;

    return QStringLiteral("%L1 %2")
        .arg(value, 0, 'f', !is_minutes && time_seconds % 60 != 0)
        .arg(QString::fromUtf8(unit));
}

} // namespace PlayTime
