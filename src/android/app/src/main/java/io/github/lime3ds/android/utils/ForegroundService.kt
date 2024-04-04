// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import io.github.lime3ds.android.R
import io.github.lime3ds.android.activities.EmulationActivity

/**
 * A service that shows a permanent notification in the background to avoid the app getting
 * cleared from memory by the system.
 */
class ForegroundService : Service() {
    companion object {
        const val EMULATION_RUNNING_NOTIFICATION = 0x1000

        const val ACTION_STOP = "stop"
    }

    private fun showRunningNotification() {
        // Intent is used to resume emulation if the notification is clicked
        val contentIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, EmulationActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val builder =
            NotificationCompat.Builder(this, getString(R.string.app_notification_channel_id))
                .setSmallIcon(R.drawable.ic_stat_notification_logo)
                .setContentTitle(getString(R.string.app_name))
                .setContentText(getString(R.string.app_notification_running))
                .setPriority(NotificationCompat.PRIORITY_LOW)
                .setOngoing(true)
                .setVibrate(null)
                .setSound(null)
                .setContentIntent(contentIntent)
        startForeground(EMULATION_RUNNING_NOTIFICATION, builder.build())
    }

    override fun onBind(intent: Intent): IBinder? {
        return null
    }

    override fun onCreate() {
        showRunningNotification()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent == null) {
            return START_NOT_STICKY
        }
        if (intent.action == ACTION_STOP) {
            NotificationManagerCompat.from(this).cancel(EMULATION_RUNNING_NOTIFICATION)
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelfResult(startId)
        }
        return START_STICKY
    }

    override fun onDestroy() =
        NotificationManagerCompat.from(this).cancel(EMULATION_RUNNING_NOTIFICATION)
}
