// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

namespace Kernel {

class ResourceLimit;

class Semaphore final : public WaitObject {
public:
    explicit Semaphore(KernelSystem& kernel);
    ~Semaphore() override;

    std::string GetTypeName() const override {
        return "Semaphore";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Semaphore;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    std::shared_ptr<ResourceLimit> resource_limit;
    s32 max_count;       ///< Maximum number of simultaneous holders the semaphore can have
    s32 available_count; ///< Number of free slots left in the semaphore
    std::string name;    ///< Name of semaphore (optional)

    bool ShouldWait(const Thread* thread) const override;
    void Acquire(Thread* thread) override;

    /**
     * Releases a certain number of slots from a semaphore.
     * @param release_count The number of slots to release
     * @return The number of free slots the semaphore had before this call
     */
    Result Release(s32* out_count, s32 release_count);

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::Semaphore)
CONSTRUCT_KERNEL_OBJECT(Kernel::Semaphore)
