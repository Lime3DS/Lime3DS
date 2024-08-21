// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import androidx.preference.PreferenceManager
import androidx.documentfile.provider.DocumentFile
import io.github.lime3ds.android.LimeApplication

object PermissionsHandler {
    const val LIME3DS_DIRECTORY = "LIME3DS_DIRECTORY"
    val preferences: SharedPreferences =
        PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)

    fun hasWriteAccess(context: Context): Boolean {
        try {
            if (lime3dsDirectory.toString().isEmpty()) {
                return false
            }

            val uri = lime3dsDirectory
            val takeFlags =
                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            context.contentResolver.takePersistableUriPermission(uri, takeFlags)
            val root = DocumentFile.fromTreeUri(context, uri)
            if (root != null && root.exists()) {
                return true
            }

            context.contentResolver.releasePersistableUriPermission(uri, takeFlags)
        } catch (e: Exception) {
            Log.error("[PermissionsHandler]: Cannot check lime3ds data directory permission, error: " + e.message)
        }
        return false
    }

    val lime3dsDirectory: Uri
        get() {
            val directoryString = preferences.getString(LIME3DS_DIRECTORY, "")
            return Uri.parse(directoryString)
        }

    fun setLime3DSDirectory(uriString: String?) =
        preferences.edit().putString(LIME3DS_DIRECTORY, uriString).apply()
}
