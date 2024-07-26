// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.app.Dialog
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import io.github.lime3ds.android.R

class MessageDialogFragment : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val titleId = requireArguments().getInt(TITLE_ID)
        val descriptionId = requireArguments().getInt(DESCRIPTION_ID)
        val descriptionString = requireArguments().getString(DESCRIPTION_STRING) ?: ""
        val helpLinkId = requireArguments().getInt(HELP_LINK)

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setPositiveButton(R.string.close, null)
            .setTitle(titleId)

        if (descriptionString.isNotEmpty()) {
            dialog.setMessage(descriptionString)
        } else if (descriptionId != 0) {
            dialog.setMessage(descriptionId)
        }

        if (helpLinkId != 0) {
            dialog.setNeutralButton(R.string.learn_more) { _, _ ->
                openLink(getString(helpLinkId))
            }
        }

        return dialog.show()
    }

    private fun openLink(link: String) {
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(link))
        startActivity(intent)
    }

    companion object {
        const val TAG = "MessageDialogFragment"

        private const val TITLE_ID = "Title"
        private const val DESCRIPTION_ID = "Description"
        private const val DESCRIPTION_STRING = "Description_string"
        private const val HELP_LINK = "Link"

        fun newInstance(
            titleId: Int,
            descriptionId: Int,
            helpLinkId: Int = 0
        ): MessageDialogFragment {
            val dialog = MessageDialogFragment()
            val bundle = Bundle()
            bundle.apply {
                putInt(TITLE_ID, titleId)
                putInt(DESCRIPTION_ID, descriptionId)
                putInt(HELP_LINK, helpLinkId)
            }
            dialog.arguments = bundle
            return dialog
        }

        fun newInstance(
            titleId: Int,
            description: String,
            helpLinkId: Int = 0
        ): MessageDialogFragment {
            val dialog = MessageDialogFragment()
            val bundle = Bundle()
            bundle.apply {
                putInt(TITLE_ID, titleId)
                putString(DESCRIPTION_STRING, description)
                putInt(HELP_LINK, helpLinkId)
            }
            dialog.arguments = bundle
            return dialog
        }
    }
}
