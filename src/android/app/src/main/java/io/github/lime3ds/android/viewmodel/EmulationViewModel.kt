// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.viewmodel

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class EmulationViewModel : ViewModel() {
    val emulationStarted get() = _emulationStarted.asStateFlow()
    private val _emulationStarted = MutableStateFlow(false)

    val shaderProgress get() = _shaderProgress.asStateFlow()
    private val _shaderProgress = MutableStateFlow(0)

    val totalShaders get() = _totalShaders.asStateFlow()
    private val _totalShaders = MutableStateFlow(0)

    val shaderMessage get() = _shaderMessage.asStateFlow()
    private val _shaderMessage = MutableStateFlow("")

    fun setShaderProgress(progress: Int) {
        _shaderProgress.value = progress
    }

    fun setTotalShaders(max: Int) {
        _totalShaders.value = max
    }

    fun setShaderMessage(msg: String) {
        _shaderMessage.value = msg
    }

    fun updateProgress(msg: String, progress: Int, max: Int) {
        setShaderMessage(msg)
        setShaderProgress(progress)
        setTotalShaders(max)
    }

    fun setEmulationStarted(started: Boolean) {
        _emulationStarted.value = started
    }
}
