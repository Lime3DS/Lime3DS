// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.model.view

class HeaderSetting(titleId: Int, descId: Int = 0) : SettingsItem(null, titleId, descId) {
    override val type = TYPE_HEADER
}
