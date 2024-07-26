// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.fragments

import android.content.DialogInterface
import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.DialogInputBinding
import io.github.lime3ds.android.features.settings.model.view.InputBindingSetting
import io.github.lime3ds.android.utils.Log
import kotlin.math.abs

class MotionBottomSheetDialogFragment : BottomSheetDialogFragment() {
    private var _binding: DialogInputBinding? = null
    private val binding get() = _binding!!

    private var setting: InputBindingSetting? = null
    private var onCancel: (() -> Unit)? = null
    private var onDismiss: (() -> Unit)? = null

    private val previousValues = ArrayList<Float>()
    private var prevDeviceId = 0
    private var waitingForEvent = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (setting == null) {
            dismiss()
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogInputBinding.inflate(inflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        BottomSheetBehavior.from<View>(view.parent as View).state =
            BottomSheetBehavior.STATE_EXPANDED

        isCancelable = false
        view.requestFocus()
        view.setOnFocusChangeListener { v, hasFocus -> if (!hasFocus) v.requestFocus() }
        if (setting!!.isButtonMappingSupported()) {
            dialog?.setOnKeyListener { _, _, event -> onKeyEvent(event) }
        }
        if (setting!!.isAxisMappingSupported()) {
            binding.root.setOnGenericMotionListener { _, event -> onMotionEvent(event) }
        }

        val inputTypeId = when {
            setting!!.isCirclePad() -> R.string.controller_circlepad
            setting!!.isCStick() -> R.string.controller_c
            setting!!.isDPad() -> R.string.controller_dpad
            setting!!.isTrigger() -> R.string.controller_trigger
            else -> R.string.button
        }
        binding.textTitle.text =
            String.format(
                getString(R.string.input_dialog_title),
                getString(inputTypeId),
                getString(setting!!.nameId)
            )

        var messageResId: Int = R.string.input_dialog_description
        if (setting!!.isAxisMappingSupported() && !setting!!.isTrigger()) {
            // Use specialized message for axis left/right or up/down
            messageResId = if (setting!!.isHorizontalOrientation()) {
                R.string.input_binding_description_horizontal_axis
            } else {
                R.string.input_binding_description_vertical_axis
            }
        }
        binding.textMessage.text = getString(messageResId)

        binding.buttonClear.setOnClickListener {
            setting?.removeOldMapping()
            dismiss()
        }
        binding.buttonCancel.setOnClickListener {
            onCancel?.invoke()
            dismiss()
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    override fun onDismiss(dialog: DialogInterface) {
        super.onDismiss(dialog)
        onDismiss?.invoke()
    }

    private fun onKeyEvent(event: KeyEvent): Boolean {
        Log.debug("[MotionBottomSheetDialogFragment] Received key event: " + event.action)
        return when (event.action) {
            KeyEvent.ACTION_UP -> {
                setting?.onKeyInput(event)
                dismiss()
                // Even if we ignore the key, we still consume it. Thus return true regardless.
                true
            }

            else -> false
        }
    }

    private fun onMotionEvent(event: MotionEvent): Boolean {
        Log.debug("[MotionBottomSheetDialogFragment] Received motion event: " + event.action)
        if (event.source and InputDevice.SOURCE_CLASS_JOYSTICK == 0) return false
        if (event.action != MotionEvent.ACTION_MOVE) return false

        val input = event.device

        val motionRanges = input.motionRanges

        if (input.id != prevDeviceId) {
            previousValues.clear()
        }
        prevDeviceId = input.id
        val firstEvent = previousValues.isEmpty()

        var numMovedAxis = 0
        var axisMoveValue = 0.0f
        var lastMovedRange: InputDevice.MotionRange? = null
        var lastMovedDir = '?'
        if (waitingForEvent) {
            for (i in motionRanges.indices) {
                val range = motionRanges[i]
                val axis = range.axis
                val origValue = event.getAxisValue(axis)
                if (firstEvent) {
                    previousValues.add(origValue)
                } else {
                    val previousValue = previousValues[i]

                    // Only handle the axes that are not neutral (more than 0.5)
                    // but ignore any axis that has a constant value (e.g. always 1)
                    if (abs(origValue) > 0.5f && origValue != previousValue) {
                        // It is common to have multiple axes with the same physical input. For example,
                        // shoulder butters are provided as both AXIS_LTRIGGER and AXIS_BRAKE.
                        // To handle this, we ignore an axis motion that's the exact same as a motion
                        // we already saw. This way, we ignore axes with two names, but catch the case
                        // where a joystick is moved in two directions.
                        // ref: bottom of https://developer.android.com/training/game-controllers/controller-input.html
                        if (origValue != axisMoveValue) {
                            axisMoveValue = origValue
                            numMovedAxis++
                            lastMovedRange = range
                            lastMovedDir = if (origValue < 0.0f) '-' else '+'
                        }
                    } else if (abs(origValue) < 0.25f && abs(previousValue) > 0.75f) {
                        // Special case for d-pads (axis value jumps between 0 and 1 without any values
                        // in between). Without this, the user would need to press the d-pad twice
                        // due to the first press being caught by the "if (firstEvent)" case further up.
                        numMovedAxis++
                        lastMovedRange = range
                        lastMovedDir = if (previousValue < 0.0f) '-' else '+'
                    }
                }
                previousValues[i] = origValue
            }

            // If only one axis moved, that's the winner.
            if (numMovedAxis == 1) {
                waitingForEvent = false
                setting?.onMotionInput(input, lastMovedRange!!, lastMovedDir)
                dismiss()
            }
        }
        return true
    }

    companion object {
        const val TAG = "MotionBottomSheetDialogFragment"

        fun newInstance(
            setting: InputBindingSetting,
            onCancel: () -> Unit,
            onDismiss: () -> Unit
        ): MotionBottomSheetDialogFragment {
            val dialog = MotionBottomSheetDialogFragment()
            dialog.apply {
                this.setting = setting
                this.onCancel = onCancel
                this.onDismiss = onDismiss
            }
            return dialog
        }
    }
}
