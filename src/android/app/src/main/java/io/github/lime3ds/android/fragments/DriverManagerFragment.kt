// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.documentfile.provider.DocumentFile
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import io.github.lime3ds.android.R
import io.github.lime3ds.android.adapters.DriverAdapter
import io.github.lime3ds.android.databinding.FragmentDriverManagerBinding
import io.github.lime3ds.android.utils.FileUtil.asDocumentFile
import io.github.lime3ds.android.utils.FileUtil.inputStream
import io.github.lime3ds.android.utils.GpuDriverHelper
import io.github.lime3ds.android.viewmodel.HomeViewModel
import io.github.lime3ds.android.viewmodel.DriverViewModel
import java.io.IOException

class DriverManagerFragment : Fragment() {
    private var _binding: FragmentDriverManagerBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private val driverViewModel: DriverViewModel by activityViewModels()

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
        _binding = FragmentDriverManagerBinding.inflate(inflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setNavigationVisibility(visible = false, animated = true)
        homeViewModel.setStatusBarShadeVisibility(visible = false)

        if (!driverViewModel.isInteractionAllowed) {
            DriversLoadingDialogFragment().show(
                childFragmentManager,
                DriversLoadingDialogFragment.TAG
            )
        }

        binding.toolbarDrivers.setNavigationOnClickListener {
            binding.root.findNavController().popBackStack()
        }

        binding.buttonInstall.setOnClickListener {
            getDriver.launch(arrayOf("application/zip"))
        }

        binding.listDrivers.apply {
            layoutManager = GridLayoutManager(
                requireContext(),
                resources.getInteger(R.integer.game_grid_columns)
            )
            adapter = DriverAdapter(driverViewModel)
        }

        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                driverViewModel.driverList.collectLatest {
                    (binding.listDrivers.adapter as DriverAdapter).submitList(it)
                }
            }
            launch {
                driverViewModel.newDriverInstalled.collect {
                    if (_binding != null && it) {
                        (binding.listDrivers.adapter as DriverAdapter).apply {
                            notifyItemChanged(driverViewModel.previouslySelectedDriver)
                            notifyItemChanged(driverViewModel.selectedDriver)
                            driverViewModel.setNewDriverInstalled(false)
                        }
                    }
                }
            }
        }

        setInsets()
    }

    // Start installing requested driver
    override fun onStop() {
        super.onStop()
        driverViewModel.onCloseDriverManager()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val mlpAppBar = binding.toolbarDrivers.layoutParams as ViewGroup.MarginLayoutParams
            mlpAppBar.leftMargin = leftInsets
            mlpAppBar.rightMargin = rightInsets
            binding.toolbarDrivers.layoutParams = mlpAppBar

            val mlplistDrivers = binding.listDrivers.layoutParams as ViewGroup.MarginLayoutParams
            mlplistDrivers.leftMargin = leftInsets
            mlplistDrivers.rightMargin = rightInsets
            binding.listDrivers.layoutParams = mlplistDrivers

            val fabSpacing = resources.getDimensionPixelSize(R.dimen.spacing_fab)
            val mlpFab =
                binding.buttonInstall.layoutParams as ViewGroup.MarginLayoutParams
            mlpFab.leftMargin = leftInsets + fabSpacing
            mlpFab.rightMargin = rightInsets + fabSpacing
            mlpFab.bottomMargin = barInsets.bottom + fabSpacing
            binding.buttonInstall.layoutParams = mlpFab

            binding.listDrivers.updatePadding(
                bottom = barInsets.bottom +
                        resources.getDimensionPixelSize(R.dimen.spacing_bottom_list_fab)
            )

            windowInsets
        }

    private val getDriver =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null) {
                return@registerForActivityResult
            }

            IndeterminateProgressDialogFragment.newInstance(
                requireActivity(),
                R.string.installing_driver,
                false
            ) {
                // Ignore file exceptions when a user selects an invalid zip
                val driverFile: DocumentFile
                try {
                    driverFile = GpuDriverHelper.copyDriverToExternalStorage(result)
                        ?: throw IOException("Driver failed validation!")
                } catch (_: IOException) {
                    return@newInstance getString(R.string.select_gpu_driver_error)
                }

                val driverData = GpuDriverHelper.getMetadataFromZip(driverFile.inputStream())
                val driverInList =
                    driverViewModel.driverList.value.firstOrNull { it.second == driverData }
                if (driverInList != null) {
                    driverFile.delete()
                    return@newInstance getString(R.string.driver_already_installed)
                } else {
                    driverViewModel.addDriver(Pair(driverFile.uri, driverData))
                    driverViewModel.setNewDriverInstalled(true)
                }
                return@newInstance Any()
            }.show(childFragmentManager, IndeterminateProgressDialogFragment.TAG)
        }
}
