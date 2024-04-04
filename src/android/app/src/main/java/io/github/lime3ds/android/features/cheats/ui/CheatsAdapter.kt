// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.cheats.ui

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.CompoundButton
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.ViewModelProvider
import androidx.recyclerview.widget.RecyclerView
import io.github.lime3ds.android.databinding.ListItemCheatBinding
import io.github.lime3ds.android.features.cheats.model.Cheat
import io.github.lime3ds.android.features.cheats.model.CheatsViewModel

class CheatsAdapter(
    private val activity: FragmentActivity,
    private val viewModel: CheatsViewModel
) : RecyclerView.Adapter<CheatsAdapter.CheatViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CheatViewHolder {
        val binding =
            ListItemCheatBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        addViewListeners(binding.root)
        return CheatViewHolder(binding)
    }

    override fun onBindViewHolder(holder: CheatViewHolder, position: Int) =
        holder.bind(activity, viewModel.cheats[position], position)

    override fun getItemCount(): Int = viewModel.cheats.size

    private fun addViewListeners(view: View) {
        // On a portrait phone screen (or other narrow screen), only one of the two panes are shown
        // at the same time. If the user is navigating using a d-pad and moves focus to an element
        // in the currently hidden pane, we need to manually show that pane.
        CheatsActivity.setOnFocusChangeListenerRecursively(view) { _, hasFocus ->
            viewModel.onListViewFocusChanged(hasFocus)
        }
    }

    inner class CheatViewHolder(private val binding: ListItemCheatBinding) :
        RecyclerView.ViewHolder(binding.root), View.OnClickListener,
        CompoundButton.OnCheckedChangeListener {
        private lateinit var viewModel: CheatsViewModel
        private lateinit var cheat: Cheat
        private var position = 0

        fun bind(activity: FragmentActivity, cheat: Cheat, position: Int) {
            viewModel = ViewModelProvider(activity)[CheatsViewModel::class.java]
            this.cheat = cheat
            this.position = position
            binding.textName.text = this.cheat.getName()
            binding.cheatSwitch.isChecked = this.cheat.getEnabled()
            binding.cheatContainer.setOnClickListener(this)
            binding.cheatSwitch.setOnCheckedChangeListener(this)
        }

        override fun onClick(root: View) {
            viewModel.setSelectedCheat(cheat, position)
            viewModel.openDetailsView()
        }

        override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
            cheat.setEnabled(isChecked)
        }
    }
}
