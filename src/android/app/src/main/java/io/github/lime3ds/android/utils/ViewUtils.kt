// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.view.View
import android.view.ViewGroup

object ViewUtils {
    fun showView(view: View, length: Long = 300) {
        view.apply {
            alpha = 0f
            visibility = View.VISIBLE
            isClickable = true
        }.animate().apply {
            duration = length
            alpha(1f)
        }.start()
    }

    fun hideView(view: View, length: Long = 300) {
        if (view.visibility == View.INVISIBLE) {
            return
        }

        view.apply {
            alpha = 1f
            isClickable = false
        }.animate().apply {
            duration = length
            alpha(0f)
        }.withEndAction {
            view.visibility = View.INVISIBLE
        }.start()
    }
}
