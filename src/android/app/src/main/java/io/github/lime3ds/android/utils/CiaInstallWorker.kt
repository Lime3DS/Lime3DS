// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.app.NotificationManager
import android.content.Context
import android.net.Uri
import android.widget.Toast
import androidx.core.app.NotificationCompat
import androidx.work.ForegroundInfo
import androidx.work.Worker
import androidx.work.WorkerParameters
import io.github.lime3ds.android.NativeLibrary.InstallStatus
import io.github.lime3ds.android.R
import io.github.lime3ds.android.utils.FileUtil.getFilename

class CiaInstallWorker(
    val context: Context,
    params: WorkerParameters
) : Worker(context, params) {
    private val GROUP_KEY_CIA_INSTALL_STATUS = "io.github.lime3ds.android.CIA_INSTALL_STATUS"
    private var lastNotifiedTime: Long = 0
    private val SUMMARY_NOTIFICATION_ID = 0xC1A0000
    private val PROGRESS_NOTIFICATION_ID = SUMMARY_NOTIFICATION_ID + 1
    private var statusNotificationId = SUMMARY_NOTIFICATION_ID + 2

    private val notificationManager = context.getSystemService(NotificationManager::class.java)
    private val installProgressBuilder = NotificationCompat.Builder(
        context,
        context.getString(R.string.cia_install_notification_channel_id)
    )
        .setContentTitle(context.getString(R.string.install_cia_title))
        .setSmallIcon(R.drawable.ic_stat_notification_logo)
    private val installStatusBuilder = NotificationCompat.Builder(
        context,
        context.getString(R.string.cia_install_notification_channel_id)
    )
        .setContentTitle(context.getString(R.string.install_cia_title))
        .setSmallIcon(R.drawable.ic_stat_notification_logo)
        .setGroup(GROUP_KEY_CIA_INSTALL_STATUS)
    private val summaryNotification = NotificationCompat.Builder(
        context,
        context.getString(R.string.cia_install_notification_channel_id)
    )
        .setContentTitle(context.getString(R.string.install_cia_title))
        .setSmallIcon(R.drawable.ic_stat_notification_logo)
        .setGroup(GROUP_KEY_CIA_INSTALL_STATUS)
        .setGroupSummary(true)
        .build()

    private fun notifyInstallStatus(filename: String, status: InstallStatus) {
        when (status) {
            InstallStatus.Success -> {
                installStatusBuilder.setContentTitle(
                    context.getString(R.string.cia_install_notification_success_title)
                )
                installStatusBuilder.setContentText(
                    context.getString(R.string.cia_install_success, filename)
                )
            }

            InstallStatus.ErrorAborted -> {
                installStatusBuilder.setContentTitle(
                    context.getString(R.string.cia_install_notification_error_title)
                )
                installStatusBuilder.setStyle(
                    NotificationCompat.BigTextStyle()
                        .bigText(context.getString(R.string.cia_install_error_aborted, filename))
                )
            }

            InstallStatus.ErrorInvalid -> {
                installStatusBuilder.setContentTitle(
                    context.getString(R.string.cia_install_notification_error_title)
                )
                installStatusBuilder.setContentText(
                    context.getString(R.string.cia_install_error_invalid, filename)
                )
            }

            InstallStatus.ErrorEncrypted -> {
                installStatusBuilder.setContentTitle(
                    context.getString(R.string.cia_install_notification_error_title)
                )
                installStatusBuilder.setStyle(
                    NotificationCompat.BigTextStyle()
                        .bigText(context.getString(R.string.cia_install_error_encrypted, filename))
                )
            }

            InstallStatus.ErrorFailedToOpenFile, InstallStatus.ErrorFileNotFound -> {
                installStatusBuilder.setContentTitle(
                    context.getString(R.string.cia_install_notification_error_title)
                )
                installStatusBuilder.setStyle(
                    NotificationCompat.BigTextStyle()
                        .bigText(context.getString(R.string.cia_install_error_unknown, filename))
                )
            }

            else -> {
                installStatusBuilder.setContentTitle(
                    context.getString(R.string.cia_install_notification_error_title)
                )
                installStatusBuilder.setStyle(
                    NotificationCompat.BigTextStyle()
                        .bigText(context.getString(R.string.cia_install_error_unknown, filename))
                )
            }
        }

        // Even if newer versions of Android don't show the group summary text that you design,
        // you always need to manually set a summary to enable grouped notifications.
        notificationManager.notify(SUMMARY_NOTIFICATION_ID, summaryNotification)
        notificationManager.notify(statusNotificationId++, installStatusBuilder.build())
    }

    override fun doWork(): Result {
        val selectedFiles = inputData.getStringArray("CIA_FILES")!!
        val toastText: CharSequence = context.resources.getQuantityString(
            R.plurals.cia_install_toast,
            selectedFiles.size, selectedFiles.size
        )
        context.mainExecutor.execute {
            Toast.makeText(context, toastText, Toast.LENGTH_LONG).show()
        }

        // Issue the initial notification with zero progress
        installProgressBuilder.setOngoing(true)
        setProgressCallback(100, 0)
        selectedFiles.forEachIndexed { i, file ->
            val filename = getFilename(Uri.parse(file))
            installProgressBuilder.setContentText(
                context.getString(
                    R.string.cia_install_notification_installing,
                    filename,
                    i,
                    selectedFiles.size
                )
            )
            val res = installCIA(file)
            notifyInstallStatus(filename, res)
        }
        notificationManager.cancel(PROGRESS_NOTIFICATION_ID)
        return Result.success()
    }

    fun setProgressCallback(max: Int, progress: Int) {
        val currentTime = System.currentTimeMillis()
        // Android applies a rate limit when updating a notification.
        // If you post updates to a single notification too frequently,
        // such as many in less than one second, the system might drop updates.
        // TODO: consider moving to C++ side
        if (currentTime - lastNotifiedTime < 500 /* ms */) {
            return
        }
        lastNotifiedTime = currentTime
        installProgressBuilder.setProgress(max, progress, false)
        notificationManager.notify(PROGRESS_NOTIFICATION_ID, installProgressBuilder.build())
    }

    override fun getForegroundInfo(): ForegroundInfo =
        ForegroundInfo(PROGRESS_NOTIFICATION_ID, installProgressBuilder.build())

    private external fun installCIA(path: String): InstallStatus
}
