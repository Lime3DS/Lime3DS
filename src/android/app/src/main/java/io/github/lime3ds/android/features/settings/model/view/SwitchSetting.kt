// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model.view

import io.github.lime3ds.android.features.settings.model.AbstractBooleanSetting
import io.github.lime3ds.android.features.settings.model.AbstractIntSetting
import io.github.lime3ds.android.features.settings.model.AbstractSetting

class SwitchSetting(
    setting: AbstractSetting,
    titleId: Int,
    descriptionId: Int,
    val key: String? = null,
    val defaultValue: Any? = null
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_SWITCH

    val isChecked: Boolean
        get() {
            if (setting == null) {
                return defaultValue as Boolean
            }

            // Try integer setting
            try {
                val setting = setting as AbstractIntSetting
                return setting.int == 1
            } catch (_: ClassCastException) {
            }

            // Try boolean setting
            try {
                val setting = setting as AbstractBooleanSetting
                return setting.boolean
            } catch (_: ClassCastException) {
            }
            return defaultValue as Boolean
        }

    /**
     * Write a value to the backing boolean. If that boolean was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param checked Pretty self explanatory.
     * @return the existing setting with the new value applied.
     */
    fun setChecked(checked: Boolean): AbstractSetting {
        // Try integer setting
        try {
            val setting = setting as AbstractIntSetting
            setting.int = if (checked) 1 else 0
            return setting
        } catch (_: ClassCastException) {
        }

        // Try boolean setting
        val setting = setting as AbstractBooleanSetting
        setting.boolean = checked
        return setting
    }
}
