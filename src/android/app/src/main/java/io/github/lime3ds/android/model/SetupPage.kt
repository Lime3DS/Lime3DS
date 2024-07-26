// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.model

data class SetupPage(
    val iconId: Int,
    val titleId: Int,
    val descriptionId: Int,
    val buttonIconId: Int,
    val leftAlignedIcon: Boolean,
    val buttonTextId: Int,
    val buttonAction: (callback: SetupCallback) -> Unit,
    val isUnskippable: Boolean = false,
    val hasWarning: Boolean = false,
    val stepCompleted: () -> StepState = { StepState.STEP_UNDEFINED },
    val warningTitleId: Int = 0,
    val warningDescriptionId: Int = 0,
    val warningHelpLinkId: Int = 0
)

interface SetupCallback {
    fun onStepCompleted()
}

enum class StepState {
    STEP_COMPLETE,
    STEP_INCOMPLETE,
    STEP_UNDEFINED
}
