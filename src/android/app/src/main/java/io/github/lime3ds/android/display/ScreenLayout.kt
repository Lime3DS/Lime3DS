// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.display

enum class ScreenLayout(val int: Int) {
    // These must match what is defined in src/common/settings.h
    DEFAULT(0),
    SINGLE_SCREEN(1),
    LARGE_SCREEN(2),
    SIDE_SCREEN(3),
    HYBRID_SCREEN(4),
    MOBILE_PORTRAIT(5),
    MOBILE_LANDSCAPE(6);

    companion object {
        fun from(int: Int): ScreenLayout {
            return entries.firstOrNull { it.int == int } ?: DEFAULT
        }
    }
}
