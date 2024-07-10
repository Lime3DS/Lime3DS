package io.github.lime3ds.android.features.settings.ui

import android.app.AlertDialog
import android.content.Context
import android.content.SharedPreferences
import android.graphics.drawable.Drawable
import android.view.InputDevice
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import androidx.core.content.ContextCompat
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.DialogControllerQuickConfigBinding
import io.github.lime3ds.android.features.settings.model.view.InputBindingSetting
import kotlin.math.abs


class ControllerQuickConfigDialog(
    private var context: Context,
    buttons: ArrayList<List<String>>,
    titles: ArrayList<List<Int>>,
    private var preferences: SharedPreferences
) {

    private var index = 0
    val inflater = LayoutInflater.from(context)
    val automappingBinding = DialogControllerQuickConfigBinding.inflate(inflater)
    var dialog: AlertDialog? = null

    var allButtons = arrayListOf<String>()
    var allTitles = arrayListOf<Int>()

    init {
        buttons.forEach {group ->
            group.forEach {button ->
                allButtons.add(button)
            }
        }
        titles.forEach {group ->
            group.forEach {title ->
                allTitles.add(title)
            }
        }

    }

    fun show() {
        val builder: AlertDialog.Builder = AlertDialog.Builder(context)
        builder
            .setView(automappingBinding.root)
            .setTitle("Automapper")
            .setPositiveButton("Next") {_,_ -> }
            .setNegativeButton("Close") { dialog, which ->
                dialog.dismiss()
            }

        dialog = builder.create()
        dialog?.show()

        dialog?.setOnKeyListener { _, _, event -> onKeyEvent(event) }
        automappingBinding.root.setOnGenericMotionListener { _, event -> onMotionEvent(event) }

        // Prepare the first element
        prepareUIforIndex(index)

        val nextButton = dialog?.getButton(AlertDialog.BUTTON_POSITIVE)
        nextButton?.setOnClickListener {
            // Skip to next:
            prepareUIforIndex(index++)
        }

    }

    private fun prepareUIforIndex(i: Int) {
        if (allButtons.size-1 < i) {
            dialog?.dismiss()
            return
        }

        if(index>0) {
            automappingBinding.lastMappingIcon.visibility = View.VISIBLE
            automappingBinding.lastMappingDescription.visibility = View.VISIBLE
        }

        val currentButton = allButtons[i]
        val currentTitleInt = allTitles[i]

        val button = InputBindingSetting.getInputObject(currentButton, preferences)

        var lastTitle = setting?.value ?: ""
        if(lastTitle.isBlank()) {
            lastTitle = context.getString(R.string.controller_quick_config_unassigned)
        }
        automappingBinding.lastMappingDescription.text = lastTitle
        automappingBinding.lastMappingIcon.setImageDrawable(automappingBinding.currentMappingIcon.drawable)
        setting = InputBindingSetting(button, currentTitleInt)

        automappingBinding.currentMappingTitle.text = calculateTitle()
        automappingBinding.currentMappingDescription.text = setting?.value
        automappingBinding.currentMappingIcon.setImageDrawable(getIcon())


        if (allButtons.size-1 < index) {
            dialog?.getButton(AlertDialog.BUTTON_POSITIVE)?.text =
                context.getString(R.string.controller_quick_config_finish)
            dialog?.getButton(AlertDialog.BUTTON_NEGATIVE)?.visibility = View.GONE
        }

    }

    private fun calculateTitle(): String {

        val inputTypeId = when {
            setting!!.isCirclePad() -> R.string.controller_circlepad
            setting!!.isCStick() -> R.string.controller_c
            setting!!.isDPad() -> R.string.controller_dpad
            setting!!.isTrigger() -> R.string.controller_trigger
            else -> R.string.button
        }

        val nameId = setting?.nameId?.let { context.getString(it) }

        return String.format(
            context.getString(R.string.input_dialog_title),
            context.getString(inputTypeId),
            nameId
        )
    }

    private fun getIcon(): Drawable? {
        val id = when {
            setting!!.isCirclePad() -> R.drawable.stick_main
            setting!!.isCStick() -> R.drawable.stick_c
            setting!!.isDPad() -> R.drawable.dpad
            else -> {
                val resourceTitle = context.resources.getResourceEntryName(setting!!.nameId)
                if(resourceTitle.startsWith("direction")) {
                    R.drawable.dpad
                } else {
                    context.resources.getIdentifier(resourceTitle, "drawable", context.packageName)
                }
            }
        }
        return ContextCompat.getDrawable(context, id)
    }

    private val previousValues = ArrayList<Float>()
    private var prevDeviceId = 0
    private var waitingForEvent = true
    private var setting: InputBindingSetting? = null
    private var debounceTimestamp = System.currentTimeMillis()

    private fun onKeyEvent(event: KeyEvent): Boolean {
        return when (event.action) {
            KeyEvent.ACTION_UP -> {
                if(System.currentTimeMillis()-debounceTimestamp < 500) {
                    return true
                }

                debounceTimestamp = System.currentTimeMillis()

                index++
                setting?.onKeyInput(event)
                prepareUIforIndex(index)
                // Even if we ignore the key, we still consume it. Thus return true regardless.
                true
            }

            else -> false
        }
    }

    private fun onMotionEvent(event: MotionEvent): Boolean {
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
            }
        }
        return true
    }

}