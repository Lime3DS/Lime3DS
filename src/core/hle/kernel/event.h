// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <boost/serialization/export.hpp>
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/wait_object.h"

namespace Kernel {

class Event final : public WaitObject {
public:
    explicit Event(KernelSystem& kernel);
    ~Event() override;

    std::string GetTypeName() const override {
        return "Event";
    }
    std::string GetName() const override {
        return name;
    }
    void SetName(const std::string& name_) {
        name = name_;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    ResetType GetResetType() const {
        return reset_type;
    }

    bool ShouldWait(const Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void WakeupAllWaitingThreads() override;

    void Signal();
    void Clear();

    std::shared_ptr<ResourceLimit> resource_limit;

private:
    ResetType reset_type; ///< Current ResetType

    bool signaled;    ///< Whether the event has already been signaled
    std::string name; ///< Name of event (optional)

    friend class KernelSystem;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::Event)
CONSTRUCT_KERNEL_OBJECT(Kernel::Event)
