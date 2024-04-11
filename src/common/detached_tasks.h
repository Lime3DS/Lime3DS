// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <functional>

namespace Common {

/**
 * A background manager which ensures that all detached task is finished before program exits.
 *
 * Some tasks, telemetry submission for example, prefer executing asynchronously and don't care
 * about the result. These tasks are suitable for std::thread::detach(). However, this is unsafe if
 * the task is launched just before the program exits (which is a common case for telemetry), so we
 * need to block on these tasks on program exit.
 *
 * To make detached task safe, a single DetachedTasks object should be placed in the main(), and
 * call WaitForAllTasks() after all program execution but before global/static variable destruction.
 * Any potentially unsafe detached task should be executed via DetachedTasks::AddTask.
 */
class DetachedTasks {
public:
    DetachedTasks();
    ~DetachedTasks();
    void WaitForAllTasks();

    static void AddTask(std::function<void()> task);

private:
    static DetachedTasks* instance;

    std::condition_variable cv;
    std::mutex mutex;
    int count = 0;
};

} // namespace Common
