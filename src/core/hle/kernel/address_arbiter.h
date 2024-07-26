// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/result.h"

// Address arbiters are an underlying kernel synchronization object that can be created/used via
// supervisor calls (SVCs). They function as sort of a global lock. Typically, games/other CTR
// applications use them as an underlying mechanism to implement thread-safe barriers, events, and
// semaphores.

namespace Kernel {

class Thread;
class ResourceLimit;

enum class ArbitrationType : u32 {
    Signal,
    WaitIfLessThan,
    DecrementAndWaitIfLessThan,
    WaitIfLessThanWithTimeout,
    DecrementAndWaitIfLessThanWithTimeout,
};

class AddressArbiter final : public Object, public WakeupCallback {
public:
    explicit AddressArbiter(KernelSystem& kernel);
    ~AddressArbiter() override;

    std::string GetTypeName() const override {
        return "Arbiter";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::AddressArbiter;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::shared_ptr<ResourceLimit> resource_limit;
    std::string name; ///< Name of address arbiter object (optional)

    Result ArbitrateAddress(std::shared_ptr<Thread> thread, ArbitrationType type, VAddr address,
                            s32 value, u64 nanoseconds);

    class Callback;

private:
    KernelSystem& kernel;

    /// Puts the thread to wait on the specified arbitration address under this address arbiter.
    void WaitThread(std::shared_ptr<Thread> thread, VAddr wait_address);

    /// Resume all threads found to be waiting on the address under this address arbiter
    u64 ResumeAllThreads(VAddr address);

    /// Resume one thread found to be waiting on the address under this address arbiter and return
    /// the resumed thread.
    bool ResumeHighestPriorityThread(VAddr address);

    /// Threads waiting for the address arbiter to be signaled.
    std::vector<std::shared_ptr<Thread>> waiting_threads;

    std::shared_ptr<Callback> timeout_callback;

    void WakeUp(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                std::shared_ptr<WaitObject> object) override;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::AddressArbiter)
BOOST_CLASS_EXPORT_KEY(Kernel::AddressArbiter::Callback)
CONSTRUCT_KERNEL_OBJECT(Kernel::AddressArbiter)
