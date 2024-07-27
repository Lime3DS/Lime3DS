// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.ui.viewholder

import android.annotation.SuppressLint
import android.view.View
import io.github.lime3ds.android.databinding.ListItemSettingBinding
import java.time.Instant
import java.time.ZoneId
import java.time.ZonedDateTime
import java.time.format.DateTimeFormatter
import java.time.format.FormatStyle
import io.github.lime3ds.android.features.settings.model.view.DateTimeSetting
import io.github.lime3ds.android.features.settings.model.view.SettingsItem
import io.github.lime3ds.android.features.settings.ui.SettingsAdapter
import java.text.SimpleDateFormat

class DateTimeViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: DateTimeSetting

    @SuppressLint("SimpleDateFormat")
    override fun bind(item: SettingsItem) {
        setting = item as DateTimeSetting
        binding.textSettingName.setText(item.nameId)
        if (item.descriptionId != 0) {
            binding.textSettingDescription.visibility = View.VISIBLE
            binding.textSettingDescription.setText(item.descriptionId)
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
        binding.textSettingValue.visibility = View.VISIBLE
        val epochTime = try {
            setting.value.toLong()
        } catch (e: NumberFormatException) {
            val date = setting.value.substringBefore(" ")
            val time = setting.value.substringAfter(" ")

            val formatter = SimpleDateFormat("yyyy-MM-dd'T'hh:mm:ssZZZZ")
            val gmt = formatter.parse("${date}T${time}+0000")
            gmt!!.time / 1000
        }
        val instant = Instant.ofEpochMilli(epochTime * 1000)
        val zonedTime = ZonedDateTime.ofInstant(instant, ZoneId.of("UTC"))
        val dateFormatter = DateTimeFormatter.ofLocalizedDateTime(FormatStyle.MEDIUM)
        binding.textSettingValue.text = dateFormatter.format(zonedTime)

        if (setting.isEditable) {
            binding.textSettingName.alpha = 1f
            binding.textSettingDescription.alpha = 1f
            binding.textSettingValue.alpha = 1f
        } else {
            binding.textSettingName.alpha = 0.5f
            binding.textSettingDescription.alpha = 0.5f
            binding.textSettingValue.alpha = 0.5f
        }
    }

    override fun onClick(clicked: View) {
        if (setting.isEditable) {
            adapter.onDateTimeClick(setting, bindingAdapterPosition)
        } else {
            adapter.onClickDisabledSetting()
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            return adapter.onLongClick(setting.setting!!, bindingAdapterPosition)
        } else {
            adapter.onClickDisabledSetting()
        }
        return false
    }
}
