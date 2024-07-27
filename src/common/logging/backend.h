// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string_view>
#include "common/logging/filter.h"

namespace Common::Log {

class Filter;

/// Initializes the logging system. This should be the first thing called in main.
void Initialize(std::string_view log_file = "");

void Start();

/// Explictily stops the logger thread and flushes the buffers
void Stop();

void DisableLoggingInTests();

/**
 * The global filter will prevent any messages from even being processed if they are filtered.
 */
void SetGlobalFilter(const Filter& filter);

void SetColorConsoleBackendEnabled(bool enabled);
} // namespace Common::Log
