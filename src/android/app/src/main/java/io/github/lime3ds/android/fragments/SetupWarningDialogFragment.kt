// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import io.github.lime3ds.android.R

class SetupWarningDialogFragment : DialogFragment() {
    private var titleId: Int = 0
    private var descriptionId: Int = 0
    private var helpLinkId: Int = 0
    private var page: Int = 0

    private lateinit var setupFragment: SetupFragment

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        titleId = requireArguments().getInt(TITLE)
        descriptionId = requireArguments().getInt(DESCRIPTION)
        helpLinkId = requireArguments().getInt(HELP_LINK)
        page = requireArguments().getInt(PAGE)

        setupFragment = requireParentFragment() as SetupFragment
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val builder = MaterialAlertDialogBuilder(requireContext())
            .setPositiveButton(R.string.warning_skip) { _: DialogInterface?, _: Int ->
                setupFragment.pageForward()
                setupFragment.setPageWarned(page)
            }
            .setNegativeButton(R.string.warning_cancel, null)

        if (titleId != 0) {
            builder.setTitle(titleId)
        } else {
            builder.setTitle("")
        }
        if (descriptionId != 0) {
            builder.setMessage(descriptionId)
        }
        if (helpLinkId != 0) {
            builder.setNeutralButton(R.string.warning_help) { _: DialogInterface?, _: Int ->
                val helpLink = resources.getString(helpLinkId)
                val intent = Intent(Intent.ACTION_VIEW, Uri.parse(helpLink))
                startActivity(intent)
            }
        }

        return builder.show()
    }

    companion object {
        const val TAG = "SetupWarningDialogFragment"

        private const val TITLE = "Title"
        private const val DESCRIPTION = "Description"
        private const val HELP_LINK = "HelpLink"
        private const val PAGE = "Page"

        fun newInstance(
            titleId: Int,
            descriptionId: Int,
            helpLinkId: Int,
            page: Int
        ): SetupWarningDialogFragment {
            val dialog = SetupWarningDialogFragment()
            val bundle = Bundle()
            bundle.apply {
                putInt(TITLE, titleId)
                putInt(DESCRIPTION, descriptionId)
                putInt(HELP_LINK, helpLinkId)
                putInt(PAGE, page)
            }
            dialog.arguments = bundle
            return dialog
        }
    }
}
