// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model.view

import io.github.lime3ds.android.NativeLibrary
import io.github.lime3ds.android.features.settings.model.AbstractSetting
import io.github.lime3ds.android.activities.EmulationActivity

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Each one corresponds to a [AbstractSetting] object, so this class's subclasses
 * should vaguely correspond to those subclasses. There are a few with multiple analogues
 * and a few with none (Headers, for example, do not correspond to anything in the ini
 * file.)
 */
abstract class SettingsItem(
    var setting: AbstractSetting?,
    val nameId: Int,
    val descriptionId: Int
) {
    abstract val type: Int

    val isEditable: Boolean
        get() {
            if (!EmulationActivity.isRunning()) return true
            return setting?.isRuntimeEditable ?: false
        }

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_SWITCH = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
        const val TYPE_STRING_SINGLE_CHOICE = 5
        const val TYPE_DATETIME_SETTING = 6
        const val TYPE_RUNNABLE = 7
        const val TYPE_INPUT_BINDING = 8
        const val TYPE_STRING_INPUT = 9
        const val TYPE_FLOAT_INPUT = 10
    }
}
