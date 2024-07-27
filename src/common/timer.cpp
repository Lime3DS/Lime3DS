// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <ctime>
#include <fmt/format.h>
#include "common/common_types.h"
#include "common/timer.h"

namespace Common {

std::chrono::milliseconds Timer::GetTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

// --------------------------------------------
// Initiate, Start, Stop, and Update the time
// --------------------------------------------

// Set initial values for the class
Timer::Timer() : m_LastTime(0), m_StartTime(0), m_Running(false) {
    Update();
}

// Write the starting time
void Timer::Start() {
    m_StartTime = GetTimeMs();
    m_Running = true;
}

// Stop the timer
void Timer::Stop() {
    // Write the final time
    m_LastTime = GetTimeMs();
    m_Running = false;
}

// Update the last time variable
void Timer::Update() {
    m_LastTime = GetTimeMs();
    // TODO(ector) - QPF
}

// -------------------------------------
// Get time difference and elapsed time
// -------------------------------------

// Get the number of milliseconds since the last Update()
std::chrono::milliseconds Timer::GetTimeDifference() {
    return GetTimeMs() - m_LastTime;
}

// Add the time difference since the last Update() to the starting time.
// This is used to compensate for a paused game.
void Timer::AddTimeDifference() {
    m_StartTime += GetTimeDifference();
}

// Get the time elapsed since the Start()
std::chrono::milliseconds Timer::GetTimeElapsed() {
    // If we have not started yet, return 1 (because then I don't
    // have to change the FPS calculation in CoreRerecording.cpp .
    if (m_StartTime.count() == 0)
        return std::chrono::milliseconds(1);

    // Return the final timer time if the timer is stopped
    if (!m_Running)
        return (m_LastTime - m_StartTime);

    return (GetTimeMs() - m_StartTime);
}

// Get the formatted time elapsed since the Start()
std::string Timer::GetTimeElapsedFormatted() const {
    // If we have not started yet, return zero
    if (m_StartTime.count() == 0)
        return "00:00:00:000";

    // The number of milliseconds since the start.
    // Use a different value if the timer is stopped.
    std::chrono::milliseconds Milliseconds;
    if (m_Running)
        Milliseconds = GetTimeMs() - m_StartTime;
    else
        Milliseconds = m_LastTime - m_StartTime;
    // Seconds
    std::chrono::seconds Seconds = std::chrono::duration_cast<std::chrono::seconds>(Milliseconds);
    // Minutes
    std::chrono::minutes Minutes = std::chrono::duration_cast<std::chrono::minutes>(Milliseconds);
    // Hours
    std::chrono::hours Hours = std::chrono::duration_cast<std::chrono::hours>(Milliseconds);

    std::string TmpStr = fmt::format("{:02}:{:02}:{:02}:{:03}", Hours.count(), Minutes.count() % 60,
                                     Seconds.count() % 60, Milliseconds.count() % 1000);
    return TmpStr;
}

// Get the number of seconds since January 1 1970
std::chrono::seconds Timer::GetTimeSinceJan1970() {
    return std::chrono::duration_cast<std::chrono::seconds>(GetTimeMs());
}

std::chrono::seconds Timer::GetLocalTimeSinceJan1970() {
    time_t sysTime, tzDiff, tzDST;
    struct tm* gmTime;

    time(&sysTime);

    // Account for DST where needed
    gmTime = localtime(&sysTime);
    if (gmTime->tm_isdst == 1)
        tzDST = 3600;
    else
        tzDST = 0;

    // Lazy way to get local time in sec
    gmTime = gmtime(&sysTime);
    tzDiff = sysTime - mktime(gmTime);

    return std::chrono::seconds(sysTime + tzDiff + tzDST);
}

// Return the current time formatted as Minutes:Seconds:Milliseconds
// in the form 00:00:000.
std::string Timer::GetTimeFormatted() {
    time_t sysTime;
    struct tm* gmTime;
    char tmp[13];

    time(&sysTime);
    gmTime = localtime(&sysTime);

    strftime(tmp, 6, "%M:%S", gmTime);

    u64 milliseconds = static_cast<u64>(GetTimeMs().count()) % 1000;
    return fmt::format("{}:{:03}", tmp, milliseconds);
}

} // Namespace Common
