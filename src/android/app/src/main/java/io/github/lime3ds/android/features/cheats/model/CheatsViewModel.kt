// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.features.cheats.model

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

class CheatsViewModel : ViewModel() {
    val selectedCheat get() = _selectedCheat.asStateFlow()
    private val _selectedCheat = MutableStateFlow<Cheat?>(null)

    val isAdding get() = _isAdding.asStateFlow()
    private val _isAdding = MutableStateFlow(false)

    val isEditing get() = _isEditing.asStateFlow()
    private val _isEditing = MutableStateFlow(false)

    /**
     * When a cheat is added, the integer stored in the returned StateFlow
     * changes to the position of that cheat, then changes back to null.
     */
    val cheatAddedEvent get() = _cheatAddedEvent.asStateFlow()
    private val _cheatAddedEvent = MutableStateFlow<Int?>(null)

    val cheatChangedEvent get() = _cheatChangedEvent.asStateFlow()
    private val _cheatChangedEvent = MutableStateFlow<Int?>(null)

    /**
     * When a cheat is deleted, the integer stored in the returned StateFlow
     * changes to the position of that cheat, then changes back to null.
     */
    val cheatDeletedEvent get() = _cheatDeletedEvent.asStateFlow()
    private val _cheatDeletedEvent = MutableStateFlow<Int?>(null)

    val openDetailsViewEvent get() = _openDetailsViewEvent.asStateFlow()
    private val _openDetailsViewEvent = MutableStateFlow(false)

    val closeDetailsViewEvent get() = _closeDetailsViewEvent.asStateFlow()
    private val _closeDetailsViewEvent = MutableStateFlow(false)

    val listViewFocusChange get() = _listViewFocusChange.asStateFlow()
    private val _listViewFocusChange = MutableStateFlow(false)

    val detailsViewFocusChange get() = _detailsViewFocusChange.asStateFlow()
    private val _detailsViewFocusChange = MutableStateFlow(false)

    private var titleId: Long = 0
    lateinit var cheats: Array<Cheat>
    private var cheatsNeedSaving = false
    private var selectedCheatPosition = -1

    fun initialize(titleId_: Long) {
        titleId = titleId_;
        load()
    }

    private fun load() {
        CheatEngine.loadCheatFile(titleId)
        cheats = CheatEngine.getCheats()
        for (i in cheats.indices) {
            cheats[i].setEnabledChangedCallback {
                cheatsNeedSaving = true
                notifyCheatUpdated(i)
            }
        }
    }

    fun saveIfNeeded() {
        if (cheatsNeedSaving) {
            CheatEngine.saveCheatFile(titleId)
            cheatsNeedSaving = false
        }
    }

    fun setSelectedCheat(cheat: Cheat?, position: Int) {
        if (isEditing.value) {
            setIsEditing(false)
        }
        _selectedCheat.value = cheat
        selectedCheatPosition = position
    }

    fun setIsEditing(value: Boolean) {
        _isEditing.value = value
        if (isAdding.value && !value) {
            _isAdding.value = false
            setSelectedCheat(null, -1)
        }
    }

    private fun notifyCheatAdded(position: Int) {
        _cheatAddedEvent.value = position
        _cheatAddedEvent.value = null
    }

    fun startAddingCheat() {
        _selectedCheat.value = null
        selectedCheatPosition = -1
        _isAdding.value = true
        _isEditing.value = true
    }

    fun finishAddingCheat(cheat: Cheat?) {
        check(isAdding.value)
        _isAdding.value = false
        _isEditing.value = false
        val position = cheats.size
        CheatEngine.addCheat(cheat)
        cheatsNeedSaving = true
        load()
        notifyCheatAdded(position)
        setSelectedCheat(cheats[position], position)
    }

    /**
     * Notifies that an edit has been made to the contents of the cheat at the given position.
     */
    private fun notifyCheatUpdated(position: Int) {
        _cheatChangedEvent.value = position
        _cheatChangedEvent.value = null
    }

    fun updateSelectedCheat(newCheat: Cheat?) {
        CheatEngine.updateCheat(selectedCheatPosition, newCheat)
        cheatsNeedSaving = true
        load()
        notifyCheatUpdated(selectedCheatPosition)
        setSelectedCheat(cheats[selectedCheatPosition], selectedCheatPosition)
    }

    /**
     * Notifies that the cheat at the given position has been deleted.
     */
    private fun notifyCheatDeleted(position: Int) {
        _cheatDeletedEvent.value = position
        _cheatDeletedEvent.value = null
    }

    fun deleteSelectedCheat() {
        val position = selectedCheatPosition
        setSelectedCheat(null, -1)
        CheatEngine.removeCheat(position)
        cheatsNeedSaving = true
        load()
        notifyCheatDeleted(position)
    }

    fun openDetailsView() {
        _openDetailsViewEvent.value = true
        _openDetailsViewEvent.value = false
    }

    fun closeDetailsView() {
        _closeDetailsViewEvent.value = true
        _closeDetailsViewEvent.value = false
    }

    fun onListViewFocusChanged(changed: Boolean) {
        _listViewFocusChange.value = changed
        _listViewFocusChange.value = false
    }

    fun onDetailsViewFocusChanged(changed: Boolean) {
        _detailsViewFocusChange.value = changed
        _detailsViewFocusChange.value = false
    }
}
