// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/container/flat_set.hpp>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "common/thread_queue_list.h"
#include "core/arm/arm_interface.h"
#include "core/core_timing.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

namespace Kernel {

class Mutex;
class Process;

enum ThreadPriority : u32 {
    ThreadPrioHighest = 0,      ///< Highest thread priority
    ThreadPrioUserlandMax = 24, ///< Highest thread priority for userland apps
    ThreadPrioDefault = 48,     ///< Default thread priority for userland apps
    ThreadPrioLowest = 63,      ///< Lowest thread priority
};

enum ThreadProcessorId : s32 {
    ThreadProcessorIdDefault = -2, ///< Run thread on default core specified by exheader
    ThreadProcessorIdAll = -1,     ///< Run thread on either core
    ThreadProcessorId0 = 0,        ///< Run thread on core 0 (AppCore)
    ThreadProcessorId1 = 1,        ///< Run thread on core 1 (SysCore)
    ThreadProcessorId2 = 2,        ///< Run thread on core 2 (additional n3ds core)
    ThreadProcessorId3 = 3,        ///< Run thread on core 3 (additional n3ds core)
    ThreadProcessorIdMax = 4,      ///< Processor ID must be less than this
};

enum class ThreadStatus {
    Running,      ///< Currently running
    Ready,        ///< Ready to run
    WaitArb,      ///< Waiting on an address arbiter
    WaitSleep,    ///< Waiting due to a SleepThread SVC
    WaitIPC,      ///< Waiting for the reply from an IPC request
    WaitSynchAny, ///< Waiting due to WaitSynch1 or WaitSynchN with wait_all = false
    WaitSynchAll, ///< Waiting due to WaitSynchronizationN with wait_all = true
    WaitHleEvent, ///< Waiting due to an HLE handler pausing the thread
    Dormant,      ///< Created but not yet made ready
    Dead          ///< Run to completion, or forcefully terminated
};

enum class ThreadWakeupReason {
    Signal, // The thread was woken up by WakeupAllWaitingThreads due to an object signal.
    Timeout // The thread was woken up due to a wait timeout.
};

class Thread;

class WakeupCallback {
public:
    virtual ~WakeupCallback() = default;
    virtual void WakeUp(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                        std::shared_ptr<WaitObject> object) = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

class ThreadManager {
public:
    explicit ThreadManager(Kernel::KernelSystem& kernel, u32 core_id);
    ~ThreadManager();

    /**
     * Gets the current thread
     */
    Thread* GetCurrentThread() const;

    /**
     * Reschedules to the next available thread (call after current thread is suspended)
     */
    void Reschedule();

    /**
     * Prints the thread queue for debugging purposes
     */
    void DebugThreadQueue();

    /**
     * Returns whether there are any threads that are ready to run.
     */
    bool HaveReadyThreads();

    /**
     * Waits the current thread on a sleep
     */
    void WaitCurrentThread_Sleep();

    /**
     * Stops the current thread and removes it from the thread_list
     */
    void ExitCurrentThread();

    /**
     * Terminates all threads belonging to a specific process.
     */
    void TerminateProcessThreads(std::shared_ptr<Process> process);

    /**
     * Get a const reference to the thread list for debug use
     */
    std::span<const std::shared_ptr<Thread>> GetThreadList();

    void SetCPU(Core::ARM_Interface& cpu_) {
        cpu = &cpu_;
    }

private:
    /**
     * Switches the CPU's active thread context to that of the specified thread
     * @param new_thread The thread to switch to
     */
    void SwitchContext(Thread* new_thread);

    /**
     * Pops and returns the next thread from the thread queue
     * @return A pointer to the next ready thread
     */
    Thread* PopNextReadyThread();

    /**
     * Callback that will wake up the thread it was scheduled for
     * @param thread_id The ID of the thread that's been awoken
     * @param cycles_late The number of CPU cycles that have passed since the desired wakeup time
     */
    void ThreadWakeupCallback(u64 thread_id, s64 cycles_late);

    Kernel::KernelSystem& kernel;
    Core::ARM_Interface* cpu;

    std::shared_ptr<Thread> current_thread;
    Common::ThreadQueueList<Thread*, ThreadPrioLowest + 1> ready_queue;
    std::deque<Thread*> unscheduled_ready_queue;
    std::unordered_map<u64, Thread*> wakeup_callback_table;

    /// Event type for the thread wake up event
    Core::TimingEventType* ThreadWakeupEventType = nullptr;

    // Lists all threadsthat aren't deleted.
    std::vector<std::shared_ptr<Thread>> thread_list;

    friend class Thread;
    friend class KernelSystem;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

class Thread final : public WaitObject {
public:
    explicit Thread(KernelSystem&, u32 core_id);
    ~Thread() override;

