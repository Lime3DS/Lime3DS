// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import io.github.lime3ds.android.R
import io.github.lime3ds.android.ui.main.MainActivity
import io.github.lime3ds.android.viewmodel.HomeViewModel

class SelectUserDirectoryDialogFragment : DialogFragment() {
    private lateinit var mainActivity: MainActivity

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        mainActivity = requireActivity() as MainActivity

        isCancelable = false
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.select_lime3ds_user_folder)
            .setMessage(R.string.cannot_skip_directory_description)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                mainActivity?.openLime3DSDirectory?.launch(null)
            }
            .show()
    }

    companion object {
        const val TAG = "SelectUserDirectoryDialogFragment"

        fun newInstance(activity: FragmentActivity): SelectUserDirectoryDialogFragment {
            ViewModelProvider(activity)[HomeViewModel::class.java].setPickingUserDir(true)
            return SelectUserDirectoryDialogFragment()
        }
    }
}
