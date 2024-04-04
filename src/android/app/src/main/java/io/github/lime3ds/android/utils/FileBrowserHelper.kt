// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.content.Context
import android.content.Intent
import androidx.documentfile.provider.DocumentFile

object FileBrowserHelper {
    fun getSelectedFiles(
        result: Intent,
        context: Context,
        extension: List<String?>
    ): Array<String>? {
        val clipData = result.clipData
        val files: MutableList<DocumentFile?> = ArrayList()
        if (clipData == null) {
            files.add(DocumentFile.fromSingleUri(context, result.data!!))
        } else {
            for (i in 0 until clipData.itemCount) {
                val item = clipData.getItemAt(i)
                files.add(DocumentFile.fromSingleUri(context, item.uri))
            }
        }
        if (files.isNotEmpty()) {
            val filePaths: MutableList<String> = ArrayList()
            for (i in files.indices) {
                val file = files[i]
                val filename = file?.name
                val extensionStart = filename?.lastIndexOf('.') ?: 0
                if (extensionStart > 0) {
                    val fileExtension = filename?.substring(extensionStart + 1)
                    if (extension.contains(fileExtension)) {
                        filePaths.add(file?.uri.toString())
                    }
                }
            }
            return if (filePaths.isEmpty()) null else filePaths.toTypedArray<String>()
        }
        return null
    }
}