    std::string GetName() const override {
        return name;
    }
    std::string GetTypeName() const override {
        return "Thread";
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Thread;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    bool ShouldWait(const Thread* thread) const override;
    void Acquire(Thread* thread) override;

    /**
     * Gets the thread's current priority
     * @return The current thread's priority
     */
    u32 GetPriority() const {
        return current_priority;
    }

    /**
     * Sets the thread's current priority
     * @param priority The new priority
     */
    void SetPriority(u32 priority);

    /**
     * Boost's a thread's priority to the best priority among the thread's held mutexes.
     * This prevents priority inversion via priority inheritance.
     */
    void UpdatePriority();

    /**
     * Temporarily boosts the thread's priority until the next time it is scheduled
     * @param priority The new priority
     */
    void BoostPriority(u32 priority);

    /**
     * Gets the thread's thread ID
     * @return The thread's ID
     */
    u32 GetThreadId() const {
        return thread_id;
    }

    /**
     * Resumes a thread from waiting
     */
    void ResumeFromWait();

    /**
     * Schedules an event to wake up the specified thread after the specified delay
     * @param nanoseconds The time this thread will be allowed to sleep for
     * @param thread_safe_mode Set to true if called from a different thread than the emulator
     * thread, such as coroutines.
     */
    void WakeAfterDelay(s64 nanoseconds, bool thread_safe_mode = false);

    /**
     * Sets the result after the thread awakens (from either WaitSynchronization SVC)
     * @param result Value to set to the returned result
     */
    void SetWaitSynchronizationResult(Result result);

    /**
     * Sets the output parameter value after the thread awakens (from WaitSynchronizationN SVC only)
     * @param output Value to set to the output parameter
     */
    void SetWaitSynchronizationOutput(s32 output);

    /**
     * Retrieves the index that this particular object occupies in the list of objects
     * that the thread passed to WaitSynchronizationN, starting the search from the last element.
     * It is used to set the output value of WaitSynchronizationN when the thread is awakened.
     * When a thread wakes up due to an object signal, the kernel will use the index of the last
     * matching object in the wait objects list in case of having multiple instances of the same
     * object in the list.
     * @param object Object to query the index of.
     */
    s32 GetWaitObjectIndex(const WaitObject* object) const;

    /**
     * Stops a thread, invalidating it from further use
     */
    void Stop();

    /**
     * Returns the Thread Local Storage address of the current thread
     * @returns VAddr of the thread's TLS
     */
    VAddr GetTLSAddress() const {
        return tls_address;
    }

    /**
     * Returns the address of the current thread's command buffer, located in the TLS.
     * @returns VAddr of the thread's command buffer.
     */
    VAddr GetCommandBufferAddress() const;

    /**
     * Returns whether this thread is waiting for all the objects in
     * its wait list to become ready, as a result of a WaitSynchronizationN call
     * with wait_all = true.
     */
    bool IsSleepingOnWaitAll() const {
        return status == ThreadStatus::WaitSynchAll;
    }

    Core::ARM_Interface::ThreadContext context{};

    u32 thread_id;

    bool can_schedule{true};
    ThreadStatus status;
    VAddr entry_point;
    VAddr stack_top;

    u32 nominal_priority; ///< Nominal thread priority, as set by the emulated application
    u32 current_priority; ///< Current thread priority, can be temporarily changed

    u64 last_running_ticks; ///< CPU tick when thread was last running

    s32 processor_id;

    VAddr tls_address; ///< Virtual address of the Thread Local Storage of the thread

    /// Mutexes currently held by this thread, which will be released when it exits.
    boost::container::flat_set<std::shared_ptr<Mutex>> held_mutexes{};

    /// Mutexes that this thread is currently waiting for.
    boost::container::flat_set<std::shared_ptr<Mutex>> pending_mutexes{};

    std::weak_ptr<Process> owner_process{}; ///< Process that owns this thread

    /// Objects that the thread is waiting on, in the same order as they were
    /// passed to WaitSynchronization1/N.
    std::vector<std::shared_ptr<WaitObject>> wait_objects{};

    VAddr wait_address; ///< If waiting on an AddressArbiter, this is the arbitration address

    std::string name{};

    /// Callback that will be invoked when the thread is resumed from a waiting state. If the thread
    /// was waiting via WaitSynchronizationN then the object will be the last object that became
    /// available. In case of a timeout, the object will be nullptr.
    std::shared_ptr<WakeupCallback> wakeup_callback{};

    const u32 core_id;

private:
    ThreadManager& thread_manager;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

/**
 * Sets up the primary application thread
 * @param kernel The kernel instance on which the thread is created
 * @param entry_point The address at which the thread should start execution
 * @param priority The priority to give the main thread
 * @param owner_process The parent process for the main thread
 * @return A shared pointer to the main thread
 */
std::shared_ptr<Thread> SetupMainThread(KernelSystem& kernel, u32 entry_point, u32 priority,
                                        std::shared_ptr<Process> owner_process);

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::Thread)
BOOST_CLASS_EXPORT_KEY(Kernel::WakeupCallback)

namespace boost::serialization {

template <class Archive>
void save_construct_data(Archive& ar, const Kernel::Thread* t, const unsigned int) {
    ar << t->core_id;
}

template <class Archive>
void load_construct_data(Archive& ar, Kernel::Thread* t, const unsigned int) {
    u32 core_id;
    ar >> core_id;
    ::new (t) Kernel::Thread(Core::Global<Kernel::KernelSystem>(), core_id);
}

} // namespace boost::serialization
