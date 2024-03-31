// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.model

import android.os.Parcelable
import androidx.annotation.StringRes
import kotlinx.parcelize.Parcelize

@Parcelize
data class License(
    @StringRes val titleId: Int,
    @StringRes val descriptionId: Int,
    @StringRes val linkId: Int,
    @StringRes val copyrightId: Int = 0,
    @StringRes val licenseId: Int = 0,
    @StringRes val licenseLinkId: Int = 0
) : Parcelable