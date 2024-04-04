// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.ui.viewholder

import android.view.View
import io.github.lime3ds.android.databinding.ListItemSettingBinding
import io.github.lime3ds.android.features.settings.model.AbstractFloatSetting
import io.github.lime3ds.android.features.settings.model.AbstractIntSetting
import io.github.lime3ds.android.features.settings.model.FloatSetting
import io.github.lime3ds.android.features.settings.model.ScaledFloatSetting
import io.github.lime3ds.android.features.settings.model.view.SettingsItem
import io.github.lime3ds.android.features.settings.model.view.SliderSetting
import io.github.lime3ds.android.features.settings.ui.SettingsAdapter

class SliderViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SliderSetting

    override fun bind(item: SettingsItem) {
        setting = item as SliderSetting
        binding.textSettingName.setText(item.nameId)
        if (item.descriptionId != 0) {
            binding.textSettingDescription.visibility = View.VISIBLE
            binding.textSettingDescription.setText(item.descriptionId)
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
        binding.textSettingValue.visibility = View.VISIBLE
        binding.textSettingValue.text = when (setting.setting) {
            is ScaledFloatSetting ->
                "${(setting.setting as ScaledFloatSetting).float.toInt()}${setting.units}"
            is FloatSetting -> "${(setting.setting as AbstractFloatSetting).float}${setting.units}"
            else -> "${(setting.setting as AbstractIntSetting).int}${setting.units}"
        }

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
            adapter.onSliderClick(setting, bindingAdapterPosition)
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
