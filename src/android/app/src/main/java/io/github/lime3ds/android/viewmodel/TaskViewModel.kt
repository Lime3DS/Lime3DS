// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class TaskViewModel : ViewModel() {
    val result: StateFlow<Any> get() = _result
    private val _result = MutableStateFlow(Any())

    val isComplete: StateFlow<Boolean> get() = _isComplete
    private val _isComplete = MutableStateFlow(false)

    val isRunning: StateFlow<Boolean> get() = _isRunning
    private val _isRunning = MutableStateFlow(false)

    val cancelled: StateFlow<Boolean> get() = _cancelled
    private val _cancelled = MutableStateFlow(false)

    lateinit var task: () -> Any

    fun clear() {
        _result.value = Any()
        _isComplete.value = false
        _isRunning.value = false
        _cancelled.value = false
    }

    fun setCancelled(value: Boolean) {
        _cancelled.value = value
    }

    fun runTask() {
        if (isRunning.value) {
            return
        }
        _isRunning.value = true

        viewModelScope.launch(Dispatchers.IO) {
            val res = task()
            _result.value = res
            _isComplete.value = true
            _isRunning.value = false
        }
    }
}

enum class TaskState {
    Completed,
    Failed,
    Cancelled
}
