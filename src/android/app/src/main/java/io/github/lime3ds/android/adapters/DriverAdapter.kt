// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.adapters

import android.net.Uri
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import io.github.lime3ds.android.R
import io.github.lime3ds.android.databinding.CardDriverOptionBinding
import io.github.lime3ds.android.utils.GpuDriverMetadata
import io.github.lime3ds.android.viewmodel.DriverViewModel
import io.github.lime3ds.android.utils.GpuDriverHelper

class DriverAdapter(private val driverViewModel: DriverViewModel) :
    ListAdapter<Pair<Uri, GpuDriverMetadata>, DriverAdapter.DriverViewHolder>(
        AsyncDifferConfig.Builder(DiffCallback()).build()
    ) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DriverViewHolder {
        val binding =
            CardDriverOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return DriverViewHolder(binding)
    }

    override fun getItemCount(): Int = currentList.size

    override fun onBindViewHolder(holder: DriverViewHolder, position: Int) =
        holder.bind(currentList[position])

    private fun onSelectDriver(position: Int) {
        driverViewModel.setSelectedDriverIndex(position)
        notifyItemChanged(driverViewModel.previouslySelectedDriver)
        notifyItemChanged(driverViewModel.selectedDriver)
    }

    private fun onDeleteDriver(driverData: Pair<Uri, GpuDriverMetadata>, position: Int) {
        if (driverViewModel.selectedDriver > position) {
            driverViewModel.setSelectedDriverIndex(driverViewModel.selectedDriver - 1)
        }
        if (GpuDriverHelper.customDriverData == driverData.second) {
            driverViewModel.setSelectedDriverIndex(0)
        }
        driverViewModel.driversToDelete.add(driverData.first)
        driverViewModel.removeDriver(driverData)
        notifyItemRemoved(position)
        notifyItemChanged(driverViewModel.selectedDriver)
    }

    inner class DriverViewHolder(val binding: CardDriverOptionBinding) :
        RecyclerView.ViewHolder(binding.root) {
        private lateinit var driverData: Pair<Uri, GpuDriverMetadata>

        fun bind(driverData: Pair<Uri, GpuDriverMetadata>) {
            this.driverData = driverData
            val driver = driverData.second

            binding.apply {
                radioButton.isChecked = driverViewModel.selectedDriver == bindingAdapterPosition
                root.setOnClickListener {
                    onSelectDriver(bindingAdapterPosition)
                }
                buttonDelete.setOnClickListener {
                    onDeleteDriver(driverData, bindingAdapterPosition)
                }

                // Delay marquee by 3s
                title.postDelayed(
                    {
                        title.isSelected = true
                        title.ellipsize = TextUtils.TruncateAt.MARQUEE
                        version.isSelected = true
                        version.ellipsize = TextUtils.TruncateAt.MARQUEE
                        description.isSelected = true
                        description.ellipsize = TextUtils.TruncateAt.MARQUEE
                    },
                    3000
                )
                if (driver.name == null) {
                    title.setText(R.string.system_gpu_driver)
                    description.text = ""
                    version.text = ""
                    version.visibility = View.GONE
                    description.visibility = View.GONE
                    buttonDelete.visibility = View.GONE
                } else {
                    title.text = driver.name
                    version.text = driver.version
                    description.text = driver.description
                    version.visibility = View.VISIBLE
                    description.visibility = View.VISIBLE
                    buttonDelete.visibility = View.VISIBLE
                }
            }
        }
    }

    private class DiffCallback : DiffUtil.ItemCallback<Pair<Uri, GpuDriverMetadata>>() {
        override fun areItemsTheSame(
            oldItem: Pair<Uri, GpuDriverMetadata>,
            newItem: Pair<Uri, GpuDriverMetadata>
        ): Boolean {
            return oldItem.first == newItem.first
        }

        override fun areContentsTheSame(
            oldItem: Pair<Uri, GpuDriverMetadata>,
            newItem: Pair<Uri, GpuDriverMetadata>
        ): Boolean {
            return oldItem.second == newItem.second
        }
    }
}
