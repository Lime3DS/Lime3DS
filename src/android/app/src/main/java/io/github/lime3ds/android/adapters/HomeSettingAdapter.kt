// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.adapters

import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.launch
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.CardHomeOptionBinding
import io.github.lime3ds.android.fragments.MessageDialogFragment
import io.github.lime3ds.android.model.HomeSetting
import io.github.lime3ds.android.viewmodel.GamesViewModel

class HomeSettingAdapter(
    private val activity: AppCompatActivity,
    private val viewLifecycle: LifecycleOwner,
    var options: List<HomeSetting>
) : RecyclerView.Adapter<HomeSettingAdapter.HomeOptionViewHolder>(), View.OnClickListener {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HomeOptionViewHolder {
        val binding =
            CardHomeOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        binding.root.setOnClickListener(this)
        return HomeOptionViewHolder(binding)
    }

    override fun getItemCount(): Int {
        return options.size
    }

    override fun onBindViewHolder(holder: HomeOptionViewHolder, position: Int) {
        holder.bind(options[position])
    }

    override fun onClick(view: View) {
        val holder = view.tag as HomeOptionViewHolder
        if (holder.option.isEnabled.invoke()) {
            holder.option.onClick.invoke()
        } else {
            MessageDialogFragment.newInstance(
                holder.option.disabledTitleId,
                holder.option.disabledMessageId
            ).show(activity.supportFragmentManager, MessageDialogFragment.TAG)
        }
    }

    inner class HomeOptionViewHolder(val binding: CardHomeOptionBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var option: HomeSetting

        init {
            itemView.tag = this
        }

        fun bind(option: HomeSetting) {
            this.option = option

            binding.optionTitle.text = activity.resources.getString(option.titleId)
            binding.optionDescription.text = activity.resources.getString(option.descriptionId)
            binding.optionIcon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    activity.resources,
                    option.iconId,
                    activity.theme
                )
            )

            viewLifecycle.lifecycleScope.launch {
                viewLifecycle.repeatOnLifecycle(Lifecycle.State.CREATED) {
                    option.details.collect { updateOptionDetails(it) }
                }
            }
            binding.optionDetail.postDelayed(
                {
                    binding.optionDetail.ellipsize = TextUtils.TruncateAt.MARQUEE
                    binding.optionDetail.isSelected = true
                },
                3000
            )

            if (option.isEnabled.invoke()) {
                binding.optionTitle.alpha = 1f
                binding.optionDescription.alpha = 1f
                binding.optionIcon.alpha = 1f
            } else {
                binding.optionTitle.alpha = 0.5f
                binding.optionDescription.alpha = 0.5f
                binding.optionIcon.alpha = 0.5f
            }
        }

        private fun updateOptionDetails(detailString: String) {
            if (detailString != "") {
                binding.optionDetail.text = detailString
                binding.optionDetail.visibility = View.VISIBLE
            }
        }
    }
}
