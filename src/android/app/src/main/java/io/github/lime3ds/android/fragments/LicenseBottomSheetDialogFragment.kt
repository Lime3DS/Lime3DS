// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import io.github.lime3ds.android.databinding.DialogLicenseBinding
import io.github.lime3ds.android.model.License
import io.github.lime3ds.android.utils.SerializableHelper.parcelable

class LicenseBottomSheetDialogFragment : BottomSheetDialogFragment() {
    private var _binding: DialogLicenseBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogLicenseBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        BottomSheetBehavior.from<View>(view.parent as View).state =
            BottomSheetBehavior.STATE_HALF_EXPANDED

        val license = requireArguments().parcelable<License>(LICENSE)!!

        binding.apply {
            textTitle.setText(license.titleId)
            textLink.setText(license.linkId)
            if (license.copyrightId != 0) {
                textCopyright.setText(license.copyrightId)
            } else {
                textCopyright.visibility = View.GONE
            }
            if (license.licenseId != 0) {
                textLicense.setText(license.licenseId)
            } else {
                textLicense.setText(license.licenseLinkId)
                BottomSheetBehavior.from<View>(view.parent as View).state =
                    BottomSheetBehavior.STATE_COLLAPSED
            }
        }
    }

    companion object {
        const val TAG = "LicenseBottomSheetDialogFragment"

        const val LICENSE = "License"

        fun newInstance(
            license: License
        ): LicenseBottomSheetDialogFragment {
            val dialog = LicenseBottomSheetDialogFragment()
            val bundle = Bundle()
            bundle.putParcelable(LICENSE, license)
            dialog.arguments = bundle
            return dialog
        }
    }
}
