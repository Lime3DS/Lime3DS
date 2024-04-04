// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.cheats.model

import androidx.annotation.Keep

@Keep
class Cheat(@field:Keep private val mPointer: Long) {
    private var enabledChangedCallback: Runnable? = null
    protected external fun finalize()

    external fun getName(): String

    external fun getNotes(): String

    external fun getCode(): String

    external fun getEnabled(): Boolean

    fun setEnabled(enabled: Boolean) {
        setEnabledImpl(enabled)
        onEnabledChanged()
    }

    private external fun setEnabledImpl(enabled: Boolean)

    fun setEnabledChangedCallback(callback: Runnable) {
        enabledChangedCallback = callback
    }

    private fun onEnabledChanged() {
        enabledChangedCallback?.run()
    }

    companion object {
        /**
         * If the code is valid, returns 0. Otherwise, returns the 1-based index
         * for the line containing the error.
         */
        @JvmStatic
        external fun isValidGatewayCode(code: String): Int

        @JvmStatic
        external fun createGatewayCode(name: String, notes: String, code: String): Cheat
    }
}
