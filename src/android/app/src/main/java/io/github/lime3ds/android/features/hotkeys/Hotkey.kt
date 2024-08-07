// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.hotkeys

enum class Hotkey(val button: Int) {
    SWAP_SCREEN(10001),
    CYCLE_LAYOUT(10002),
    CLOSE_GAME(10003),
    PAUSE_OR_RESUME(10004),
    QUICKSAVE(10005),
    QUICKLOAD(10006);
}
