// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.text.InputFilter
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import io.github.lime3ds.android.R
import io.github.lime3ds.android.applets.SoftwareKeyboard
import io.github.lime3ds.android.databinding.DialogSoftwareKeyboardBinding
import io.github.lime3ds.android.utils.SerializableHelper.serializable

class KeyboardDialogFragment : DialogFragment() {
    private lateinit var config: SoftwareKeyboard.KeyboardConfig

    private var _binding: DialogSoftwareKeyboardBinding? = null
    private val binding get() = _binding!!

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogSoftwareKeyboardBinding.inflate(layoutInflater)

        config = requireArguments().serializable<SoftwareKeyboard.KeyboardConfig>(CONFIG)!!

        binding.apply {
            editText.hint = config.hintText
            editTextInput.isSingleLine = !config.multilineMode
            editTextInput.filters =
                arrayOf(SoftwareKeyboard.Filter(), InputFilter.LengthFilter(config.maxTextLength))
        }

        val builder = MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.software_keyboard)
            .setView(binding.root)

        isCancelable = false

        when (config.buttonConfig) {
            SoftwareKeyboard.ButtonConfig.Triple -> {
                val negativeText =
                    config.buttonText[0].ifEmpty { getString(android.R.string.cancel) }
                val neutralText = config.buttonText[1].ifEmpty { getString(R.string.i_forgot) }
                val positiveText = config.buttonText[2].ifEmpty { getString(android.R.string.ok) }
                builder.setNegativeButton(negativeText, null)
                    .setNeutralButton(neutralText, null)
                    .setPositiveButton(positiveText, null)
            }

            SoftwareKeyboard.ButtonConfig.Dual -> {
                val negativeText =
                    config.buttonText[0].ifEmpty { getString(android.R.string.cancel) }
                val positiveText = config.buttonText[2].ifEmpty { getString(android.R.string.ok) }
                builder.setNegativeButton(negativeText, null)
                    .setPositiveButton(positiveText, null)
            }

            SoftwareKeyboard.ButtonConfig.Single -> {
                val positiveText = config.buttonText[2].ifEmpty { getString(android.R.string.ok) }
                builder.setPositiveButton(positiveText, null)
            }
        }

        // This overrides the default alert dialog behavior to prevent dismissing the keyboard
        // dialog while we show an error message
        val alertDialog = builder.create()
        alertDialog.create()
        if (alertDialog.getButton(DialogInterface.BUTTON_POSITIVE) != null) {
            alertDialog.getButton(DialogInterface.BUTTON_POSITIVE).setOnClickListener {
                SoftwareKeyboard.data.button = config.buttonConfig
                SoftwareKeyboard.data.text = binding.editTextInput.text.toString()
                val error = SoftwareKeyboard.ValidateInput(SoftwareKeyboard.data.text)
                if (error != SoftwareKeyboard.ValidationError.None) {
                    SoftwareKeyboard.HandleValidationError(config, error)
                    return@setOnClickListener
                }
                dismiss()
                synchronized(SoftwareKeyboard.finishLock) { SoftwareKeyboard.finishLock.notifyAll() }
            }
        }
        if (alertDialog.getButton(DialogInterface.BUTTON_NEUTRAL) != null) {
            alertDialog.getButton(DialogInterface.BUTTON_NEUTRAL).setOnClickListener {
                SoftwareKeyboard.data.button = 1
                dismiss()
                synchronized(SoftwareKeyboard.finishLock) { SoftwareKeyboard.finishLock.notifyAll() }
            }
        }
        if (alertDialog.getButton(DialogInterface.BUTTON_NEGATIVE) != null) {
            alertDialog.getButton(DialogInterface.BUTTON_NEGATIVE).setOnClickListener {
                SoftwareKeyboard.data.button = 0
                dismiss()
                synchronized(SoftwareKeyboard.finishLock) { SoftwareKeyboard.finishLock.notifyAll() }
            }
        }

        return alertDialog
    }

    companion object {
        const val TAG = "KeyboardDialogFragment"

        const val CONFIG = "config"

        fun newInstance(config: SoftwareKeyboard.KeyboardConfig): KeyboardDialogFragment {
            val frag = KeyboardDialogFragment()
            val args = Bundle()
            args.putSerializable(CONFIG, config)
            frag.arguments = args
            return frag
        }
    }
}
