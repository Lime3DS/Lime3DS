// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.viewmodel

import android.content.res.Resources
import android.net.Uri
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.preference.PreferenceManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.R
import io.github.lime3ds.android.fragments.Lime3DSDirectoryDialogFragment
import io.github.lime3ds.android.utils.GameHelper
import io.github.lime3ds.android.utils.PermissionsHandler

class HomeViewModel : ViewModel() {
    val navigationVisible get() = _navigationVisible.asStateFlow()
    private val _navigationVisible = MutableStateFlow(Pair(false, false))

    val statusBarShadeVisible get() = _statusBarShadeVisible.asStateFlow()
    private val _statusBarShadeVisible = MutableStateFlow(true)

    val isPickingUserDir get() = _isPickingUserDir.asStateFlow()
    private val _isPickingUserDir = MutableStateFlow(false)

    val userDir get() = _userDir.asStateFlow()
    private val _userDir = MutableStateFlow(
        Uri.parse(
            PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)
                .getString(PermissionsHandler.LIME3DS_DIRECTORY, "")
        ).path ?: ""
    )

    val gamesDir get() = _gamesDir.asStateFlow()
    private val _gamesDir = MutableStateFlow(
        Uri.parse(
            PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)
                .getString(GameHelper.KEY_GAME_PATH, "")
        ).path ?: ""
    )

    var directoryListener: Lime3DSDirectoryDialogFragment.Listener? = null

    val dirProgress get() = _dirProgress.asStateFlow()
    private val _dirProgress = MutableStateFlow(0)

    val maxDirProgress get() = _maxDirProgress.asStateFlow()
    private val _maxDirProgress = MutableStateFlow(0)

    val messageText get() = _messageText.asStateFlow()
    private val _messageText = MutableStateFlow("")

    val copyComplete get() = _copyComplete.asStateFlow()
    private val _copyComplete = MutableStateFlow(false)

    var copyInProgress = false

    var navigatedToSetup = false

    fun setNavigationVisibility(visible: Boolean, animated: Boolean) {
        if (_navigationVisible.value.first == visible) {
            return
        }
        _navigationVisible.value = Pair(visible, animated)
    }

    fun setStatusBarShadeVisibility(visible: Boolean) {
        if (_statusBarShadeVisible.value == visible) {
            return
        }
        _statusBarShadeVisible.value = visible
    }

    fun setPickingUserDir(picking: Boolean) {
        _isPickingUserDir.value = picking
    }

    fun setUserDir(activity: FragmentActivity, dir: String) {
        ViewModelProvider(activity)[GamesViewModel::class.java].reloadGames(true)
        _userDir.value = dir
    }

    fun setGamesDir(activity: FragmentActivity, dir: String) {
        ViewModelProvider(activity)[GamesViewModel::class.java].reloadGames(true)
        _gamesDir.value = dir
    }

    fun clearCopyInfo() {
        _messageText.value = ""
        _dirProgress.value = 0
        _maxDirProgress.value = 0
        _copyComplete.value = false
        copyInProgress = false
    }

    fun onUpdateSearchProgress(resources: Resources, directoryName: String) {
        _messageText.value = resources.getString(R.string.searching_directory, directoryName)
    }

    fun onUpdateCopyProgress(resources: Resources, filename: String, progress: Int, max: Int) {
        _messageText.value = resources.getString(R.string.copy_file_name, filename)
        _dirProgress.value = progress
        _maxDirProgress.value = max
    }

    fun setCopyComplete(complete: Boolean) {
        _copyComplete.value = complete
    }
}
