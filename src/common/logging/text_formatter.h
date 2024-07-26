// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Common::Log {

struct Entry;

/// Formats a log entry into the provided text buffer.
std::string FormatLogMessage(const Entry& entry);
/// Formats and prints a log entry to stderr.
void PrintMessage(const Entry& entry);
/// Prints the same message as `PrintMessage`, but colored according to the severity level.
void PrintColoredMessage(const Entry& entry);
/// Formats and prints a log entry to the android logcat.
void PrintMessageToLogcat(const Entry& entry);
} // namespace Common::Log
