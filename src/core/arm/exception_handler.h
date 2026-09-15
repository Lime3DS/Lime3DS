// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"

namespace Core {

class System;

enum class ExceptionType : u32 {
    UnmappedRead,
    UnmappedWrite,
    DataAbort,
    PrefetchAbort,
    UndefinedInstruction,
    Break,
};

/**
 * Logs a detailed crash report and halts emulation.
 *
 * Produces a structured report containing the exception type, register state and stack dump.
 * The report is logged at LOG_CRITICAL level and passed to System::SetStatus to halt the run loop.
 *
 * @param system          The emulator system instance.
 * @param type            The type of exception that occurred.
 */
void LogException(System& system, ExceptionType type);

/**
 * Configures whether exceptions should be ignored for the current session.
 *
 * When enabled, LogException() will log the exception report without halting emulation.
 *
 * @param ignore    Whether to ignore exceptions and continue emulation.
 */
void SetIgnoreExceptionsForSession(bool ignore);
bool AreExceptionsIgnoredForSession();

} // namespace Core
