// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Common {

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
// Defined in error.cpp.
[[nodiscard]] std::string GetLastErrorMsg();

// Like GetLastErrorMsg(), but passing an explicit error code.
// Defined in error.cpp.
[[nodiscard]] std::string NativeErrorToString(int e);

} // namespace Common
