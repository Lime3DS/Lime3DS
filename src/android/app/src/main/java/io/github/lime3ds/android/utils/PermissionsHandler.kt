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
    const val CITRA_DIRECTORY = "CITRA_DIRECTORY"
    val preferences: SharedPreferences =
        PreferenceManager.getDefaultSharedPreferences(LimeApplication.appContext)

    fun hasWriteAccess(context: Context): Boolean {
        try {
            if (citraDirectory.toString().isEmpty()) {
                return false
            }

            val uri = citraDirectory
            val takeFlags =
                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            context.contentResolver.takePersistableUriPermission(uri, takeFlags)
            val root = DocumentFile.fromTreeUri(context, uri)
            if (root != null && root.exists()) {
                return true
            }

            context.contentResolver.releasePersistableUriPermission(uri, takeFlags)
        } catch (e: Exception) {
            Log.error("[PermissionsHandler]: Cannot check citra data directory permission, error: " + e.message)
        }
        return false
    }

    val citraDirectory: Uri
        get() {
            val directoryString = preferences.getString(CITRA_DIRECTORY, "")
            return Uri.parse(directoryString)
        }

    fun setCitraDirectory(uriString: String?) =
        preferences.edit().putString(CITRA_DIRECTORY, uriString).apply()
}
