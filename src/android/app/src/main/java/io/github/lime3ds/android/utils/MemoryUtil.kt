// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package io.github.lime3ds.android.utils

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import io.github.lime3ds.android.LimeApplication
import io.github.lime3ds.android.R
import java.util.Locale
import kotlin.math.ceil

object MemoryUtil {
    private val context get() = LimeApplication.appContext

    private val Float.hundredths: String
        get() = String.format(Locale.ROOT, "%.2f", this)

    const val Kb: Float = 1024F
    const val Mb = Kb * 1024
    const val Gb = Mb * 1024
    const val Tb = Gb * 1024
    const val Pb = Tb * 1024
    const val Eb = Pb * 1024

    fun bytesToSizeUnit(size: Float, roundUp: Boolean = false): String =
        when {
            size < Kb -> {
                context.getString(
                    R.string.memory_formatted,
                    size.hundredths,
                    context.getString(R.string.memory_byte_shorthand)
                )
            }
            size < Mb -> {
                context.getString(
                    R.string.memory_formatted,
                    if (roundUp) ceil(size / Kb) else (size / Kb).hundredths,
                    context.getString(R.string.memory_kilobyte)
                )
            }
            size < Gb -> {
                context.getString(
                    R.string.memory_formatted,
                    if (roundUp) ceil(size / Mb) else (size / Mb).hundredths,
                    context.getString(R.string.memory_megabyte)
                )
            }
            size < Tb -> {
                context.getString(
                    R.string.memory_formatted,
                    if (roundUp) ceil(size / Gb) else (size / Gb).hundredths,
                    context.getString(R.string.memory_gigabyte)
                )
            }
            size < Pb -> {
                context.getString(
                    R.string.memory_formatted,
                    if (roundUp) ceil(size / Tb) else (size / Tb).hundredths,
                    context.getString(R.string.memory_terabyte)
                )
            }
            size < Eb -> {
                context.getString(
                    R.string.memory_formatted,
                    if (roundUp) ceil(size / Pb) else (size / Pb).hundredths,
                    context.getString(R.string.memory_petabyte)
                )
            }
            else -> {
                context.getString(
                    R.string.memory_formatted,
                    if (roundUp) ceil(size / Eb) else (size / Eb).hundredths,
                    context.getString(R.string.memory_exabyte)
                )
            }
        }

    val totalMemory: Float
        get() {
            val memInfo = ActivityManager.MemoryInfo()
            with(context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager) {
                getMemoryInfo(memInfo)
            }

            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                memInfo.advertisedMem.toFloat()
            } else {
                memInfo.totalMem.toFloat()
            }
        }

    fun isLessThan(minimum: Int, size: Float): Boolean =
        when (size) {
            Kb -> totalMemory < Mb && totalMemory < minimum
            Mb -> totalMemory < Gb && (totalMemory / Mb) < minimum
            Gb -> totalMemory < Tb && (totalMemory / Gb) < minimum
            Tb -> totalMemory < Pb && (totalMemory / Tb) < minimum
            Pb -> totalMemory < Eb && (totalMemory / Pb) < minimum
            Eb -> totalMemory / Eb < minimum
            else -> totalMemory < Kb && totalMemory < minimum
        }

    // Devices are unlikely to have 0.5GB increments of memory so we'll just round up to account for
    // the potential error created by memInfo.totalMem
    fun getDeviceRAM(): String = bytesToSizeUnit(totalMemory, true)
}
