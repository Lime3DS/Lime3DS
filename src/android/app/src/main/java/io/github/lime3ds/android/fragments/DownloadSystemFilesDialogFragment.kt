// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import io.github.lime3ds.android.NativeLibrary.InstallStatus
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.DialogProgressBarBinding
import io.github.lime3ds.android.viewmodel.GamesViewModel
import io.github.lime3ds.android.viewmodel.SystemFilesViewModel

class DownloadSystemFilesDialogFragment : DialogFragment() {
    private var _binding: DialogProgressBarBinding? = null
    private val binding get() = _binding!!

    private val downloadViewModel: SystemFilesViewModel by activityViewModels()
    private val gamesViewModel: GamesViewModel by activityViewModels()

    private lateinit var titles: LongArray

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogProgressBarBinding.inflate(layoutInflater)

        titles = requireArguments().getLongArray(TITLES)!!

        binding.progressText.visibility = View.GONE

        binding.progressBar.min = 0
        binding.progressBar.max = titles.size
        if (downloadViewModel.isDownloading.value != true) {
            binding.progressBar.progress = 0
        }

        isCancelable = false
        return MaterialAlertDialogBuilder(requireContext())
            .setView(binding.root)
            .setTitle(R.string.downloading_files)
            .setMessage(R.string.downloading_files_description)
            .setNegativeButton(android.R.string.cancel, null)
            .create()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    downloadViewModel.progress.collectLatest { binding.progressBar.progress = it }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    downloadViewModel.result.collect {
                        when (it) {
                            InstallStatus.Success -> {
                                downloadViewModel.clear()
                                dismiss()
                                MessageDialogFragment.newInstance(R.string.download_success, 0)
                                    .show(requireActivity().supportFragmentManager, MessageDialogFragment.TAG)
                                gamesViewModel.setShouldSwapData(true)
                            }

                            InstallStatus.ErrorFailedToOpenFile,
                            InstallStatus.ErrorEncrypted,
                            InstallStatus.ErrorFileNotFound,
                            InstallStatus.ErrorInvalid,
                            InstallStatus.ErrorAborted -> {
                                downloadViewModel.clear()
                                dismiss()
                                MessageDialogFragment.newInstance(
                                    R.string.download_failed,
                                    R.string.download_failed_description
                                ).show(requireActivity().supportFragmentManager, MessageDialogFragment.TAG)
                                gamesViewModel.setShouldSwapData(true)
                            }

                            InstallStatus.Cancelled -> {
                                downloadViewModel.clear()
                                dismiss()
                                MessageDialogFragment.newInstance(
                                    R.string.download_cancelled,
                                    R.string.download_cancelled_description
                                ).show(requireActivity().supportFragmentManager, MessageDialogFragment.TAG)
                            }

                            // Do nothing on null
                            else -> {}
                        }
                    }
                }
            }
        }

        // Consider using WorkManager here. While the home menu can only really amount to
        // about 150MBs, this could be a problem on inconsistent networks
        downloadViewModel.download(titles)
    }

    override fun onResume() {
        super.onResume()
        val alertDialog = dialog as AlertDialog
        val negativeButton = alertDialog.getButton(Dialog.BUTTON_NEGATIVE)
        negativeButton.setOnClickListener {
            downloadViewModel.cancel()
            dialog?.setTitle(R.string.cancelling)
            binding.progressBar.isIndeterminate = true
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        const val TAG = "DownloadSystemFilesDialogFragment"

        const val TITLES = "Titles"

        fun newInstance(titles: LongArray): DownloadSystemFilesDialogFragment {
            val dialog = DownloadSystemFilesDialogFragment()
            val args = Bundle()
            args.putLongArray(TITLES, titles)
            dialog.arguments = args
            return dialog
        }
    }
}
