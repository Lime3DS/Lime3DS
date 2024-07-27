// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.applets

import androidx.annotation.Keep
import io.github.lime3ds.android.NativeLibrary
import io.github.lime3ds.android.fragments.MiiSelectorDialogFragment
import java.io.Serializable

@Keep
object MiiSelector {
    lateinit var data: MiiSelectorData
    val finishLock = Object()

    private fun ExecuteImpl(config: MiiSelectorConfig) {
        val emulationActivity = NativeLibrary.sEmulationActivity.get()
        data = MiiSelectorData(0, 0)
        val fragment = MiiSelectorDialogFragment.newInstance(config)
        fragment.show(emulationActivity!!.supportFragmentManager, "mii_selector")
    }

    @JvmStatic
    fun Execute(config: MiiSelectorConfig): MiiSelectorData {
        NativeLibrary.sEmulationActivity.get()!!.runOnUiThread { ExecuteImpl(config) }
        synchronized(finishLock) {
            try {
                finishLock.wait()
            } catch (ignored: Exception) {
            }
        }
        return data
    }

    @Keep
    class MiiSelectorConfig : Serializable {
        var enableCancelButton = false
        var title: String? = null
        var initiallySelectedMiiIndex: Long = 0

        // List of Miis to display
        lateinit var miiNames: Array<String>
    }

    class MiiSelectorData (var returnCode: Long, var index: Int)
}
