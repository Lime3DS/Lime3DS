// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.contracts

import android.content.Context
import android.content.Intent
import androidx.activity.result.contract.ActivityResultContract

class OpenFileResultContract : ActivityResultContract<Boolean?, Intent?>() {
    override fun createIntent(context: Context, input: Boolean?): Intent {
        return Intent(Intent.ACTION_OPEN_DOCUMENT)
            .setType("application/octet-stream")
            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, input)
    }

    override fun parseResult(resultCode: Int, intent: Intent?): Intent? = intent
}
