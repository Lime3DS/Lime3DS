// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model.view

import io.github.lime3ds.android.features.settings.model.AbstractIntSetting
import io.github.lime3ds.android.features.settings.model.AbstractSetting
import io.github.lime3ds.android.features.settings.model.AbstractShortSetting

class SingleChoiceSetting(
    setting: AbstractSetting?,
    titleId: Int,
    descriptionId: Int,
    val choicesId: Int,
    val valuesId: Int,
    val key: String? = null,
    val defaultValue: Int? = null
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_SINGLE_CHOICE

    val selectedValue: Int
        get() {
            if (setting == null) {
                return defaultValue!!
            }

            try {
                val setting = setting as AbstractIntSetting
                return setting.int
            } catch (_: ClassCastException) {
            }

            try {
                val setting = setting as AbstractShortSetting
                return setting.short.toInt()
            } catch (_: ClassCastException) {
            }

            return defaultValue!!
        }

    /**
     * Write a value to the backing int. If that int was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the int.
     * @return the existing setting with the new value applied.
     */
    fun setSelectedValue(selection: Int): AbstractIntSetting {
        val intSetting = setting as AbstractIntSetting
        intSetting.int = selection
        return intSetting
    }

    fun setSelectedValue(selection: Short): AbstractShortSetting {
        val shortSetting = setting as AbstractShortSetting
        shortSetting.short = selection
        return shortSetting
    }
}
