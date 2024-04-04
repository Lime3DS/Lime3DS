// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.cheats.ui

import android.annotation.SuppressLint
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.FragmentCheatDetailsBinding
import io.github.lime3ds.android.features.cheats.model.Cheat
import io.github.lime3ds.android.features.cheats.model.CheatsViewModel

class CheatDetailsFragment : Fragment() {
    private val cheatsViewModel: CheatsViewModel by activityViewModels()

    private var _binding: FragmentCheatDetailsBinding? = null
    private val binding get() = _binding!!

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentCheatDetailsBinding.inflate(layoutInflater)
        return binding.root
    }

    // This is using the correct scope, lint is just acting up
    @SuppressLint("UnsafeRepeatOnLifecycleDetector")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.selectedCheat.collect { onSelectedCheatUpdated(it) }
                }
            }
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    cheatsViewModel.isEditing.collect { onIsEditingUpdated(it) }
                }
            }
        }
        binding.buttonDelete.setOnClickListener { onDeleteClicked() }
        binding.buttonEdit.setOnClickListener { onEditClicked() }
        binding.buttonCancel.setOnClickListener { onCancelClicked() }
        binding.buttonOk.setOnClickListener { onOkClicked() }

        // On a portrait phone screen (or other narrow screen), only one of the two panes are shown
        // at the same time. If the user is navigating using a d-pad and moves focus to an element
        // in the currently hidden pane, we need to manually show that pane.
        CheatsActivity.setOnFocusChangeListenerRecursively(view) { _, hasFocus ->
            cheatsViewModel.onDetailsViewFocusChanged(hasFocus)
        }

        binding.toolbarCheatDetails.setNavigationOnClickListener {
            cheatsViewModel.closeDetailsView()
        }

        setInsets()
    }

    override fun onDestroy() {
        super.onDestroy()
        _binding = null
    }

    private fun clearEditErrors() {
        binding.editName.error = null
        binding.editCode.error = null
    }

    private fun onDeleteClicked() {
        val name = binding.editNameInput.text.toString()
        MaterialAlertDialogBuilder(requireContext())
            .setMessage(getString(R.string.cheats_delete_confirmation, name))
            .setPositiveButton(
                android.R.string.ok
            ) { _: DialogInterface?, _: Int -> cheatsViewModel.deleteSelectedCheat() }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun onEditClicked() {
        cheatsViewModel.setIsEditing(true)
        binding.buttonOk.requestFocus()
    }

    private fun onCancelClicked() {
        cheatsViewModel.setIsEditing(false)
        onSelectedCheatUpdated(cheatsViewModel.selectedCheat.value)
        binding.buttonDelete.requestFocus()
        cheatsViewModel.closeDetailsView()
    }

    private fun onOkClicked() {
        clearEditErrors()
        val name = binding.editNameInput.text.toString()
        val notes = binding.editNotesInput.text.toString()
        val code = binding.editCodeInput.text.toString()
        if (name.isEmpty()) {
            binding.editName.error = getString(R.string.cheats_error_no_name)
            binding.scrollView.smoothScrollTo(0, binding.editNameInput.top)
            return
        } else if (code.isEmpty()) {
            binding.editCode.error = getString(R.string.cheats_error_no_code_lines)
            binding.scrollView.smoothScrollTo(0, binding.editCodeInput.bottom)
            return
        }
        val validityResult = Cheat.isValidGatewayCode(code)
        if (validityResult != 0) {
            binding.editCode.error = getString(R.string.cheats_error_on_line, validityResult)
            binding.scrollView.smoothScrollTo(0, binding.editCodeInput.bottom)
            return
        }
        val newCheat = Cheat.createGatewayCode(name, notes, code)
        if (cheatsViewModel.isAdding.value == true) {
            cheatsViewModel.finishAddingCheat(newCheat)
        } else {
            cheatsViewModel.updateSelectedCheat(newCheat)
        }
        binding.buttonEdit.requestFocus()
    }

    private fun onSelectedCheatUpdated(cheat: Cheat?) {
        clearEditErrors()
        val isEditing: Boolean = cheatsViewModel.isEditing.value == true

        // If the fragment was recreated while editing a cheat, it's vital that we
        // don't repopulate the fields, otherwise the user's changes will be lost
        if (!isEditing) {
            if (cheat == null) {
                binding.editNameInput.setText("")
                binding.editNotesInput.setText("")
                binding.editCodeInput.setText("")
            } else {
                binding.editNameInput.setText(cheat.getName())
                binding.editNotesInput.setText(cheat.getNotes())
                binding.editCodeInput.setText(cheat.getCode())
            }
        }
    }

    private fun onIsEditingUpdated(isEditing: Boolean) {
        if (isEditing) {
            binding.root.visibility = View.VISIBLE
        }
        binding.editNameInput.isEnabled = isEditing
        binding.editNotesInput.isEnabled = isEditing
        binding.editCodeInput.isEnabled = isEditing

        binding.buttonDelete.visibility = if (isEditing) View.GONE else View.VISIBLE
        binding.buttonEdit.visibility = if (isEditing) View.GONE else View.VISIBLE
        binding.buttonCancel.visibility = if (isEditing) View.VISIBLE else View.GONE
        binding.buttonOk.visibility = if (isEditing) View.VISIBLE else View.GONE
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View?, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val mlpAppBar = binding.toolbarCheatDetails.layoutParams as ViewGroup.MarginLayoutParams
            mlpAppBar.leftMargin = leftInsets
            mlpAppBar.rightMargin = rightInsets
            binding.toolbarCheatDetails.layoutParams = mlpAppBar

            binding.scrollView.updatePadding(left = leftInsets, right = rightInsets)
            binding.buttonContainer.updatePadding(left = leftInsets, right = rightInsets)

            windowInsets
        }
}
