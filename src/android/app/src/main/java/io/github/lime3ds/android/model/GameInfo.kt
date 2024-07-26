// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.model

import androidx.annotation.Keep
import java.io.IOException

class GameInfo(path: String) {
    @Keep
    private val pointer: Long

    init {
        pointer = initialize(path)
        if (pointer == 0L) {
            throw IOException()
        }
    }

    protected external fun finalize()

    external fun getTitle(): String

    external fun getRegions(): String

    external fun getCompany(): String

    external fun getIcon(): IntArray?

    external fun getIsVisibleSystemTitle(): Boolean

    companion object {
        @JvmStatic
        private external fun initialize(path: String): Long
    }
}
