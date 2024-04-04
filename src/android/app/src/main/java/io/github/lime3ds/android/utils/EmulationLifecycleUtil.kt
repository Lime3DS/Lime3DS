// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package io.github.lime3ds.android.utils

object EmulationLifecycleUtil {
    private var shutdownHooks: MutableList<Runnable> = ArrayList()
    private var pauseResumeHooks: MutableList<Runnable> = ArrayList()


    fun closeGame() {
        shutdownHooks.forEach(Runnable::run)
    }

    fun pauseOrResume() {
        pauseResumeHooks.forEach(Runnable::run)
    }

    fun addShutdownHook(hook: Runnable) {
        shutdownHooks.add(hook)
    }

    fun addPauseResumeHook(hook: Runnable) {
        pauseResumeHooks.add(hook)
    }

    fun clear() {
        pauseResumeHooks.clear()
        shutdownHooks.clear()
    }
}
