// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.display

enum class ScreenLayout(val int: Int) {
    // These must match what is defined in src/common/settings.h
    ORIGINAL(0),
    SINGLE_SCREEN(1),
    LARGE_SCREEN(2),
    SIDE_SCREEN(3),
    HYBRID_SCREEN(4),
    CUSTOM_LAYOUT(5),
    MOBILE_LANDSCAPE(6);


    companion object {
        fun from(int: Int): ScreenLayout {
            return entries.firstOrNull { it.int == int } ?: MOBILE_LANDSCAPE
        }
    }
}

enum class PortraitScreenLayout(val int: Int) {
    // These must match what is defined in src/common/settings.h
    TOP_FULL_WIDTH(0),
    CUSTOM_PORTRAIT_LAYOUT(1);

    companion object {
        fun from(int: Int): PortraitScreenLayout {
            return entries.firstOrNull { it.int == int } ?: TOP_FULL_WIDTH;
        }
    }
}