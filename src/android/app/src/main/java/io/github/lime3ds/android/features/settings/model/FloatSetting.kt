// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model

enum class FloatSetting(
    override val key: String,
    override val section: String,
    override val defaultValue: Float
) : AbstractFloatSetting {
    LARGE_SCREEN_PROPORTION("large_screen_proportion",Settings.SECTION_LAYOUT,2.25f),
    EMPTY_SETTING("", "", 0.0f);

    override var float: Float = defaultValue

    override val valueAsString: String
        get() = float.toString()

    override val isRuntimeEditable: Boolean
        get() {
            for (setting in NOT_RUNTIME_EDITABLE) {
                if (setting == this) {
                    return false
                }
            }
            return true
        }

    companion object {
        private val NOT_RUNTIME_EDITABLE = emptyList<FloatSetting>()

        fun from(key: String): FloatSetting? = FloatSetting.values().firstOrNull { it.key == key }

        fun clear() = FloatSetting.values().forEach { it.float = it.defaultValue }
    }
}
