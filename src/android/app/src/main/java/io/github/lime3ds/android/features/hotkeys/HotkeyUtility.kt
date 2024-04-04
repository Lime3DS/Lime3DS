// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.hotkeys

import io.github.lime3ds.android.utils.EmulationLifecycleUtil
import io.github.lime3ds.android.display.ScreenAdjustmentUtil

class HotkeyUtility(private val screenAdjustmentUtil: ScreenAdjustmentUtil) {

    val hotkeyButtons = Hotkey.entries.map { it.button }

    fun handleHotkey(bindedButton: Int): Boolean {
        if(hotkeyButtons.contains(bindedButton)) {
            when (bindedButton) {
                Hotkey.SWAP_SCREEN.button -> screenAdjustmentUtil.swapScreen()
                Hotkey.CYCLE_LAYOUT.button -> screenAdjustmentUtil.cycleLayouts()
                Hotkey.CLOSE_GAME.button -> EmulationLifecycleUtil.closeGame()
                Hotkey.PAUSE_OR_RESUME.button -> EmulationLifecycleUtil.pauseOrResume()
                else -> {}
            }
            return true
        }
        return false
    }
}
