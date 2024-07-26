// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <thread>
#include "common/assert.h"
#include "common/detached_tasks.h"

namespace Common {

DetachedTasks* DetachedTasks::instance = nullptr;

DetachedTasks::DetachedTasks() {
    ASSERT(instance == nullptr);
    instance = this;
}

void DetachedTasks::WaitForAllTasks() {
    std::unique_lock lock{mutex};
    cv.wait(lock, [this]() { return count == 0; });
}

DetachedTasks::~DetachedTasks() {
    std::unique_lock lock{mutex};
    ASSERT(count == 0);
    instance = nullptr;
}

void DetachedTasks::AddTask(std::function<void()> task) {
    std::unique_lock lock{instance->mutex};
    ++instance->count;
    std::thread([task{std::move(task)}]() {
        task();
        std::unique_lock lock{instance->mutex};
        --instance->count;
        std::notify_all_at_thread_exit(instance->cv, std::move(lock));
    }).detach();
}

} // namespace Common
