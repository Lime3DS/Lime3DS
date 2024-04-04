// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.ui.viewholder

import android.view.View
import io.github.lime3ds.android.databinding.ListItemSettingBinding
import io.github.lime3ds.android.features.settings.model.view.SettingsItem
import io.github.lime3ds.android.features.settings.model.view.StringInputSetting
import io.github.lime3ds.android.features.settings.ui.SettingsAdapter

class StringInputViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SettingsItem

    override fun bind(item: SettingsItem) {
        setting = item
        binding.textSettingName.setText(item.nameId)
        if (item.descriptionId != 0) {
            binding.textSettingDescription.visibility = View.VISIBLE
            binding.textSettingDescription.setText(item.descriptionId)
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
        binding.textSettingValue.visibility = View.VISIBLE
        binding.textSettingValue.text = setting.setting?.valueAsString
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            adapter.onClickDisabledSetting()
            return
        }
        adapter.onStringInputClick((setting as StringInputSetting), bindingAdapterPosition)
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
