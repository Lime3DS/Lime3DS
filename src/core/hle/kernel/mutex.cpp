// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"

SERIALIZE_EXPORT_IMPL(Kernel::Mutex)

namespace Kernel {

void ReleaseThreadMutexes(Thread* thread) {
    for (auto& mtx : thread->held_mutexes) {
        mtx->lock_count = 0;
        mtx->holding_thread = nullptr;
        mtx->WakeupAllWaitingThreads();
    }
    thread->held_mutexes.clear();
}

Mutex::Mutex(KernelSystem& kernel) : WaitObject(kernel), kernel(kernel) {}

Mutex::~Mutex() {
    if (resource_limit) {
        resource_limit->Release(ResourceLimitType::Mutex, 1);
    }
}

std::shared_ptr<Mutex> KernelSystem::CreateMutex(bool initial_locked, std::string name) {
    auto mutex = std::make_shared<Mutex>(*this);
    mutex->lock_count = 0;
    mutex->name = std::move(name);
    mutex->holding_thread = nullptr;

    // Acquire mutex with current thread if initialized as locked
    if (initial_locked) {
        mutex->Acquire(GetCurrentThreadManager().GetCurrentThread());
    }

    return mutex;
}

bool Mutex::ShouldWait(const Thread* thread) const {
    return lock_count > 0 && thread != holding_thread.get();
}

void Mutex::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    // Actually "acquire" the mutex only if we don't already have it
    if (lock_count == 0) {
        priority = thread->current_priority;
        thread->held_mutexes.insert(SharedFrom(this));
        holding_thread = SharedFrom(thread);
        thread->UpdatePriority();
        kernel.PrepareReschedule();
    }

    lock_count++;
}

Result Mutex::Release(Thread* thread) {
    // We can only release the mutex if it's held by the calling thread.
    if (thread != holding_thread.get()) {
        if (holding_thread) {
            LOG_ERROR(
                Kernel,
                "Tried to release a mutex (owned by thread id {}) from a different thread id {}",
                holding_thread->thread_id, thread->thread_id);
        }
        return Result(ErrCodes::WrongLockingThread, ErrorModule::Kernel,
                      ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    // Note: It should not be possible for the situation where the mutex has a holding thread with a
    // zero lock count to occur. The real kernel still checks for this, so we do too.
    if (lock_count <= 0)
        return Result(ErrorDescription::InvalidResultValue, ErrorModule::Kernel,
                      ErrorSummary::InvalidState, ErrorLevel::Permanent);

    lock_count--;

    // Yield to the next thread only if we've fully released the mutex
    if (lock_count == 0) {
        holding_thread->held_mutexes.erase(SharedFrom(this));
        holding_thread->UpdatePriority();
        holding_thread = nullptr;
        WakeupAllWaitingThreads();
        kernel.PrepareReschedule();
    }

    return ResultSuccess;
}

void Mutex::AddWaitingThread(std::shared_ptr<Thread> thread) {
    WaitObject::AddWaitingThread(thread);
    thread->pending_mutexes.insert(SharedFrom(this));
    UpdatePriority();
}

void Mutex::RemoveWaitingThread(Thread* thread) {
    WaitObject::RemoveWaitingThread(thread);
    thread->pending_mutexes.erase(SharedFrom(this));
    UpdatePriority();
}

void Mutex::UpdatePriority() {
    if (!holding_thread)
        return;

    u32 best_priority = ThreadPrioLowest;
    for (auto& waiter : GetWaitingThreads()) {
        if (waiter->current_priority < best_priority)
            best_priority = waiter->current_priority;
    }

    if (best_priority != priority) {
        priority = best_priority;
        holding_thread->UpdatePriority();
    }
}

template <class Archive>
void Mutex::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<WaitObject>(*this);
    ar & lock_count;
    ar & priority;
    ar & name;
    ar & holding_thread;
    ar & resource_limit;
}
SERIALIZE_IMPL(Mutex)

} // namespace Kernel
