// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model.view

import androidx.annotation.DrawableRes

class RunnableSetting(
    titleId: Int,
    descriptionId: Int,
    val isRuntimeRunnable: Boolean,
    @DrawableRes val iconId: Int = 0,
    val runnable: () -> Unit,
    val value: (() -> String)? = null
) : SettingsItem(null, titleId, descriptionId) {
    override val type = TYPE_RUNNABLE
}
