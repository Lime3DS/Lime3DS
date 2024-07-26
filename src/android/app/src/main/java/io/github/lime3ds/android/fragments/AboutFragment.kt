// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.MarginLayoutParams
import android.widget.Toast
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import com.google.android.material.transition.MaterialSharedAxis
import io.github.lime3ds.android.BuildConfig
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.FragmentAboutBinding
import io.github.lime3ds.android.viewmodel.HomeViewModel

class AboutFragment : Fragment() {
    private var _binding: FragmentAboutBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentAboutBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        homeViewModel.setNavigationVisibility(visible = false, animated = true)
        homeViewModel.setStatusBarShadeVisibility(visible = false)

        binding.toolbarAbout.setNavigationOnClickListener {
            binding.root.findNavController().popBackStack()
        }

        binding.buttonContributors.setOnClickListener {
            openLink(
                getString(R.string.contributors_link)
            )
        }
        binding.buttonLicenses.setOnClickListener {
            exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
            binding.root.findNavController().navigate(R.id.action_aboutFragment_to_licensesFragment)
        }

        binding.textBuildHash.text = BuildConfig.VERSION_NAME
        binding.buttonBuildHash.setOnClickListener {
            val clipBoard =
                requireContext().getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            val clip = ClipData.newPlainText(getString(R.string.build), BuildConfig.GIT_HASH)
            clipBoard.setPrimaryClip(clip)

            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                Toast.makeText(
                    requireContext(),
                    R.string.copied_to_clipboard,
                    Toast.LENGTH_SHORT
                ).show()
            }
        }

        binding.buttonDiscord.setOnClickListener { openLink(getString(R.string.support_link)) }
        binding.buttonWebsite.setOnClickListener { openLink(getString(R.string.website_link)) }
        binding.buttonGithub.setOnClickListener { openLink(getString(R.string.github_link)) }

        setInsets()
    }

    private fun openLink(link: String) {
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse(link))
        startActivity(intent)
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val mlpAppBar = binding.toolbarAbout.layoutParams as MarginLayoutParams
            mlpAppBar.leftMargin = leftInsets
            mlpAppBar.rightMargin = rightInsets
            binding.toolbarAbout.layoutParams = mlpAppBar

            val mlpScrollAbout = binding.scrollAbout.layoutParams as MarginLayoutParams
            mlpScrollAbout.leftMargin = leftInsets
            mlpScrollAbout.rightMargin = rightInsets
            binding.scrollAbout.layoutParams = mlpScrollAbout

            binding.contentAbout.updatePadding(bottom = barInsets.bottom)

            windowInsets
        }
}
