// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.content.Intent
import android.net.Uri
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.ViewModelProvider
import io.github.lime3ds.android.fragments.Lime3DSDirectoryDialogFragment
import io.github.lime3ds.android.fragments.CopyDirProgressDialog
import io.github.lime3ds.android.model.SetupCallback
import io.github.lime3ds.android.viewmodel.HomeViewModel

/**
 * Lime3DS directory initialization ui flow controller.
 */
class Lime3DSDirectoryHelper(private val fragmentActivity: FragmentActivity) {
    fun showLime3DSDirectoryDialog(result: Uri, callback: SetupCallback? = null) {
        val lime3dsDirectoryDialog = Lime3DSDirectoryDialogFragment.newInstance(
            fragmentActivity,
            result.toString(),
            Lime3DSDirectoryDialogFragment.Listener { moveData: Boolean, path: Uri ->
                val previous = PermissionsHandler.lime3dsDirectory
                // Do noting if user select the previous path.
                if (path == previous) {
                    return@Listener
                }

                val takeFlags = Intent.FLAG_GRANT_WRITE_URI_PERMISSION or
                        Intent.FLAG_GRANT_READ_URI_PERMISSION
                fragmentActivity.contentResolver.takePersistableUriPermission(
                    path,
                    takeFlags
                )
                if (!moveData || previous.toString().isEmpty()) {
                    initializeLime3DSDirectory(path)
                    callback?.onStepCompleted()
                    val viewModel = ViewModelProvider(fragmentActivity)[HomeViewModel::class.java]
                    viewModel.setUserDir(fragmentActivity, path.path!!)
                    viewModel.setPickingUserDir(false)
                    return@Listener
                }

                // If user check move data, show copy progress dialog.
                CopyDirProgressDialog.newInstance(fragmentActivity, previous, path, callback)
                    ?.show(fragmentActivity.supportFragmentManager, CopyDirProgressDialog.TAG)
            })
        lime3dsDirectoryDialog.show(
            fragmentActivity.supportFragmentManager,
            Lime3DSDirectoryDialogFragment.TAG
        )
    }

    companion object {
        fun initializeLime3DSDirectory(path: Uri) {
            PermissionsHandler.setLime3DSDirectory(path.toString())
            DirectoryInitialization.resetLime3DSDirectoryState()
            DirectoryInitialization.start()
        }
    }
}
