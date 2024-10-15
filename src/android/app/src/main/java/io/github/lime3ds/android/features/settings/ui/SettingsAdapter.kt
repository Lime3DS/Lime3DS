// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.ui

import android.annotation.SuppressLint
import android.content.Context
import android.content.DialogInterface
import android.graphics.Color
import android.icu.util.Calendar
import android.icu.util.TimeZone
import android.text.Editable
import android.text.InputFilter
import android.text.InputType
import android.text.TextWatcher
import android.text.format.DateFormat
import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.EditText
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.datepicker.MaterialDatePicker
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import com.google.android.material.textfield.TextInputEditText
import com.google.android.material.textfield.TextInputLayout
import com.google.android.material.timepicker.MaterialTimePicker
import com.google.android.material.timepicker.TimeFormat
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.DialogSliderBinding
import io.github.lime3ds.android.databinding.DialogSoftwareKeyboardBinding
import io.github.lime3ds.android.databinding.ListItemSettingBinding
import io.github.lime3ds.android.databinding.ListItemSettingSwitchBinding
import io.github.lime3ds.android.databinding.ListItemSettingsHeaderBinding
import io.github.lime3ds.android.features.settings.model.AbstractBooleanSetting
import io.github.lime3ds.android.features.settings.model.AbstractFloatSetting
import io.github.lime3ds.android.features.settings.model.AbstractIntSetting
import io.github.lime3ds.android.features.settings.model.AbstractSetting
import io.github.lime3ds.android.features.settings.model.AbstractStringSetting
import io.github.lime3ds.android.features.settings.model.FloatSetting
import io.github.lime3ds.android.features.settings.model.ScaledFloatSetting
import io.github.lime3ds.android.features.settings.model.AbstractShortSetting
import io.github.lime3ds.android.features.settings.model.view.DateTimeSetting
import io.github.lime3ds.android.features.settings.model.view.InputBindingSetting
import io.github.lime3ds.android.features.settings.model.view.SettingsItem
import io.github.lime3ds.android.features.settings.model.view.SingleChoiceSetting
import io.github.lime3ds.android.features.settings.model.view.SliderSetting
import io.github.lime3ds.android.features.settings.model.view.StringInputSetting
import io.github.lime3ds.android.features.settings.model.view.StringSingleChoiceSetting
import io.github.lime3ds.android.features.settings.model.view.SubmenuSetting
import io.github.lime3ds.android.features.settings.model.view.SwitchSetting
import io.github.lime3ds.android.features.settings.ui.viewholder.DateTimeViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.HeaderViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.InputBindingSettingViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.RunnableViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.SettingViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.SingleChoiceViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.SliderViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.StringInputViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.SubmenuViewHolder
import io.github.lime3ds.android.features.settings.ui.viewholder.SwitchSettingViewHolder
import io.github.lime3ds.android.fragments.MessageDialogFragment
import io.github.lime3ds.android.fragments.MotionBottomSheetDialogFragment
import io.github.lime3ds.android.utils.SystemSaveGame
import java.lang.IllegalStateException
import java.lang.NumberFormatException
import java.text.SimpleDateFormat
import kotlin.math.roundToInt

