// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import io.github.lime3ds.android.R
import io.github.lime3ds.android.applets.MiiSelector
import io.github.lime3ds.android.utils.SerializableHelper.serializable

class MiiSelectorDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val config = requireArguments().serializable<MiiSelector.MiiSelectorConfig>(CONFIG)!!

        // Note: we intentionally leave out the Standard Mii in the native code so that
        // the string can get translated
        val list = mutableListOf<String>()
        list.add(getString(R.string.standard_mii))
        list.addAll(config.miiNames)
        val initialIndex =
            if (config.initiallySelectedMiiIndex < list.size) config.initiallySelectedMiiIndex.toInt() else 0
        MiiSelector.data.index = initialIndex
        val builder = MaterialAlertDialogBuilder(requireActivity())
            .setTitle(if (config.title!!.isEmpty()) getString(R.string.mii_selector) else config.title)
            .setSingleChoiceItems(list.toTypedArray(), initialIndex) { _: DialogInterface?, which: Int ->
                MiiSelector.data.index = which
            }
            .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int ->
                MiiSelector.data.returnCode = 0
                synchronized(MiiSelector.finishLock) { MiiSelector.finishLock.notifyAll() }
            }
        if (config.enableCancelButton) {
            builder.setNegativeButton(android.R.string.cancel) { _: DialogInterface?, _: Int ->
                MiiSelector.data.returnCode = 1
                synchronized(MiiSelector.finishLock) { MiiSelector.finishLock.notifyAll() }
            }
        }
        isCancelable = false
        return builder.create()
    }

    companion object {
        const val TAG = "MiiSelectorDialogFragment"

        const val CONFIG = "config"

        fun newInstance(config: MiiSelector.MiiSelectorConfig): MiiSelectorDialogFragment {
            val frag = MiiSelectorDialogFragment()
            val args = Bundle()
            args.putSerializable(CONFIG, config)
            frag.arguments = args
            return frag
        }
    }
}
