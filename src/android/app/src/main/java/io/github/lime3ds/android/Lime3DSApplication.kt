// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android

import android.annotation.SuppressLint
import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.content.Context
import android.os.Build
import io.github.lime3ds.android.utils.DirectoryInitialization
import io.github.lime3ds.android.utils.DocumentsTree
import io.github.lime3ds.android.utils.GpuDriverHelper
import io.github.lime3ds.android.utils.PermissionsHandler
import io.github.lime3ds.android.utils.Log
import io.github.lime3ds.android.utils.MemoryUtil

class LimeApplication : Application() {
    private fun createNotificationChannel() {
        with(getSystemService(NotificationManager::class.java)) {
            // General notification
            val name: CharSequence = getString(R.string.app_notification_channel_name)
            val description = getString(R.string.app_notification_channel_description)
            val generalChannel = NotificationChannel(
                getString(R.string.app_notification_channel_id),
                name,
                NotificationManager.IMPORTANCE_LOW
            )
            generalChannel.description = description
            generalChannel.setSound(null, null)
            generalChannel.vibrationPattern = null
            createNotificationChannel(generalChannel)

            // CIA Install notifications
            val ciaChannel = NotificationChannel(
                getString(R.string.cia_install_notification_channel_id),
                getString(R.string.cia_install_notification_channel_name),
                NotificationManager.IMPORTANCE_DEFAULT
            )
            ciaChannel.description =
                getString(R.string.cia_install_notification_channel_description)
            ciaChannel.setSound(null, null)
            ciaChannel.vibrationPattern = null
            createNotificationChannel(ciaChannel)
        }
    }

    override fun onCreate() {
        super.onCreate()
        application = this
        documentsTree = DocumentsTree()
        if (PermissionsHandler.hasWriteAccess(applicationContext)) {
            DirectoryInitialization.start()
        }

        NativeLibrary.logDeviceInfo()
        logDeviceInfo()
        createNotificationChannel()
    }

    fun logDeviceInfo() {
        Log.info("Device Manufacturer - ${Build.MANUFACTURER}")
        Log.info("Device Model - ${Build.MODEL}")
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.R) {
            Log.info("SoC Manufacturer - ${Build.SOC_MANUFACTURER}")
            Log.info("SoC Model - ${Build.SOC_MODEL}")
        }
        Log.info("Total System Memory - ${MemoryUtil.getDeviceRAM()}")
    }

    companion object {
        private var application: LimeApplication? = null

        val appContext: Context get() = application!!.applicationContext

        @SuppressLint("StaticFieldLeak")
        lateinit var documentsTree: DocumentsTree
    }
}
