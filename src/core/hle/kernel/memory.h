// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <boost/icl/interval_set.hpp>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "common/serialization/boost_interval_set.hpp"

namespace Kernel {

struct AddressMapping;
class VMManager;

struct MemoryRegionInfo {
    u32 base; // Not an address, but offset from start of FCRAM
    u32 size;
    u32 used;

    // The domain of the interval_set are offsets from start of FCRAM
    using IntervalSet = boost::icl::interval_set<u32>;
    using Interval = IntervalSet::interval_type;

    IntervalSet free_blocks;

    // When locked, Free calls will be ignored, while Allocate calls will hit an assert. A memory
    // region locks itself after deserialization.
    bool is_locked{};

    /**
     * Reset the allocator state
     * @param base The base offset the beginning of FCRAM.
     * @param size The region size this allocator manages
     */
    void Reset(u32 base, u32 size);

    /**
     * Allocates memory from the heap.
     * @param size The size of memory to allocate.
     * @returns The set of blocks that make up the allocation request. Empty set if there is no
     *     enough space.
     */
    IntervalSet HeapAllocate(u32 size);

    /**
     * Allocates memory from the linear heap with specific address and size.
     * @param offset the address offset to the beginning of FCRAM.
     * @param size size of the memory to allocate.
     * @returns true if the allocation is successful. false if the requested region is not free.
     */
    bool LinearAllocate(u32 offset, u32 size);

    /**
     * Allocates memory from the linear heap with only size specified.
     * @param size size of the memory to allocate.
     * @returns the address offset to the beginning of FCRAM; null if there is no enough space
     */
    std::optional<u32> LinearAllocate(u32 size);

    /**
     * Allocates memory from the linear heap with only size specified.
     * @param size size of the memory to allocate.
     * @returns the address offset to the found block, searching from the end of FCRAM; null if
     * there is no enough space
     */
    std::optional<u32> RLinearAllocate(u32 size);

    /**
     * Frees one segment of memory. The memory must have been allocated as heap or linear heap.
     * @param offset the region address offset to the beginning of FCRAM.
     * @param size the size of the region to free.
     */
    void Free(u32 offset, u32 size);

    /**
     * Unlock the MemoryRegion. Used after loading is completed.
     */
    void Unlock();

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::MemoryRegionInfo)
