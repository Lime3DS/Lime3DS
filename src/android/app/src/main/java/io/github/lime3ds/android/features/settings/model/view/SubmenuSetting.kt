// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model.view

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes

class SubmenuSetting(
    @StringRes titleId: Int,
    @StringRes descriptionId: Int,
    @DrawableRes val iconId: Int,
    val menuKey: String
) : SettingsItem(null, titleId, descriptionId) {
    override val type = TYPE_SUBMENU
}
