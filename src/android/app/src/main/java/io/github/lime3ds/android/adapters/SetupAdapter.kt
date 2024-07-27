// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.adapters

import android.text.Html
import android.text.method.LinkMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.res.ResourcesCompat
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import io.github.lime3ds.android.databinding.PageSetupBinding
import io.github.lime3ds.android.model.SetupCallback
import io.github.lime3ds.android.model.SetupPage
import io.github.lime3ds.android.model.StepState
import io.github.lime3ds.android.utils.ViewUtils

class SetupAdapter(val activity: AppCompatActivity, val pages: List<SetupPage>) :
    RecyclerView.Adapter<SetupAdapter.SetupPageViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SetupPageViewHolder {
        val binding = PageSetupBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return SetupPageViewHolder(binding)
    }

    override fun getItemCount(): Int = pages.size

    override fun onBindViewHolder(holder: SetupPageViewHolder, position: Int) =
        holder.bind(pages[position])

    inner class SetupPageViewHolder(val binding: PageSetupBinding) :
        RecyclerView.ViewHolder(binding.root), SetupCallback {
        lateinit var page: SetupPage

        init {
            itemView.tag = this
        }

        fun bind(page: SetupPage) {
            this.page = page

            if (page.stepCompleted.invoke() == StepState.STEP_COMPLETE) {
                onStepCompleted()
            }

            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    activity.resources,
                    page.iconId,
                    activity.theme
                )
            )
            binding.textTitle.text = activity.resources.getString(page.titleId)
            binding.textDescription.text =
                Html.fromHtml(activity.resources.getString(page.descriptionId), 0)
            binding.textDescription.movementMethod = LinkMovementMethod.getInstance()

            binding.buttonAction.apply {
                text = activity.resources.getString(page.buttonTextId)
                if (page.buttonIconId != 0) {
                    icon = ResourcesCompat.getDrawable(
                        activity.resources,
                        page.buttonIconId,
                        activity.theme
                    )
                }
                iconGravity =
                    if (page.leftAlignedIcon) {
                        MaterialButton.ICON_GRAVITY_START
                    } else {
                        MaterialButton.ICON_GRAVITY_END
                    }
                setOnClickListener {
                    page.buttonAction.invoke(this@SetupPageViewHolder)
                }
            }
        }

        override fun onStepCompleted() {
            ViewUtils.hideView(binding.buttonAction, 200)
            ViewUtils.showView(binding.textConfirmation, 200)
        }
    }
}
