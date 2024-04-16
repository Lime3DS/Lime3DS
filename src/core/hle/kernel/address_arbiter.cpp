// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include "common/archives.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/memory.h"

SERIALIZE_EXPORT_IMPL(Kernel::AddressArbiter)
SERIALIZE_EXPORT_IMPL(Kernel::AddressArbiter::Callback)

namespace Kernel {

void AddressArbiter::WaitThread(std::shared_ptr<Thread> thread, VAddr wait_address) {
    thread->wait_address = wait_address;
    thread->status = ThreadStatus::WaitArb;
    waiting_threads.emplace_back(std::move(thread));
}

u64 AddressArbiter::ResumeAllThreads(VAddr address) {
    // Determine which threads are waiting on this address, those should be woken up.
    auto itr = std::stable_partition(waiting_threads.begin(), waiting_threads.end(),
                                     [address](const auto& thread) {
                                         ASSERT_MSG(thread->status == ThreadStatus::WaitArb,
                                                    "Inconsistent AddressArbiter state");
                                         return thread->wait_address != address;
                                     });

    // Wake up all the found threads
    const u64 num_threads = std::distance(itr, waiting_threads.end());
    std::for_each(itr, waiting_threads.end(), [](auto& thread) { thread->ResumeFromWait(); });

    // Remove the woken up threads from the wait list.
    waiting_threads.erase(itr, waiting_threads.end());
    return num_threads;
}

bool AddressArbiter::ResumeHighestPriorityThread(VAddr address) {
    // Determine which threads are waiting on this address, those should be considered for wakeup.
    auto matches_start = std::stable_partition(
        waiting_threads.begin(), waiting_threads.end(), [address](const auto& thread) {
            ASSERT_MSG(thread->status == ThreadStatus::WaitArb,
                       "Inconsistent AddressArbiter state");
            return thread->wait_address != address;
        });

    // Iterate through threads, find highest priority thread that is waiting to be arbitrated.
    // Note: The real kernel will pick the first thread in the list if more than one have the
    // same highest priority value. Lower priority values mean higher priority.
    auto itr = std::min_element(matches_start, waiting_threads.end(),
                                [](const auto& lhs, const auto& rhs) {
                                    return lhs->current_priority < rhs->current_priority;
                                });

    if (itr == waiting_threads.end()) {
        return false;
    }

    auto thread = *itr;
    thread->ResumeFromWait();
    waiting_threads.erase(itr);

    return true;
}

AddressArbiter::AddressArbiter(KernelSystem& kernel)
    : Object(kernel), kernel(kernel), timeout_callback(std::make_shared<Callback>(*this)) {}

AddressArbiter::~AddressArbiter() {
    if (resource_limit) {
        resource_limit->Release(ResourceLimitType::AddressArbiter, 1);
    }
}

std::shared_ptr<AddressArbiter> KernelSystem::CreateAddressArbiter(std::string name) {
    auto address_arbiter = std::make_shared<AddressArbiter>(*this);
    address_arbiter->name = std::move(name);
    return address_arbiter;
}

class AddressArbiter::Callback : public WakeupCallback {
public:
    explicit Callback(AddressArbiter& _parent) : parent(_parent) {}
    AddressArbiter& parent;

    void WakeUp(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                std::shared_ptr<WaitObject> object) override {
        parent.WakeUp(reason, std::move(thread), std::move(object));
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<WakeupCallback>(*this);
    }
    friend class boost::serialization::access;
};

void AddressArbiter::WakeUp(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                            std::shared_ptr<WaitObject> object) {
    ASSERT(reason == ThreadWakeupReason::Timeout);
    // Remove the newly-awakened thread from the Arbiter's waiting list.
    waiting_threads.erase(std::remove(waiting_threads.begin(), waiting_threads.end(), thread),
                          waiting_threads.end());
};

Result AddressArbiter::ArbitrateAddress(std::shared_ptr<Thread> thread, ArbitrationType type,
                                        VAddr address, s32 value, u64 nanoseconds) {
    switch (type) {

    // Signal thread(s) waiting for arbitrate address...
    case ArbitrationType::Signal: {
        u64 num_threads{};

        // Negative value means resume all threads
        if (value < 0) {
            num_threads = ResumeAllThreads(address);
        } else {
            // Resume first N threads
            for (s32 i = 0; i < value; i++) {
                num_threads += ResumeHighestPriorityThread(address);
            }
        }

        // Prevents lag from low priority threads that spam svcArbitrateAddress and wake no threads
        // The tick count is taken directly from official HOS kernel. The priority value is one less
        // than official kernel as the affected FMV threads dont meet the priority threshold of 50.
        // TODO: Revisit this when scheduler is rewritten and adjust if there isn't a problem there.
        if (num_threads == 0 && thread->current_priority >= 49) {
            kernel.current_cpu->GetTimer().AddTicks(1614u);
        }
        break;
    }
    // Wait current thread (acquire the arbiter)...
    case ArbitrationType::WaitIfLessThan:
        if ((s32)kernel.memory.Read32(address) < value) {
            WaitThread(std::move(thread), address);
        }
        break;
    case ArbitrationType::WaitIfLessThanWithTimeout:
        if ((s32)kernel.memory.Read32(address) < value) {
            thread->wakeup_callback = timeout_callback;
            thread->WakeAfterDelay(nanoseconds);
            WaitThread(std::move(thread), address);
        }
        break;
    case ArbitrationType::DecrementAndWaitIfLessThan: {
        s32 memory_value = kernel.memory.Read32(address);
        if (memory_value < value) {
            // Only change the memory value if the thread should wait
            kernel.memory.Write32(address, (s32)memory_value - 1);
            WaitThread(std::move(thread), address);
        }
        break;
    }
    case ArbitrationType::DecrementAndWaitIfLessThanWithTimeout: {
        s32 memory_value = kernel.memory.Read32(address);
        if (memory_value < value) {
            // Only change the memory value if the thread should wait
            kernel.memory.Write32(address, (s32)memory_value - 1);
            thread->wakeup_callback = timeout_callback;
            thread->WakeAfterDelay(nanoseconds);
            WaitThread(std::move(thread), address);
        }
        break;
    }

    default:
        LOG_ERROR(Kernel, "unknown type={}", type);
        return ResultInvalidEnumValueFnd;
    }

    // The calls that use a timeout seem to always return a Timeout error even if they did not put
    // the thread to sleep
    if (type == ArbitrationType::WaitIfLessThanWithTimeout ||
        type == ArbitrationType::DecrementAndWaitIfLessThanWithTimeout) {
        return ResultTimeout;
    }
    return ResultSuccess;
}

template <class Archive>
void AddressArbiter::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Object>(*this);
    ar & name;
    ar & waiting_threads;
    ar & timeout_callback;
    ar & resource_limit;
}
SERIALIZE_IMPL(AddressArbiter)

} // namespace Kernel

namespace boost::serialization {

template <class Archive>
void save_construct_data(Archive& ar, const Kernel::AddressArbiter::Callback* t,
                         const unsigned int) {
    ar << Kernel::SharedFrom(&t->parent);
}

template <class Archive>
void load_construct_data(Archive& ar, Kernel::AddressArbiter::Callback* t, const unsigned int) {
    std::shared_ptr<Kernel::AddressArbiter> parent;
    ar >> parent;
    ::new (t) Kernel::AddressArbiter::Callback(*parent);
}

} // namespace boost::serialization
