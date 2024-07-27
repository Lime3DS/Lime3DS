// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <string>
#include "common/common_types.h"

namespace Common {
class Timer {
public:
    Timer();

    void Start();
    void Stop();
    void Update();

    // The time difference is always returned in milliseconds, regardless of alternative internal
    // representation
    [[nodiscard]] std::chrono::milliseconds GetTimeDifference();
    void AddTimeDifference();

    [[nodiscard]] static std::chrono::seconds GetTimeSinceJan1970();
    [[nodiscard]] static std::chrono::seconds GetLocalTimeSinceJan1970();

    [[nodiscard]] static std::string GetTimeFormatted();
    [[nodiscard]] std::string GetTimeElapsedFormatted() const;
    [[nodiscard]] std::chrono::milliseconds GetTimeElapsed();

    [[nodiscard]] static std::chrono::milliseconds GetTimeMs();

private:
    std::chrono::milliseconds m_LastTime;
    std::chrono::milliseconds m_StartTime;
    bool m_Running;
};

} // Namespace Common
