// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"

namespace Kernel {

class Thread;

/// Class that represents a Kernel object that a thread can be waiting on
class WaitObject : public Object {
public:
    using Object::Object;

    /**
     * Check if the specified thread should wait until the object is available
     * @param thread The thread about which we're deciding.
     * @return True if the current thread should wait due to this object being unavailable
     */
    virtual bool ShouldWait(const Thread* thread) const = 0;

    /// Acquire/lock the object for the specified thread if it is available
    virtual void Acquire(Thread* thread) = 0;

    /**
     * Add a thread to wait on this object
     * @param thread Pointer to thread to add
     */
    virtual void AddWaitingThread(std::shared_ptr<Thread> thread);

    /**
     * Removes a thread from waiting on this object (e.g. if it was resumed already)
     * @param thread Pointer to thread to remove
     */
    virtual void RemoveWaitingThread(Thread* thread);

    /**
     * Wake up all threads waiting on this object that can be awoken, in priority order,
     * and set the synchronization result and output of the thread.
     */
    virtual void WakeupAllWaitingThreads();

    /// Obtains the highest priority thread that is ready to run from this object's waiting list.
    std::shared_ptr<Thread> GetHighestPriorityReadyThread() const;

    /// Get a const reference to the waiting threads list for debug use
    const std::vector<std::shared_ptr<Thread>>& GetWaitingThreads() const;

    /// Sets a callback which is called when the object becomes available
    void SetHLENotifier(std::function<void()> callback);

private:
    /// Threads waiting for this object to become available
    std::vector<std::shared_ptr<Thread>> waiting_threads;

    /// Function to call when this object becomes available
    std::function<void()> hle_notifier;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

// Specialization of DynamicObjectCast for WaitObjects
template <>
inline std::shared_ptr<WaitObject> DynamicObjectCast<WaitObject>(std::shared_ptr<Object> object) {
    if (object != nullptr && object->IsWaitable()) {
        return std::static_pointer_cast<WaitObject>(object);
    }
    return nullptr;
}

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::WaitObject)
