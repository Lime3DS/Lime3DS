// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "common/serialization/atomic.h"
#include "core/global.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

class KernelSystem;

using Handle = u32;

enum class HandleType : u32 {
    Unknown,
    Event,
    Mutex,
    SharedMemory,
    Thread,
    Process,
    AddressArbiter,
    Semaphore,
    Timer,
    ResourceLimit,
    CodeSet,
    ClientPort,
    ServerPort,
    ClientSession,
    ServerSession,
};

enum {
    DEFAULT_STACK_SIZE = 0x4000,
};

class Object : NonCopyable, public std::enable_shared_from_this<Object> {
public:
    explicit Object(KernelSystem& kernel);
    virtual ~Object();

    /// Returns a unique identifier for the object. For debugging purposes only.
    u32 GetObjectId() const {
        return object_id.load(std::memory_order_relaxed);
    }

    virtual std::string GetTypeName() const {
        return "[BAD KERNEL OBJECT TYPE]";
    }
    virtual std::string GetName() const {
        return "[UNKNOWN KERNEL OBJECT]";
    }
    virtual HandleType GetHandleType() const = 0;

    /**
     * Check if a thread can wait on the object
     * @return True if a thread can wait on the object, otherwise false
     */
    bool IsWaitable() const;

private:
    std::atomic<u32> object_id;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

template <typename T>
std::shared_ptr<T> SharedFrom(T* raw) {
    if (raw == nullptr)
        return nullptr;

    return std::static_pointer_cast<T>(raw->shared_from_this());
}

/**
 * Attempts to downcast the given Object pointer to a pointer to T.
 * @return Derived pointer to the object, or `nullptr` if `object` isn't of type T.
 */
template <typename T>
inline std::shared_ptr<T> DynamicObjectCast(std::shared_ptr<Object> object) {
    if (object != nullptr && object->GetHandleType() == T::HANDLE_TYPE) {
        return std::static_pointer_cast<T>(object);
    }
    return nullptr;
}

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::Object)

#define CONSTRUCT_KERNEL_OBJECT(T)                                                                 \
    namespace boost::serialization {                                                               \
    template <class Archive>                                                                       \
    void load_construct_data(Archive& ar, T* t, const unsigned int file_version) {                 \
        ::new (t) T(Core::Global<Kernel::KernelSystem>());                                         \
    }                                                                                              \
    }
