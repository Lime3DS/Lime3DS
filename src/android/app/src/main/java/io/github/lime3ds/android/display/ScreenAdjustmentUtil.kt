// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.display

import android.view.WindowManager
import io.github.lime3ds.android.NativeLibrary
import io.github.lime3ds.android.features.settings.model.BooleanSetting
import io.github.lime3ds.android.features.settings.model.IntSetting
import io.github.lime3ds.android.features.settings.model.Settings
import io.github.lime3ds.android.features.settings.utils.SettingsFile
import io.github.lime3ds.android.utils.EmulationMenuSettings

class ScreenAdjustmentUtil(private val windowManager: WindowManager,
                           private val settings: Settings) {
    fun swapScreen() {
        val isEnabled = !EmulationMenuSettings.swapScreens
        EmulationMenuSettings.swapScreens = isEnabled
        NativeLibrary.swapScreens(
            isEnabled,
            windowManager.defaultDisplay.rotation
        )
        BooleanSetting.SWAP_SCREEN.boolean = isEnabled
        settings.saveSetting(BooleanSetting.SWAP_SCREEN, SettingsFile.FILE_NAME_CONFIG)
    }

    fun cycleLayouts() {
        val nextLayout = (EmulationMenuSettings.landscapeScreenLayout + 1) % ScreenLayout.entries.size
        changeScreenOrientation(ScreenLayout.from(nextLayout))
    }

    fun changeScreenOrientation(layoutOption: ScreenLayout) {
        EmulationMenuSettings.landscapeScreenLayout = layoutOption.int
        NativeLibrary.notifyOrientationChange(
            EmulationMenuSettings.landscapeScreenLayout,
            windowManager.defaultDisplay.rotation
        )
        IntSetting.SCREEN_LAYOUT.int = layoutOption.int
        settings.saveSetting(IntSetting.SCREEN_LAYOUT, SettingsFile.FILE_NAME_CONFIG)
    }
}
