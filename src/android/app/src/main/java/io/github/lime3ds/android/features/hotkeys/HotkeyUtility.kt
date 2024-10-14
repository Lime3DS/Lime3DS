// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.hotkeys

import android.content.Context
import android.widget.Toast
import io.github.lime3ds.android.NativeLibrary
import io.github.lime3ds.android.R
import io.github.lime3ds.android.utils.EmulationLifecycleUtil
import io.github.lime3ds.android.display.ScreenAdjustmentUtil

class HotkeyUtility(private val screenAdjustmentUtil: ScreenAdjustmentUtil, private val context: Context) {
// TODO:
// hotkeyLoaded should be a global val that tracks if a hotkey is laoded or not
// Seperate shortcuts better 
    val hotkeyButtons = Hotkey.entries.map { it.button }

    fun handleHotkey(bindedButton: Int): Boolean {
        if(hotkeyButtons.contains(bindedButton)) {
            when (bindedButton) {
                Hotkey.SWAP_SCREEN.button -> screenAdjustmentUtil.swapScreen()
                Hotkey.CYCLE_LAYOUT.button -> screenAdjustmentUtil.cycleLayouts()
                Hotkey.CLOSE_GAME.button -> EmulationLifecycleUtil.closeGame()
                Hotkey.PAUSE_OR_RESUME.button -> EmulationLifecycleUtil.pauseOrResume()
                Hotkey.QUICKSAVE.button -> {
                    NativeLibrary.saveState(NativeLibrary.QUICKSAVE_SLOT)
                    Toast.makeText(context,
                        context.getString(R.string.quicksave_saving),
                        Toast.LENGTH_SHORT).show()
                }
                Hotkey.QUICKLOAD.button -> {
                    val hotkeyLoaded = NativeLibrary.loadStateIfAvailable(NativeLibrary.QUICKSAVE_SLOT)
                    val stringRes = if(hotkeyLoaded) {
                        R.string.quickload_loading
                    } else {
                        R.string.quickload_not_found
                    }
                    Toast.makeText(context,
                        context.getString(stringRes),
                        Toast.LENGTH_SHORT).show()
                }
                else -> {}
            }
            return true
        }
        return false
    }
}
