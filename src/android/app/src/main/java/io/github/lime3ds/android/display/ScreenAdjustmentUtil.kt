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

    // TODO: Consider how cycling should handle custom layout
    // right now it simply skips it
    fun cycleLayouts() {
        val nextLayout = if (NativeLibrary.isPortraitMode) {
            (EmulationMenuSettings.portraitScreenLayout + 1) % (PortraitScreenLayout.entries.size - 1)
        } else {
            (EmulationMenuSettings.landscapeScreenLayout + 1) % (ScreenLayout.entries.size - 1)
        }
        settings.loadSettings()

        changeScreenOrientation(nextLayout)
    }

    fun changeScreenOrientation(layoutOption: Int) {
        if (NativeLibrary.isPortraitMode) {
            EmulationMenuSettings.portraitScreenLayout = layoutOption
            NativeLibrary.notifyOrientationChange(
                EmulationMenuSettings.portraitScreenLayout,
                windowManager.defaultDisplay.rotation
            )
            IntSetting.PORTRAIT_SCREEN_LAYOUT.int = layoutOption
            settings.saveSetting(IntSetting.PORTRAIT_SCREEN_LAYOUT, SettingsFile.FILE_NAME_CONFIG)

        } else {
            EmulationMenuSettings.landscapeScreenLayout = layoutOption
            NativeLibrary.notifyOrientationChange(
                EmulationMenuSettings.landscapeScreenLayout,
                windowManager.defaultDisplay.rotation
            )
            IntSetting.SCREEN_LAYOUT.int = layoutOption
            settings.saveSetting(IntSetting.SCREEN_LAYOUT, SettingsFile.FILE_NAME_CONFIG)
        }
    }
}
