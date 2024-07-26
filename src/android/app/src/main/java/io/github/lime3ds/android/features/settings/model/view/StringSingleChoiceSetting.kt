// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model.view

import io.github.lime3ds.android.features.settings.model.AbstractSetting
import io.github.lime3ds.android.features.settings.model.AbstractShortSetting
import io.github.lime3ds.android.features.settings.model.AbstractStringSetting

class StringSingleChoiceSetting(
    setting: AbstractSetting?,
    titleId: Int,
    descriptionId: Int,
    val choices: Array<String>,
    val values: Array<String>?,
    val key: String? = null,
    private val defaultValue: String? = null
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_STRING_SINGLE_CHOICE

    fun getValueAt(index: Int): String? {
        if (values == null) return null
        return if (index >= 0 && index < values.size) {
            values[index]
        } else {
            ""
        }
    }

    val selectedValue: String
        get() {
            if (setting == null) {
                return defaultValue!!
            }

            try {
                val setting = setting as AbstractStringSetting
                return setting.string
            } catch (_: ClassCastException) {
            }

            try {
                val setting = setting as AbstractShortSetting
                return setting.short.toString()
            } catch (_: ClassCastException) {
            }
            return defaultValue!!
        }
    val selectValueIndex: Int
        get() {
            val selectedValue = selectedValue
            for (i in values!!.indices) {
                if (values[i] == selectedValue) {
                    return i
                }
            }
            return -1
        }

    /**
     * Write a value to the backing int. If that int was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the int.
     * @return the existing setting with the new value applied.
     */
    fun setSelectedValue(selection: String): AbstractStringSetting {
        val stringSetting = setting as AbstractStringSetting
        stringSetting.string = selection
        return stringSetting
    }

    fun setSelectedValue(selection: Short): AbstractShortSetting {
        val shortSetting = setting as AbstractShortSetting
        shortSetting.short = selection
        return shortSetting
    }
}
