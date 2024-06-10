// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.ui.viewholder

import android.view.View
import android.widget.CompoundButton
import io.github.lime3ds.android.databinding.ListItemSettingSwitchBinding
import io.github.lime3ds.android.features.settings.model.view.SettingsItem
import io.github.lime3ds.android.features.settings.model.view.SwitchSetting
import io.github.lime3ds.android.features.settings.ui.SettingsAdapter
import io.github.lime3ds.android.utils.GpuDriverHelper
import io.github.lime3ds.android.R

class SwitchSettingViewHolder(val binding: ListItemSettingSwitchBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {

    private lateinit var setting: SwitchSetting
    private lateinit var settingitem: SettingsItem
    
    override fun bind(item: SettingsItem) {
        setting = item as SwitchSetting
        settingitem = item
        binding.textSettingName.setText(item.nameId)
        if (item.descriptionId != 0) {
            binding.textSettingDescription.setText(item.descriptionId)
            binding.textSettingDescription.visibility = View.VISIBLE
        } else {
            binding.textSettingDescription.text = ""
            binding.textSettingDescription.visibility = View.GONE
        }

        binding.switchWidget.setOnCheckedChangeListener(null)
        binding.switchWidget.isChecked = setting.isChecked
        binding.switchWidget.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
            adapter.onBooleanClick(item, bindingAdapterPosition, binding.switchWidget.isChecked)
        }

        binding.switchWidget.isEnabled = setting.isEditable && isForceMaxGpuClockSpeedClickable()

        if (setting.isEditable) {
            val alphaValue = if (isForceMaxGpuClockSpeedClickable()) 1f else 0.5f
            binding.textSettingName.alpha = alphaValue
            binding.textSettingDescription.alpha = alphaValue
        } else {
            binding.textSettingName.alpha = 0.5f
            binding.textSettingDescription.alpha = 0.5f
        }
    }

    override fun onClick(clicked: View) {
        if (setting.isEditable) {
            if (isForceMaxGpuClockSpeedClickable()) { 
                binding.switchWidget.toggle()
            } else {
                adapter.onForceMaximumGpuClockSpeedDisabled()
            }
        } else {
            adapter.onClickDisabledSetting()
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            if (isForceMaxGpuClockSpeedClickable()) {
                return adapter.onLongClick(setting.setting!!, bindingAdapterPosition)
            }
            adapter.onForceMaximumGpuClockSpeedDisabled()
            return false
        } else {
            adapter.onClickDisabledSetting()
        }
        return false
    }

    private fun isForceMaxGpuClockSpeedClickable(): Boolean {
        return if (settingitem.nameId == R.string.force_max_gpu_clock_speed) {
            GpuDriverHelper.supportsCustomDriverLoading()
        } else {
            true
        }
    }
        
}
