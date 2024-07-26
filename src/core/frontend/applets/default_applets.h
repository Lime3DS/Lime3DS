// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Frontend {
/**
 * Registers default, frontend-independent applet implementations.
 * Will be replaced later if any frontend-specific implementation is available.
 */
void RegisterDefaultApplets(Core::System& system);
} // namespace Frontend
