// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.model

import android.os.Parcelable
import android.content.Intent
import android.net.Uri
import java.util.HashSet
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.activities.EmulationActivity

@Parcelize
@Serializable
class Game(
    val title: String = "",
    val description: String = "",
    val path: String = "",
    val titleId: Long = 0L,
    val company: String = "",
    val regions: String = "",
    val isInstalled: Boolean = false,
    val isSystemTitle: Boolean = false,
    val isVisibleSystemTitle: Boolean = false,
    val icon: IntArray? = null,
    val filename: String
) : Parcelable {
    val keyAddedToLibraryTime get() = "${filename}_AddedToLibraryTime"
    val keyLastPlayedTime get() = "${filename}_LastPlayed"

    val launchIntent: Intent
        get() = Intent(LimeApplication.appContext, EmulationActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            data = if (isInstalled) {
                LimeApplication.documentsTree.getUri(path)
            } else {
                Uri.parse(path)
            }
        }

    override fun equals(other: Any?): Boolean {
        if (other !is Game) {
            return false
        }

        return hashCode() == other.hashCode()
    }

    override fun hashCode(): Int {
        var result = title.hashCode()
        result = 31 * result + description.hashCode()
        result = 31 * result + regions.hashCode()
        result = 31 * result + path.hashCode()
        result = 31 * result + titleId.hashCode()
        result = 31 * result + company.hashCode()
        return result
    }

    companion object {
        val allExtensions: Set<String> get() = extensions + badExtensions

        val extensions: Set<String> = HashSet(
            listOf("3ds", "3dsx", "elf", "axf", "cci", "cxi", "app")
        )

        val badExtensions: Set<String> = HashSet(
            listOf("rar", "zip", "7z", "torrent", "tar", "gz")
        )
    }
}
