// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.settings.ui.viewholder

import android.view.View
import io.github.lime3ds.android.databinding.ListItemSettingsHeaderBinding
import io.github.lime3ds.android.features.settings.model.view.SettingsItem
import io.github.lime3ds.android.features.settings.ui.SettingsAdapter

class HeaderViewHolder(val binding: ListItemSettingsHeaderBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {

    init {
        itemView.setOnClickListener(null)
    }

    override fun bind(item: SettingsItem) {
        binding.textHeaderName.setText(item.nameId)
        if (item.descriptionId != 0) {
            binding.textHeaderDescription.visibility = View.VISIBLE
            binding.textHeaderDescription.setText(item.descriptionId)
        }else {
            binding.textHeaderDescription.visibility = View.GONE
        }
    }

    override fun onClick(clicked: View) {
        // no-op
    }

    override fun onLongClick(clicked: View): Boolean {
        // no-op
        return true
    }
}
