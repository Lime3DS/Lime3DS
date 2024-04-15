// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/string.hpp>
#include "common/archives.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/thread.h"

SERIALIZE_EXPORT_IMPL(Kernel::Semaphore)

namespace Kernel {

Semaphore::Semaphore(KernelSystem& kernel) : WaitObject(kernel) {}

Semaphore::~Semaphore() {
    if (resource_limit) {
        resource_limit->Release(ResourceLimitType::Semaphore, 1);
    }
}

ResultVal<std::shared_ptr<Semaphore>> KernelSystem::CreateSemaphore(s32 initial_count,
                                                                    s32 max_count,
                                                                    std::string name) {

    R_UNLESS(initial_count <= max_count, ResultInvalidCombinationKernel);

    // When the semaphore is created, some slots are reserved for other threads,
    // and the rest is reserved for the caller thread
    auto semaphore = std::make_shared<Semaphore>(*this);
    semaphore->max_count = max_count;
    semaphore->available_count = initial_count;
    semaphore->name = std::move(name);
    return semaphore;
}

bool Semaphore::ShouldWait(const Thread* thread) const {
    return available_count <= 0;
}

void Semaphore::Acquire(Thread* thread) {
    if (available_count <= 0) {
        return;
    }
    --available_count;
}

Result Semaphore::Release(s32* out_count, s32 release_count) {
    R_UNLESS(max_count >= release_count + available_count, ResultOutOfRangeKernel);

    *out_count = available_count;
    available_count += release_count;
    WakeupAllWaitingThreads();

    return ResultSuccess;
}

template <class Archive>
void Semaphore::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<WaitObject>(*this);
    ar & max_count;
    ar & available_count;
    ar & name;
    ar & resource_limit;
}
SERIALIZE_IMPL(Semaphore)

} // namespace Kernel