class SettingsAdapter(
    private val fragmentView: SettingsFragmentView,
    private val context: Context
) : RecyclerView.Adapter<SettingViewHolder?>(), DialogInterface.OnClickListener {
    private var settings: ArrayList<SettingsItem>? = null
    private var clickedItem: SettingsItem? = null
    private var clickedPosition: Int
    private var dialog: AlertDialog? = null
    private var sliderProgress = 0f
    private var textSliderValue: TextInputEditText? = null
    private var textInputLayout: TextInputLayout? = null
    private var textInputValue: String = ""

    private var defaultCancelListener =
        DialogInterface.OnClickListener { _: DialogInterface?, _: Int -> closeDialog() }

    init {
        clickedPosition = -1
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SettingViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            SettingsItem.TYPE_HEADER -> {
                HeaderViewHolder(ListItemSettingsHeaderBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SWITCH -> {
                SwitchSettingViewHolder(ListItemSettingSwitchBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SINGLE_CHOICE, SettingsItem.TYPE_STRING_SINGLE_CHOICE -> {
                SingleChoiceViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SLIDER -> {
                SliderViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_SUBMENU -> {
                SubmenuViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_DATETIME_SETTING -> {
                DateTimeViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_RUNNABLE -> {
                RunnableViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_INPUT_BINDING -> {
                InputBindingSettingViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            SettingsItem.TYPE_STRING_INPUT -> {
                StringInputViewHolder(ListItemSettingBinding.inflate(inflater), this)
            }

            else -> {
                // TODO: Create an error view since we can't return null now
                HeaderViewHolder(ListItemSettingsHeaderBinding.inflate(inflater), this)
            }
        }
    }

    override fun onBindViewHolder(holder: SettingViewHolder, position: Int) {
        getItem(position)?.let { holder.bind(it) }
    }

    private fun getItem(position: Int): SettingsItem? {
        return settings?.get(position)
    }

    override fun getItemCount(): Int {
        return settings?.size ?: 0
    }

    override fun getItemViewType(position: Int): Int {
        return getItem(position)?.type ?: -1
    }

    fun setSettingsList(settings: ArrayList<SettingsItem>?) {
        this.settings = settings ?: arrayListOf()
        notifyDataSetChanged()
    }

    fun onBooleanClick(item: SwitchSetting, position: Int, checked: Boolean) {
        val setting = item.setChecked(checked)
        fragmentView.putSetting(setting)
        fragmentView.onSettingChanged()
    }

    private fun onSingleChoiceClick(item: SingleChoiceSetting) {
        clickedItem = item
        val value = getSelectionForSingleChoiceValue(item)
        dialog = MaterialAlertDialogBuilder(context)
            .setTitle(item.nameId)
            .setSingleChoiceItems(item.choicesId, value, this)
            .show()
    }

    fun onSingleChoiceClick(item: SingleChoiceSetting, position: Int) {
        clickedPosition = position
        onSingleChoiceClick(item)
    }

    private fun onStringSingleChoiceClick(item: StringSingleChoiceSetting) {
        clickedItem = item
        dialog = context?.let {
            MaterialAlertDialogBuilder(it)
                .setTitle(item.nameId)
                .setSingleChoiceItems(item.choices, item.selectValueIndex, this)
                .show()
        }
    }

    fun onStringSingleChoiceClick(item: StringSingleChoiceSetting, position: Int) {
        clickedPosition = position
        onStringSingleChoiceClick(item)
    }

    @SuppressLint("SimpleDateFormat")
    fun onDateTimeClick(item: DateTimeSetting, position: Int) {
        clickedItem = item
        clickedPosition = position

        val storedTime: Long = try {
            java.lang.Long.decode(item.value) * 1000
        } catch (e: NumberFormatException) {
            val date = item.value.substringBefore(" ")
            val time = item.value.substringAfter(" ")

            val formatter = SimpleDateFormat("yyyy-MM-dd'T'hh:mm:ssZZZZ")
            val gmt = formatter.parse("${date}T${time}+0000")
            gmt!!.time
        }

        // Helper to extract hour and minute from epoch time
        val calendar: Calendar = Calendar.getInstance()
        calendar.timeInMillis = storedTime
        calendar.timeZone = TimeZone.getTimeZone("UTC")

        var timeFormat: Int = TimeFormat.CLOCK_12H
        if (DateFormat.is24HourFormat(fragmentView.activityView as AppCompatActivity)) {
            timeFormat = TimeFormat.CLOCK_24H
        }

        val datePicker: MaterialDatePicker<Long> = MaterialDatePicker.Builder.datePicker()
            .setSelection(storedTime)
            .setTitleText(R.string.select_rtc_date)
            .build()
        val timePicker: MaterialTimePicker = MaterialTimePicker.Builder()
            .setTimeFormat(timeFormat)
            .setHour(calendar.get(Calendar.HOUR_OF_DAY))
            .setMinute(calendar.get(Calendar.MINUTE))
            .setTitleText(R.string.select_rtc_time)
            .build()

        datePicker.addOnPositiveButtonClickListener {
            val activity = fragmentView.activityView as? AppCompatActivity
            activity?.supportFragmentManager?.let { fragmentManager ->
                timePicker.show(fragmentManager, "TimePicker")
            }
        }
        timePicker.addOnPositiveButtonClickListener {
            var epochTime: Long = datePicker.selection!! / 1000
            epochTime += timePicker.hour.toLong() * 60 * 60
            epochTime += timePicker.minute.toLong() * 60
            val rtcString = epochTime.toString()
            if (item.value != rtcString) {
                fragmentView.onSettingChanged()
            }
            notifyItemChanged(clickedPosition)
            val setting = item.setSelectedValue(rtcString)
            fragmentView.putSetting(setting)
            clickedItem = null
        }
        datePicker.show(
            (fragmentView.activityView as AppCompatActivity).supportFragmentManager,
            "DatePicker"
        )
    }

    fun onSliderClick(item: SliderSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        sliderProgress = (item.selectedFloat * 100f).roundToInt() / 100f


        val inflater = LayoutInflater.from(context)
        val sliderBinding = DialogSliderBinding.inflate(inflater)
        textInputLayout = sliderBinding.textInput
        textSliderValue = sliderBinding.textValue
        if (item.setting is FloatSetting) {
            textSliderValue?.let {
                it.inputType = InputType.TYPE_CLASS_NUMBER or InputType.TYPE_NUMBER_FLAG_DECIMAL
                it.setText(sliderProgress.toString())
            }
        } else {
            textSliderValue?.setText(sliderProgress.roundToInt().toString())
        }

        textInputLayout?.suffixText = item.units

        sliderBinding.slider.apply {
            valueFrom = item.min.toFloat()
            valueTo = item.max.toFloat()
            value = sliderProgress
            textSliderValue?.addTextChangedListener(object : TextWatcher {
                override fun afterTextChanged(s: Editable) {
                    var textValue = s.toString().toFloatOrNull();
                    if (item.setting !is FloatSetting) {
                        textValue = textValue?.roundToInt()?.toFloat();
                    }
                    if (textValue == null || textValue < valueFrom || textValue > valueTo) {
                        textInputLayout?.error = "Inappropriate value"
                    } else {
                        textInputLayout?.error = null
                        value = textValue
                    }
                }

                override fun beforeTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
                override fun onTextChanged(p0: CharSequence?, p1: Int, p2: Int, p3: Int) {}
            })

            addOnChangeListener { _: Slider, value: Float, _: Boolean ->
                sliderProgress = (value * 100).roundToInt().toFloat() / 100f
                var sliderString = sliderProgress.toString()
                if (item.setting !is FloatSetting) {
                    sliderString = sliderProgress.roundToInt().toString()
                    if (textSliderValue?.text.toString() != sliderString) {
                        textSliderValue?.setText(sliderString)
                        textSliderValue?.setSelection(textSliderValue?.length() ?: 0 )
                    }
                } else {
                    val currentText = textSliderValue?.text.toString()
                    val currentTextValue = currentText.toFloat()
                    if (currentTextValue != sliderProgress) {
                        textSliderValue?.setText(sliderString)
                        textSliderValue?.setSelection(textSliderValue?.length() ?: 0 )
                    }
                }
            }
        }

        dialog = MaterialAlertDialogBuilder(context)
            .setTitle(item.nameId)
            .setView(sliderBinding.root)
            .setPositiveButton(android.R.string.ok, this)
            .setNegativeButton(android.R.string.cancel, defaultCancelListener)
            .setNeutralButton(R.string.slider_default) { dialog: DialogInterface, which: Int ->
                sliderBinding.slider?.value = when (item.setting) {
                    is ScaledFloatSetting -> {
                        val scaledSetting = item.setting as ScaledFloatSetting
                        scaledSetting.defaultValue * scaledSetting.scale
                    }

                    is FloatSetting -> (item.setting as FloatSetting).defaultValue
                    else -> item.defaultValue ?: 0f
                }
                onClick(dialog, which)
            }
            .show()
    }

    fun onSubmenuClick(item: SubmenuSetting) {
        fragmentView.loadSubMenu(item.menuKey)
    }

    fun onInputBindingClick(item: InputBindingSetting, position: Int) {
        val activity = fragmentView.activityView as FragmentActivity
        MotionBottomSheetDialogFragment.newInstance(
            item,
            { closeDialog() },
            {
                notifyItemChanged(position)
                fragmentView.onSettingChanged()
            }
        ).show(activity.supportFragmentManager, MotionBottomSheetDialogFragment.TAG)
    }

    fun onStringInputClick(item: StringInputSetting, position: Int) {
        clickedItem = item
        clickedPosition = position
        textInputValue = item.selectedValue

        val inflater = LayoutInflater.from(context)
        val inputBinding = DialogSoftwareKeyboardBinding.inflate(inflater)

        inputBinding.editTextInput.setText(textInputValue)
        inputBinding.editTextInput.doOnTextChanged { text, _, _, _ ->
            textInputValue = text.toString()
        }
        if (item.characterLimit != 0) {
            inputBinding.editTextInput.filters =
                arrayOf(InputFilter.LengthFilter(item.characterLimit))
        }

        dialog = MaterialAlertDialogBuilder(context)
            .setView(inputBinding.root)
            .setTitle(item.nameId)
            .setPositiveButton(android.R.string.ok, this)
            .setNegativeButton(android.R.string.cancel, defaultCancelListener)
            .show()
    }

    override fun onClick(dialog: DialogInterface, which: Int) {
        when (clickedItem) {
            is SingleChoiceSetting -> {
                val scSetting = clickedItem as? SingleChoiceSetting
                scSetting?.let {
                        val setting = when (it.setting) {
                        is AbstractIntSetting -> {
                        val value = getValueForSingleChoiceSelection(it, which)
                        if (it.selectedValue != value) {
                            fragmentView?.onSettingChanged()
                        }
                            it.setSelectedValue(value)
                        }
                        is AbstractShortSetting -> {
                            val value = getValueForSingleChoiceSelection(it, which).toShort()
                            if (it.selectedValue.toShort() != value) {
                                fragmentView?.onSettingChanged()
                            }
                            it.setSelectedValue(value)
                        }
                        else -> throw IllegalStateException("Unrecognized type used for SingleChoiceSetting!")
                    }
                    fragmentView?.putSetting(setting)
                    closeDialog()
                }
            }

            is StringSingleChoiceSetting -> {
                val scSetting = clickedItem as? StringSingleChoiceSetting
                scSetting?.let {
                    val setting = when (it.setting) {
                        is AbstractStringSetting -> {
                            val value = it.getValueAt(which)
                            if (it.selectedValue != value) fragmentView?.onSettingChanged()
                            it.setSelectedValue(value ?: "")
                        }

                        is AbstractShortSetting -> {
                            if (it.selectValueIndex != which) fragmentView?.onSettingChanged()
                            it.setSelectedValue(it.getValueAt(which)?.toShort() ?: 1)
                        }

                        else -> throw IllegalStateException("Unrecognized type used for StringSingleChoiceSetting!")
                    }

                    fragmentView?.putSetting(setting)
                    closeDialog()
                }
            }

            is SliderSetting -> {
                val sliderSetting = clickedItem as? SliderSetting
                sliderSetting?.let {
                    val sliderval = (it.selectedFloat * 100).roundToInt().toFloat() / 100
                    if (sliderval != sliderProgress) {
                        fragmentView?.onSettingChanged()
                    }
                    when (it.setting) {
                        is AbstractIntSetting -> {
                            val value = sliderProgress.roundToInt()
                            val setting = it.setSelectedValue(value)
                            fragmentView?.putSetting(setting)
                        }
                        else -> {
                            val setting = it.setSelectedValue(sliderProgress)
                            fragmentView?.putSetting(setting)
                        }
                   }
                    closeDialog()
                }
            }

            is StringInputSetting -> {
                val inputSetting = clickedItem as? StringInputSetting
                inputSetting?.let {
                    if (it.selectedValue != textInputValue) {
                        fragmentView?.onSettingChanged()
                    }
                    val setting = it.setSelectedValue(textInputValue ?: "")
                    fragmentView?.putSetting(setting)
                    closeDialog()
               }
            }
        }
        clickedItem = null
        sliderProgress = -1f
        textInputValue = ""
    }

    fun onLongClick(setting: AbstractSetting, position: Int): Boolean {
        MaterialAlertDialogBuilder(context)
            .setMessage(R.string.reset_setting_confirmation)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                when (setting) {
                    is AbstractBooleanSetting -> setting.boolean = setting.defaultValue as Boolean
                    is AbstractFloatSetting -> {
                        if (setting is ScaledFloatSetting) {
                            setting.float = setting.defaultValue * setting.scale
                        } else {
                            setting.float = setting.defaultValue as Float
                        }
                    }

                    is AbstractIntSetting -> setting.int = setting.defaultValue as Int
                    is AbstractStringSetting -> setting.string = setting.defaultValue as String
                    is AbstractShortSetting -> setting.short = setting.defaultValue as Short
                }
                notifyItemChanged(position)
                fragmentView.onSettingChanged()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()

        return true
    }

    fun onClickDisabledSetting() {
        MessageDialogFragment.newInstance(
            R.string.setting_not_editable,
            R.string.setting_not_editable_description
        ).show((fragmentView as SettingsFragment).childFragmentManager, MessageDialogFragment.TAG)
    }

    fun onClickRegenerateConsoleId() {
        MaterialAlertDialogBuilder(context)
            .setTitle(R.string.regenerate_console_id)
            .setMessage(R.string.regenerate_console_id_description)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                SystemSaveGame.regenerateConsoleId()
                notifyDataSetChanged()
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    fun closeDialog() {
        if (dialog != null) {
            if (clickedPosition != -1) {
                notifyItemChanged(clickedPosition)
                clickedPosition = -1
            }
            dialog!!.dismiss()
            dialog = null
        }
    }

    private fun getValueForSingleChoiceSelection(item: SingleChoiceSetting, which: Int): Int {
        val valuesId = item.valuesId
        return if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            valuesArray[which]
        } else {
            which
        }
    }

    private fun getSelectionForSingleChoiceValue(item: SingleChoiceSetting): Int {
        val value = item.selectedValue
        val valuesId = item.valuesId
        if (valuesId > 0) {
            val valuesArray = context.resources.getIntArray(valuesId)
            for (index in valuesArray.indices) {
                val current = valuesArray[index]
                if (current == value) {
                    return index
                }
            }
        } else {
            return value
        }
        return -1
    }
}
