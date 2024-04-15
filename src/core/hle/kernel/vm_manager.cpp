// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <iterator>
#include <boost/serialization/map.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/split_member.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/service/plgldr/plgldr.h"
#include "core/memory.h"

SERIALIZE_EXPORT_IMPL(Kernel::VirtualMemoryArea)

namespace Kernel {

static const char* GetMemoryStateName(MemoryState state) {
    static const char* names[] = {
        "Free",   "Reserved",   "IO",      "Static", "Code",      "Private",
        "Shared", "Continuous", "Aliased", "Alias",  "AliasCode", "Locked",
    };

    return names[(int)state];
}

bool VirtualMemoryArea::CanBeMergedWith(const VirtualMemoryArea& next) const {
    ASSERT(base + size == next.base);
    if (permissions != next.permissions || meminfo_state != next.meminfo_state ||
        type != next.type) {
        return false;
    }
    if (type == VMAType::BackingMemory &&
        backing_memory.GetPtr() + size != next.backing_memory.GetPtr()) {
        return false;
    }
    return true;
}

template <class Archive>
void VirtualMemoryArea::serialize(Archive& ar, const unsigned int) {
    ar & base;
    ar & size;
    ar & type;
    ar & permissions;
    ar & meminfo_state;
    ar & backing_memory;
}
SERIALIZE_IMPL(VirtualMemoryArea)

VMManager::VMManager(Memory::MemorySystem& memory, Kernel::Process& proc)
    : page_table(std::make_shared<Memory::PageTable>()), memory(memory), process(proc) {
    Reset();
}

VMManager::~VMManager() = default;

void VMManager::Reset() {
    ASSERT(!is_locked);

    vma_map.clear();

    // Initialize the map with a single free region covering the entire managed space.
    VirtualMemoryArea initial_vma;
    initial_vma.size = MAX_ADDRESS;
    vma_map.emplace(initial_vma.base, initial_vma);

    page_table->Clear();

    UpdatePageTableForVMA(initial_vma);
}

VMManager::VMAHandle VMManager::FindVMA(VAddr target) const {
    if (target >= MAX_ADDRESS) {
        return vma_map.end();
    } else {
        return std::prev(vma_map.upper_bound(target));
    }
}

ResultVal<VAddr> VMManager::MapBackingMemoryToBase(VAddr base, u32 region_size, MemoryRef memory,
                                                   u32 size, MemoryState state) {
    ASSERT(!is_locked);

    // Find the first Free VMA.
    VMAHandle vma_handle = std::find_if(vma_map.begin(), vma_map.end(), [&](const auto& vma) {
        if (vma.second.type != VMAType::Free)
            return false;

        VAddr vma_end = vma.second.base + vma.second.size;
        return vma_end > base && vma_end >= base + size;
    });

    VAddr target = std::max(base, vma_handle->second.base);

    // Do not try to allocate the block if there are no available addresses within the desired
    // region.
    if (vma_handle == vma_map.end() || target + size > base + region_size) {
        return Result(ErrorDescription::OutOfMemory, ErrorModule::Kernel,
                      ErrorSummary::OutOfResource, ErrorLevel::Permanent);
    }

    auto result = MapBackingMemory(target, memory, size, state);

    if (result.Failed())
        return result.Code();

    return target;
}

ResultVal<VMManager::VMAHandle> VMManager::MapBackingMemory(VAddr target, MemoryRef memory,
                                                            u32 size, MemoryState state) {
    ASSERT(!is_locked);
    ASSERT(memory.GetPtr() != nullptr);

    // This is the appropriately sized VMA that will turn into our allocation.
    CASCADE_RESULT(VMAIter vma_handle, CarveVMA(target, size));
    VirtualMemoryArea& final_vma = vma_handle->second;
    ASSERT(final_vma.size == size);

    final_vma.type = VMAType::BackingMemory;
    final_vma.permissions = VMAPermission::ReadWrite;
    final_vma.meminfo_state = state;
    final_vma.backing_memory = memory;
    UpdatePageTableForVMA(final_vma);

    return MergeAdjacent(vma_handle);
}

Result VMManager::ChangeMemoryState(VAddr target, u32 size, MemoryState expected_state,
                                    VMAPermission expected_perms, MemoryState new_state,
                                    VMAPermission new_perms) {
    if (is_locked) {
        return ResultSuccess;
    }

    VAddr target_end = target + size;
    VMAIter begin_vma = StripIterConstness(FindVMA(target));
    VMAIter i_end = vma_map.lower_bound(target_end);

    if (begin_vma == vma_map.end())
        return ResultInvalidAddress;

    for (auto i = begin_vma; i != i_end; ++i) {
        auto& vma = i->second;
        if (vma.meminfo_state != expected_state) {
            return ResultInvalidAddressState;
        }
        u32 perms = static_cast<u32>(expected_perms);
        if ((static_cast<u32>(vma.permissions) & perms) != perms) {
            return ResultInvalidAddressState;
        }
    }

    CASCADE_RESULT(auto vma, CarveVMARange(target, size));

    const VMAIter end = vma_map.end();
    // The comparison against the end of the range must be done using addresses since VMAs can be
    // merged during this process, causing invalidation of the iterators.
    while (vma != end && vma->second.base < target_end) {
        vma->second.permissions = new_perms;
        vma->second.meminfo_state = new_state;
        UpdatePageTableForVMA(vma->second);
        vma = std::next(MergeAdjacent(vma));
    }

    return ResultSuccess;
}

VMManager::VMAIter VMManager::Unmap(VMAIter vma_handle) {
    ASSERT(!is_locked);

    VirtualMemoryArea& vma = vma_handle->second;
    vma.type = VMAType::Free;
    vma.permissions = VMAPermission::None;
    vma.meminfo_state = MemoryState::Free;
    vma.backing_memory = nullptr;

    UpdatePageTableForVMA(vma);

    return MergeAdjacent(vma_handle);
}

Result VMManager::UnmapRange(VAddr target, u32 size) {
    ASSERT(!is_locked);

    CASCADE_RESULT(VMAIter vma, CarveVMARange(target, size));
    const VAddr target_end = target + size;

    const VMAIter end = vma_map.end();
    // The comparison against the end of the range must be done using addresses since VMAs can be
    // merged during this process, causing invalidation of the iterators.
    while (vma != end && vma->second.base < target_end) {
        vma = std::next(Unmap(vma));
    }

    ASSERT(FindVMA(target)->second.size >= size);
    return ResultSuccess;
}

VMManager::VMAHandle VMManager::Reprotect(VMAHandle vma_handle, VMAPermission new_perms) {
    ASSERT(!is_locked);

    VMAIter iter = StripIterConstness(vma_handle);

    VirtualMemoryArea& vma = iter->second;
    vma.permissions = new_perms;
    UpdatePageTableForVMA(vma);

    return MergeAdjacent(iter);
}

Result VMManager::ReprotectRange(VAddr target, u32 size, VMAPermission new_perms) {
    ASSERT(!is_locked);

    CASCADE_RESULT(VMAIter vma, CarveVMARange(target, size));
    const VAddr target_end = target + size;

    const VMAIter end = vma_map.end();
    // The comparison against the end of the range must be done using addresses since VMAs can be
    // merged during this process, causing invalidation of the iterators.
    while (vma != end && vma->second.base < target_end) {
        vma = std::next(StripIterConstness(Reprotect(vma, new_perms)));
    }

    return ResultSuccess;
}

void VMManager::LogLayout(Common::Log::Level log_level) const {
    for (const auto& p : vma_map) {
        const VirtualMemoryArea& vma = p.second;
        LOG_GENERIC(Common::Log::Class::Kernel, log_level, "{:08X} - {:08X}  size: {:8X} {}{}{} {}",
                    vma.base, vma.base + vma.size, vma.size,
                    (u8)vma.permissions & (u8)VMAPermission::Read ? 'R' : '-',
                    (u8)vma.permissions & (u8)VMAPermission::Write ? 'W' : '-',
                    (u8)vma.permissions & (u8)VMAPermission::Execute ? 'X' : '-',
                    GetMemoryStateName(vma.meminfo_state));
    }
}

void VMManager::Unlock() {
    is_locked = false;
}

VMManager::VMAIter VMManager::StripIterConstness(const VMAHandle& iter) {
    // This uses a neat C++ trick to convert a const_iterator to a regular iterator, given
    // non-const access to its container.
    return vma_map.erase(iter, iter); // Erases an empty range of elements
}

ResultVal<VMManager::VMAIter> VMManager::CarveVMA(VAddr base, u32 size) {
    ASSERT_MSG((size & Memory::CITRA_PAGE_MASK) == 0, "non-page aligned size: {:#10X}", size);
    ASSERT_MSG((base & Memory::CITRA_PAGE_MASK) == 0, "non-page aligned base: {:#010X}", base);

    VMAIter vma_handle = StripIterConstness(FindVMA(base));
    if (vma_handle == vma_map.end()) {
        // Target address is outside the range managed by the kernel
        return ResultInvalidAddress;
    }

    const VirtualMemoryArea& vma = vma_handle->second;
    if (vma.type != VMAType::Free) {
        // Region is already allocated
        return ResultInvalidAddressState;
    }

    const VAddr start_in_vma = base - vma.base;
    const VAddr end_in_vma = start_in_vma + size;

    if (end_in_vma > vma.size) {
        // Requested allocation doesn't fit inside VMA
        return ResultInvalidAddressState;
    }

    if (end_in_vma != vma.size) {
        // Split VMA at the end of the allocated region
        SplitVMA(vma_handle, end_in_vma);
    }
    if (start_in_vma != 0) {
        // Split VMA at the start of the allocated region
        vma_handle = SplitVMA(vma_handle, start_in_vma);
    }

    return vma_handle;
}

ResultVal<VMManager::VMAIter> VMManager::CarveVMARange(VAddr target, u32 size) {
    ASSERT_MSG((size & Memory::CITRA_PAGE_MASK) == 0, "non-page aligned size: {:#10X}", size);
    ASSERT_MSG((target & Memory::CITRA_PAGE_MASK) == 0, "non-page aligned base: {:#010X}", target);

    const VAddr target_end = target + size;
    ASSERT(target_end >= target);
    ASSERT(target_end <= MAX_ADDRESS);
    ASSERT(size > 0);

    VMAIter begin_vma = StripIterConstness(FindVMA(target));
    const VMAIter i_end = vma_map.lower_bound(target_end);
    if (std::any_of(begin_vma, i_end,
                    [](const auto& entry) { return entry.second.type == VMAType::Free; })) {
        return ResultInvalidAddressState;
    }

    if (target != begin_vma->second.base) {
        begin_vma = SplitVMA(begin_vma, target - begin_vma->second.base);
    }

    VMAIter end_vma = StripIterConstness(FindVMA(target_end));
    if (end_vma != vma_map.end() && target_end != end_vma->second.base) {
        end_vma = SplitVMA(end_vma, target_end - end_vma->second.base);
    }

    return begin_vma;
}

VMManager::VMAIter VMManager::SplitVMA(VMAIter vma_handle, u32 offset_in_vma) {
    VirtualMemoryArea& old_vma = vma_handle->second;
    VirtualMemoryArea new_vma = old_vma; // Make a copy of the VMA

    // For now, don't allow no-op VMA splits (trying to split at a boundary) because it's probably
    // a bug. This restriction might be removed later.
    ASSERT(offset_in_vma < old_vma.size);
    ASSERT(offset_in_vma > 0);

    old_vma.size = offset_in_vma;
    new_vma.base += offset_in_vma;
    new_vma.size -= offset_in_vma;

    switch (new_vma.type) {
    case VMAType::Free:
        break;
    case VMAType::BackingMemory:
        new_vma.backing_memory += offset_in_vma;
        break;
    }

    ASSERT(old_vma.CanBeMergedWith(new_vma));

    return vma_map.emplace_hint(std::next(vma_handle), new_vma.base, new_vma);
}

VMManager::VMAIter VMManager::MergeAdjacent(VMAIter iter) {
    const VMAIter next_vma = std::next(iter);
    if (next_vma != vma_map.end() && iter->second.CanBeMergedWith(next_vma->second)) {
        iter->second.size += next_vma->second.size;
        vma_map.erase(next_vma);
    }

    if (iter != vma_map.begin()) {
        VMAIter prev_vma = std::prev(iter);
        if (prev_vma->second.CanBeMergedWith(iter->second)) {
            prev_vma->second.size += iter->second.size;
            vma_map.erase(iter);
            iter = prev_vma;
        }
    }

    return iter;
}

void VMManager::UpdatePageTableForVMA(const VirtualMemoryArea& vma) {
    switch (vma.type) {
    case VMAType::Free:
        memory.UnmapRegion(*page_table, vma.base, vma.size);
        break;
    case VMAType::BackingMemory:
        memory.MapMemoryRegion(*page_table, vma.base, vma.size, vma.backing_memory);
        break;
    }

    auto plgldr = Service::PLGLDR::GetService(Core::System::GetInstance());
    if (plgldr)
        plgldr->OnMemoryChanged(process, Core::System::GetInstance().Kernel());
}

ResultVal<std::vector<std::pair<MemoryRef, u32>>> VMManager::GetBackingBlocksForRange(VAddr address,
                                                                                      u32 size) {
    std::vector<std::pair<MemoryRef, u32>> backing_blocks;
    VAddr interval_target = address;
    while (interval_target != address + size) {
        auto vma = FindVMA(interval_target);
        if (vma->second.type != VMAType::BackingMemory) {
            LOG_ERROR(Kernel, "Trying to use already freed memory");
            return ResultInvalidAddressState;
        }

        VAddr interval_end = std::min(address + size, vma->second.base + vma->second.size);
        u32 interval_size = interval_end - interval_target;
        auto backing_memory = vma->second.backing_memory + (interval_target - vma->second.base);
        backing_blocks.push_back({backing_memory, interval_size});

        interval_target += interval_size;
    }
    return backing_blocks;
}

template <class Archive>
void VMManager::serialize(Archive& ar, const unsigned int) {
    ar & vma_map;
    ar & page_table;
    if (Archive::is_loading::value) {
        is_locked = true;
    }
}
SERIALIZE_IMPL(VMManager)

} // namespace Kernel
