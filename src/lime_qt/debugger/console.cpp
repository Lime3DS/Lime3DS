// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef _WIN32
#include <windows.h>

#include <wincon.h>
#endif

#include "common/logging/backend.h"
#include "lime_qt/debugger/console.h"
#include "lime_qt/uisettings.h"

namespace Debugger {
void ToggleConsole() {
    static bool console_shown = false;
    if (console_shown == UISettings::values.show_console.GetValue()) {
        return;
    } else {
        console_shown = UISettings::values.show_console.GetValue();
    }

    using namespace Common::Log;
#ifdef _WIN32
    FILE* temp;
    if (UISettings::values.show_console) {
        if (AllocConsole()) {
            // The first parameter for freopen_s is a out parameter, so we can just ignore it
            freopen_s(&temp, "CONIN$", "r", stdin);
            freopen_s(&temp, "CONOUT$", "w", stdout);
            freopen_s(&temp, "CONOUT$", "w", stderr);
            SetColorConsoleBackendEnabled(true);
        }
    } else {
        if (FreeConsole()) {
            // In order to close the console, we have to also detach the streams on it.
            // Just redirect them to NUL if there is no console window
            SetColorConsoleBackendEnabled(false);
            freopen_s(&temp, "NUL", "r", stdin);
            freopen_s(&temp, "NUL", "w", stdout);
            freopen_s(&temp, "NUL", "w", stderr);
        }
    }
#else
    SetColorConsoleBackendEnabled(UISettings::values.show_console.GetValue());
#endif
}
} // namespace Debugger
