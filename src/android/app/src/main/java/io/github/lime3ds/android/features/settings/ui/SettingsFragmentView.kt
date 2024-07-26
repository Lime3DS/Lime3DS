// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.ui

import io.github.lime3ds.android.features.settings.model.AbstractSetting
import io.github.lime3ds.android.features.settings.model.view.SettingsItem

/**
 * Abstraction for a screen showing a list of settings. Instances of
 * this type of view will each display a layer of the setting hierarchy.
 */
interface SettingsFragmentView {
    /**
     * Pass an ArrayList to the View so that it can be displayed on screen.
     *
     * @param settingsList The result of converting the HashMap to an ArrayList
     */
    fun showSettingsList(settingsList: ArrayList<SettingsItem>)

    /**
     * Instructs the Fragment to load the settings screen.
     */
    fun loadSettingsList()

    /**
     * @return The Fragment's containing activity.
     */
    val activityView: SettingsActivityView?

    /**
     * Tell the Fragment to tell the containing Activity to show a new
     * Fragment containing a submenu of settings.
     *
     * @param menuKey Identifier for the settings group that should be shown.
     */
    fun loadSubMenu(menuKey: String)

    /**
     * Tell the Fragment to tell the containing activity to display a toast message.
     *
     * @param message Text to be shown in the Toast
     * @param is_long Whether this should be a long Toast or short one.
     */
    fun showToastMessage(message: String?, is_long: Boolean)

    /**
     * Have the fragment add a setting to the HashMap.
     *
     * @param setting The (possibly previously missing) new setting.
     */
    fun putSetting(setting: AbstractSetting)

    /**
     * Have the fragment tell the containing Activity that a setting was modified.
     */
    fun onSettingChanged()
}
