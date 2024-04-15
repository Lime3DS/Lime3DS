// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <utility>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

SERIALIZE_EXPORT_IMPL(Kernel::WaitObject)

namespace Kernel {

template <class Archive>
void WaitObject::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Object>(*this);
    ar & waiting_threads;
    // NB: hle_notifier *not* serialized since it's a callback!
    // Fortunately it's only used in one place (DSP) so we can reconstruct it there
}
SERIALIZE_IMPL(WaitObject)

void WaitObject::AddWaitingThread(std::shared_ptr<Thread> thread) {
    auto itr = std::find(waiting_threads.begin(), waiting_threads.end(), thread);
    if (itr == waiting_threads.end())
        waiting_threads.push_back(std::move(thread));
}

void WaitObject::RemoveWaitingThread(Thread* thread) {
    auto itr = std::find_if(waiting_threads.begin(), waiting_threads.end(),
                            [thread](const auto& p) { return p.get() == thread; });
    // If a thread passed multiple handles to the same object,
    // the kernel might attempt to remove the thread from the object's
    // waiting threads list multiple times.
    if (itr != waiting_threads.end())
        waiting_threads.erase(itr);
}

std::shared_ptr<Thread> WaitObject::GetHighestPriorityReadyThread() const {
    Thread* candidate = nullptr;
    u32 candidate_priority = ThreadPrioLowest + 1;

    for (const auto& thread : waiting_threads) {
        // The list of waiting threads must not contain threads that are not waiting to be awakened.
        ASSERT_MSG(thread->status == ThreadStatus::WaitSynchAny ||
                       thread->status == ThreadStatus::WaitSynchAll ||
                       thread->status == ThreadStatus::WaitHleEvent,
                   "Inconsistent thread statuses in waiting_threads");

        if (thread->current_priority >= candidate_priority)
            continue;

        if (ShouldWait(thread.get()))
            continue;

        // A thread is ready to run if it's either in ThreadStatus::WaitSynchAny or
        // in ThreadStatus::WaitSynchAll and the rest of the objects it is waiting on are ready.
        bool ready_to_run = true;
        if (thread->status == ThreadStatus::WaitSynchAll) {
            ready_to_run = std::none_of(thread->wait_objects.begin(), thread->wait_objects.end(),
                                        [&thread](const std::shared_ptr<WaitObject>& object) {
                                            return object->ShouldWait(thread.get());
                                        });
        }

        if (ready_to_run) {
            candidate = thread.get();
            candidate_priority = thread->current_priority;
        }
    }

    return SharedFrom(candidate);
}

void WaitObject::WakeupAllWaitingThreads() {
    while (auto thread = GetHighestPriorityReadyThread()) {
        if (!thread->IsSleepingOnWaitAll()) {
            Acquire(thread.get());
        } else {
            for (auto& object : thread->wait_objects) {
                object->Acquire(thread.get());
            }
        }

        // Invoke the wakeup callback before clearing the wait objects
        if (thread->wakeup_callback)
            thread->wakeup_callback->WakeUp(ThreadWakeupReason::Signal, thread, SharedFrom(this));

        for (auto& object : thread->wait_objects)
            object->RemoveWaitingThread(thread.get());
        thread->wait_objects.clear();

        thread->ResumeFromWait();
    }

    if (hle_notifier)
        hle_notifier();
}

const std::vector<std::shared_ptr<Thread>>& WaitObject::GetWaitingThreads() const {
    return waiting_threads;
}

void WaitObject::SetHLENotifier(std::function<void()> callback) {
    hle_notifier = std::move(callback);
}

} // namespace Kernel
