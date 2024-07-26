// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/core_timing.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"

namespace Core {
class Timing;
}

namespace Kernel {

class TimerManager {
public:
    TimerManager(Core::Timing& timing);

private:
    /// The timer callback event, called when a timer is fired
    void TimerCallback(u64 callback_id, s64 cycles_late);

    Core::Timing& timing;

    /// The event type of the generic timer callback event
    Core::TimingEventType* timer_callback_event_type = nullptr;

    u64 next_timer_callback_id = 0;
    std::unordered_map<u64, Timer*> timer_callback_table;

    friend class Timer;
    friend class KernelSystem;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

class ResourceLimit;

class Timer final : public WaitObject {
public:
    explicit Timer(KernelSystem& kernel);
    ~Timer() override;

    std::string GetTypeName() const override {
        return "Timer";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Timer;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    ResetType GetResetType() const {
        return reset_type;
    }

    u64 GetInitialDelay() const {
        return initial_delay;
    }

    u64 GetIntervalDelay() const {
        return interval_delay;
    }

    bool ShouldWait(const Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void WakeupAllWaitingThreads() override;

    /**
     * Starts the timer, with the specified initial delay and interval.
     * @param initial Delay until the timer is first fired
     * @param interval Delay until the timer is fired after the first time
     */
    void Set(s64 initial, s64 interval);

    void Cancel();
    void Clear();

    /**
     * Signals the timer, waking up any waiting threads and rescheduling it
     * for the next interval.
     * This method should not be called from outside the timer callback handler,
     * lest multiple callback events get scheduled.
     */
    void Signal(s64 cycles_late);

    std::shared_ptr<ResourceLimit> resource_limit;

private:
    ResetType reset_type; ///< The ResetType of this timer

    u64 initial_delay;  ///< The delay until the timer fires for the first time
    u64 interval_delay; ///< The delay until the timer fires after the first time

    bool signaled;    ///< Whether the timer has been signaled or not
    std::string name; ///< Name of timer (optional)

    /// ID used as userdata to reference this object when inserting into the CoreTiming queue.
    u64 callback_id;

    KernelSystem& kernel;
    TimerManager& timer_manager;

    friend class KernelSystem;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::Timer)
CONSTRUCT_KERNEL_OBJECT(Kernel::Timer)
