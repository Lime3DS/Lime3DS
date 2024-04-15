// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

SERIALIZE_EXPORT_IMPL(Kernel::Timer)

namespace Kernel {

Timer::Timer(KernelSystem& kernel)
    : WaitObject(kernel), kernel(kernel), timer_manager(kernel.GetTimerManager()) {}

Timer::~Timer() {
    Cancel();
    timer_manager.timer_callback_table.erase(callback_id);
    if (resource_limit) {
        resource_limit->Release(ResourceLimitType::Timer, 1);
    }
}

std::shared_ptr<Timer> KernelSystem::CreateTimer(ResetType reset_type, std::string name) {
    auto timer = std::make_shared<Timer>(*this);
    timer->reset_type = reset_type;
    timer->signaled = false;
    timer->name = std::move(name);
    timer->initial_delay = 0;
    timer->interval_delay = 0;
    timer->callback_id = ++timer_manager->next_timer_callback_id;
    timer_manager->timer_callback_table[timer->callback_id] = timer.get();
    return timer;
}

bool Timer::ShouldWait(const Thread* thread) const {
    return !signaled;
}

void Timer::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    if (reset_type == ResetType::OneShot)
        signaled = false;
}

void Timer::Set(s64 initial, s64 interval) {
    // Ensure we get rid of any previous scheduled event
    Cancel();

    initial_delay = initial;
    interval_delay = interval;

    if (initial == 0) {
        // Immediately invoke the callback
        Signal(0);
    } else {
        kernel.timing.ScheduleEvent(nsToCycles(initial), timer_manager.timer_callback_event_type,
                                    callback_id);
    }
}

void Timer::Cancel() {
    kernel.timing.UnscheduleEvent(timer_manager.timer_callback_event_type, callback_id);
}

void Timer::Clear() {
    signaled = false;
}

void Timer::WakeupAllWaitingThreads() {
    WaitObject::WakeupAllWaitingThreads();

    if (reset_type == ResetType::Pulse)
        signaled = false;
}

void Timer::Signal(s64 cycles_late) {
    LOG_TRACE(Kernel, "Timer {} fired", GetObjectId());

    signaled = true;

    // Resume all waiting threads
    WakeupAllWaitingThreads();

    if (interval_delay != 0) {
        // Reschedule the timer with the interval delay
        kernel.timing.ScheduleEvent(nsToCycles(interval_delay) - cycles_late,
                                    timer_manager.timer_callback_event_type, callback_id);
    }
}

template <class Archive>
void Timer::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<WaitObject>(*this);
    ar & reset_type;
    ar & initial_delay;
    ar & interval_delay;
    ar & signaled;
    ar & name;
    ar & callback_id;
    ar & resource_limit;
}
SERIALIZE_IMPL(Timer)

/// The timer callback event, called when a timer is fired
void TimerManager::TimerCallback(u64 callback_id, s64 cycles_late) {
    std::shared_ptr<Timer> timer = SharedFrom(timer_callback_table.at(callback_id));

    if (timer == nullptr) {
        LOG_CRITICAL(Kernel, "Callback fired for invalid timer {:016x}", callback_id);
        return;
    }

    timer->Signal(cycles_late);
}

TimerManager::TimerManager(Core::Timing& timing) : timing(timing) {
    timer_callback_event_type =
        timing.RegisterEvent("TimerCallback", [this](u64 thread_id, s64 cycle_late) {
            TimerCallback(thread_id, cycle_late);
        });
}

template <class Archive>
void TimerManager::serialize(Archive& ar, const unsigned int) {
    ar & next_timer_callback_id;
    ar & timer_callback_table;
}
SERIALIZE_IMPL(TimerManager)

} // namespace Kernel
