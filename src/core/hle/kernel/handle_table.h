// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/result.h"

namespace Kernel {

enum KernelHandle : Handle {
    CurrentThread = 0xFFFF8000,
    CurrentProcess = 0xFFFF8001,
};

/**
 * This class allows the creation of Handles, which are references to objects that can be tested
 * for validity and looked up. Here they are used to pass references to kernel objects to/from the
 * emulated process. it has been designed so that it follows the same handle format and has
 * approximately the same restrictions as the handle manager in the CTR-OS.
 *
 * Handles contain two sub-fields: a slot index (bits 31:15) and a generation value (bits 14:0).
 * The slot index is used to index into the arrays in this class to access the data corresponding
 * to the Handle.
 *
 * To prevent accidental use of a freed Handle whose slot has already been reused, a global counter
 * is kept and incremented every time a Handle is created. This is the Handle's "generation". The
 * value of the counter is stored into the Handle as well as in the handle table (in the
 * "generations" array). When looking up a handle, the Handle's generation must match with the
 * value stored on the class, otherwise the Handle is considered invalid.
 *
 * To find free slots when allocating a Handle without needing to scan the entire object array, the
 * generations field of unallocated slots is re-purposed as a linked list of indices to free slots.
 * When a Handle is created, an index is popped off the list and used for the new Handle. When it
 * is destroyed, it is again pushed onto the list to be re-used by the next allocation. It is
 * likely that this allocation strategy differs from the one used in CTR-OS, but this hasn't been
 * verified and isn't likely to cause any problems.
 */
class HandleTable final : NonCopyable {
public:
    explicit HandleTable(KernelSystem& kernel);
    ~HandleTable();

    /**
     * Allocates a handle for the given object.
     * @return The created Handle or one of the following errors:
     *           - `ResultOutOfHandles`: the maximum number of handles has been exceeded.
     */
    Result Create(Handle* out_handle, std::shared_ptr<Object> obj);

    /**
     * Returns a new handle that points to the same object as the passed in handle.
     * @return The duplicated Handle or one of the following errors:
     *           - `ResultInvalidHandle`: an invalid handle was passed in.
     *           - Any errors returned by `Create()`.
     */
    Result Duplicate(Handle* out_handle, Handle handle);

    /**
     * Closes a handle, removing it from the table and decreasing the object's ref-count.
     * @return `ResultSuccess` or one of the following errors:
     *           - `ResultInvalidHandle`: an invalid handle was passed in.
     */
    Result Close(Handle handle);

    /// Checks if a handle is valid and points to an existing object.
    bool IsValid(Handle handle) const;

    /**
     * Looks up a handle.
     * @return Pointer to the looked-up object, or `nullptr` if the handle is not valid.
     */
    std::shared_ptr<Object> GetGeneric(Handle handle) const;

    /**
     * Looks up a handle while verifying its type.
     * @return Pointer to the looked-up object, or `nullptr` if the handle is not valid or its
     *         type differs from the requested one.
     */
    template <class T>
    std::shared_ptr<T> Get(Handle handle) const {
        return DynamicObjectCast<T>(GetGeneric(handle));
    }

    /// Closes all handles held in this table.
    void Clear();

private:
    /**
     * This is the maximum limit of handles allowed per process in CTR-OS. It can be further
     * reduced by ExHeader values, but this is not emulated here.
     */
    static const std::size_t MAX_COUNT = 4096;

    /// Stores the Object referenced by the handle or null if the slot is empty.
    std::array<std::shared_ptr<Object>, MAX_COUNT> objects;

    /**
     * The value of `next_generation` when the handle was created, used to check for validity. For
     * empty slots, contains the index of the next free slot in the list.
     */
    std::array<u16, MAX_COUNT> generations;

    /**
     * Global counter of the number of created handles. Stored in `generations` when a handle is
     * created, and wraps around to 1 when it hits 0x8000.
     */
    u16 next_generation;

    /// Head of the free slots linked list.
    u16 next_free_slot;

    KernelSystem& kernel;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::HandleTable)
CONSTRUCT_KERNEL_OBJECT(Kernel::HandleTable)
