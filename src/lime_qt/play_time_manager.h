// SPDX-FileCopyrightText: 2024 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

#include <map>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/polyfill_thread.h"

namespace PlayTime {

using ProgramId = u64;
using PlayTime = u64;
using PlayTimeDatabase = std::map<ProgramId, PlayTime>;

class PlayTimeManager {
public:
    explicit PlayTimeManager();
    ~PlayTimeManager();

    PlayTimeManager(const PlayTimeManager&) = delete;
    PlayTimeManager& operator=(const PlayTimeManager&) = delete;

    u64 GetPlayTime(u64 program_id) const;
    void ResetProgramPlayTime(u64 program_id);
    void SetProgramId(u64 program_id);
    void Start();
    void Stop();

private:
    void AutoTimestamp(std::stop_token stop_token);
    void Save();

    PlayTimeDatabase database;
    u64 running_program_id;
    std::jthread play_time_thread;
};

QString ReadablePlayTime(qulonglong time_seconds);

} // namespace PlayTime
