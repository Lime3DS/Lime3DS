// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model

enum class ScaledFloatSetting(
    override val key: String,
    override val section: String,
    override val defaultValue: Float,
    val scale: Int
) : AbstractFloatSetting {
    AUDIO_VOLUME("volume", Settings.SECTION_AUDIO, 1.0f, 100);

    override var float: Float = defaultValue
        get() = field * scale
        set(value) {
            field = value / scale
        }

    override val valueAsString: String get() = (float / scale).toString()

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
        private val NOT_RUNTIME_EDITABLE = emptyList<ScaledFloatSetting>()

        fun from(key: String): ScaledFloatSetting? =
            ScaledFloatSetting.values().firstOrNull { it.key == key }

        fun clear() = ScaledFloatSetting.values().forEach { it.float = it.defaultValue * it.scale }
    }
}
