// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.display

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
