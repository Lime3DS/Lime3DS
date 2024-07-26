// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.viewmodel

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.yield
import io.github.lime3ds.android.NativeLibrary
import io.github.lime3ds.android.NativeLibrary.InstallStatus
import io.github.lime3ds.android.utils.Log
import java.util.concurrent.atomic.AtomicInteger
import kotlin.coroutines.CoroutineContext
import kotlin.math.min

class SystemFilesViewModel : ViewModel() {
    private var job: Job
    private val coroutineContext: CoroutineContext
        get() = Dispatchers.IO + job

    val isDownloading get() = _isDownloading.asStateFlow()
    private val _isDownloading = MutableStateFlow(false)

    val progress get() = _progress.asStateFlow()
    private val _progress = MutableStateFlow(0)

    val result get() = _result.asStateFlow()
    private val _result = MutableStateFlow<InstallStatus?>(null)

    val shouldRefresh get() = _shouldRefresh.asStateFlow()
    private val _shouldRefresh = MutableStateFlow(false)

    private var cancelled = false

    private val RETRY_AMOUNT = 3

    init {
        job = Job()
        clear()
    }

    fun setShouldRefresh(refresh: Boolean) {
        _shouldRefresh.value = refresh
    }

    fun setProgress(progress: Int) {
        _progress.value = progress
    }

    fun download(titles: LongArray) {
        if (isDownloading.value) {
            return
        }
        clear()
        _isDownloading.value = true
        Log.debug("System menu download started.")

        val minExecutors = min(Runtime.getRuntime().availableProcessors(), titles.size)
        val segment = (titles.size / minExecutors)
        val atomicProgress = AtomicInteger(0)
        for (i in 0 until minExecutors) {
            val titlesSegment = if (i < minExecutors - 1) {
                titles.copyOfRange(i * segment, (i + 1) * segment)
            } else {
                titles.copyOfRange(i * segment, titles.size)
            }

            CoroutineScope(coroutineContext).launch {
                titlesSegment.forEach { title: Long ->
                    // Notify UI of cancellation before ending coroutine
                    if (cancelled) {
                        _result.value = InstallStatus.ErrorAborted
                        cancelled = false
                    }

                    // Takes a moment to see if the coroutine was cancelled
                    yield()

                    // Retry downloading a title repeatedly
                    for (j in 0 until RETRY_AMOUNT) {
                        val result = tryDownloadTitle(title)
                        if (result == InstallStatus.Success) {
                            break
                        } else if (j == RETRY_AMOUNT - 1) {
                            _result.value = result
                            return@launch
                        }
                        Log.warning("Download for title{$title} failed, retrying in 3s...")
                        delay(3000L)
                    }

                    Log.debug("Successfully installed title - $title")
                    setProgress(atomicProgress.incrementAndGet())

                    Log.debug("System File Progress - ${atomicProgress.get()} / ${titles.size}")
                    if (atomicProgress.get() == titles.size) {
                        _result.value = InstallStatus.Success
                        setShouldRefresh(true)
                    }
                }
            }
        }
    }

    private fun tryDownloadTitle(title: Long): InstallStatus {
        val result = NativeLibrary.downloadTitleFromNus(title)
        if (result != InstallStatus.Success) {
            Log.error("Failed to install title $title with error - $result")
        }
        return result
    }

    fun clear() {
        Log.debug("Clearing")
        job.cancelChildren()
        job = Job()
        _progress.value = 0
        _result.value = null
        _isDownloading.value = false
        cancelled = false
    }

    fun cancel() {
        Log.debug("Canceling system file download.")
        cancelled = true
        job.cancelChildren()
        job = Job()
        _progress.value = 0
        _result.value = InstallStatus.Cancelled
    }
}
