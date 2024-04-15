// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>
#include <boost/serialization/set.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/kernel/config_mem.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/result.h"
#include "core/memory.h"

SERIALIZE_EXPORT_IMPL(Kernel::MemoryRegionInfo)

namespace Kernel {

/// Size of the APPLICATION, SYSTEM and BASE memory regions (respectively) for each system
/// memory configuration type.
static const u32 memory_region_sizes[8][3] = {
    // Old 3DS layouts
    {0x04000000, 0x02C00000, 0x01400000}, // 0
    {/* This appears to be unused. */},   // 1
    {0x06000000, 0x00C00000, 0x01400000}, // 2
    {0x05000000, 0x01C00000, 0x01400000}, // 3
    {0x04800000, 0x02400000, 0x01400000}, // 4
    {0x02000000, 0x04C00000, 0x01400000}, // 5

    // New 3DS layouts
    {0x07C00000, 0x06400000, 0x02000000}, // 6
    {0x0B200000, 0x02E00000, 0x02000000}, // 7
};

void KernelSystem::MemoryInit(MemoryMode memory_mode, New3dsMemoryMode n3ds_mode,
                              u64 override_init_time) {
    const bool is_new_3ds = Settings::values.is_new_3ds.GetValue();
    u32 mem_type_index = static_cast<u32>(memory_mode);
    u32 reported_mem_type = static_cast<u32>(memory_mode);
    if (is_new_3ds) {
        if (n3ds_mode == New3dsMemoryMode::NewProd || n3ds_mode == New3dsMemoryMode::NewDev2) {
            mem_type_index = 6;
            reported_mem_type = 6;
        } else if (n3ds_mode == New3dsMemoryMode::NewDev1) {
            mem_type_index = 7;
            reported_mem_type = 7;
        } else {
            // On the N3ds, all O3ds configurations (<=5) are forced to 6 instead.
            mem_type_index = 6;
        }
    }

    // The kernel allocation regions (APPLICATION, SYSTEM and BASE) are laid out in sequence, with
    // the sizes specified in the memory_region_sizes table.
    VAddr base = 0;
    for (int i = 0; i < 3; ++i) {
        memory_regions[i]->Reset(base, memory_region_sizes[mem_type_index][i]);

        base += memory_regions[i]->size;
    }

    // We must've allocated the entire FCRAM by the end
    ASSERT(base == (is_new_3ds ? Memory::FCRAM_N3DS_SIZE : Memory::FCRAM_SIZE));

    config_mem_handler = std::make_shared<ConfigMem::Handler>();
    auto& config_mem = config_mem_handler->GetConfigMem();
    config_mem.app_mem_type = reported_mem_type;
    config_mem.app_mem_alloc = memory_region_sizes[reported_mem_type][0];
    config_mem.sys_mem_alloc = memory_regions[1]->size;
    config_mem.base_mem_alloc = memory_regions[2]->size;

    shared_page_handler = std::make_shared<SharedPage::Handler>(timing, override_init_time);
}

std::shared_ptr<MemoryRegionInfo> KernelSystem::GetMemoryRegion(MemoryRegion region) {
    switch (region) {
    case MemoryRegion::APPLICATION:
        return memory_regions[0];
    case MemoryRegion::SYSTEM:
        return memory_regions[1];
    case MemoryRegion::BASE:
        return memory_regions[2];
    default:
        UNREACHABLE();
    }
}

void KernelSystem::HandleSpecialMapping(VMManager& address_space, const AddressMapping& mapping) {
    using namespace Memory;

    struct MemoryArea {
        VAddr vaddr_base;
        PAddr paddr_base;
        u32 size;
    };

    // The order of entries in this array is important. The VRAM and IO VAddr ranges overlap, and
    // VRAM must be tried first.
    static constexpr MemoryArea memory_areas[] = {
        {VRAM_VADDR, VRAM_PADDR, VRAM_SIZE},
        {IO_AREA_VADDR, IO_AREA_PADDR, IO_AREA_SIZE},
        {DSP_RAM_VADDR, DSP_RAM_PADDR, DSP_RAM_SIZE},
        {N3DS_EXTRA_RAM_VADDR, N3DS_EXTRA_RAM_PADDR, N3DS_EXTRA_RAM_SIZE - 0x20000},
    };

    VAddr mapping_limit = mapping.address + mapping.size;
    if (mapping_limit < mapping.address) {
        LOG_CRITICAL(Loader, "Mapping size overflowed: address=0x{:08X} size=0x{:X}",
                     mapping.address, mapping.size);
        return;
    }

    auto area =
        std::find_if(std::begin(memory_areas), std::end(memory_areas), [&](const auto& area) {
            return mapping.address >= area.vaddr_base &&
                   mapping_limit <= area.vaddr_base + area.size;
        });
    if (area == std::end(memory_areas)) {
        LOG_ERROR(Loader,
                  "Unhandled special mapping: address=0x{:08X} size=0x{:X}"
                  " read_only={} unk_flag={}",
                  mapping.address, mapping.size, mapping.read_only, mapping.unk_flag);
        return;
    }

    u32 offset_into_region = mapping.address - area->vaddr_base;
    if (area->paddr_base == IO_AREA_PADDR) {
        LOG_ERROR(Loader, "MMIO mappings are not supported yet. phys_addr=0x{:08X}",
                  area->paddr_base + offset_into_region);
        return;
    }

    auto target_pointer = memory.GetPhysicalRef(area->paddr_base + offset_into_region);

    // TODO(yuriks): This flag seems to have some other effect, but it's unknown what
    MemoryState memory_state = mapping.unk_flag ? MemoryState::Static : MemoryState::IO;

    auto vma =
        address_space.MapBackingMemory(mapping.address, target_pointer, mapping.size, memory_state)
            .Unwrap();
    address_space.Reprotect(vma,
                            mapping.read_only ? VMAPermission::Read : VMAPermission::ReadWrite);
}

void KernelSystem::MapSharedPages(VMManager& address_space) {
    auto cfg_mem_vma = address_space
                           .MapBackingMemory(Memory::CONFIG_MEMORY_VADDR, {config_mem_handler},
                                             Memory::CONFIG_MEMORY_SIZE, MemoryState::Shared)
                           .Unwrap();
    address_space.Reprotect(cfg_mem_vma, VMAPermission::Read);

    auto shared_page_vma = address_space
                               .MapBackingMemory(Memory::SHARED_PAGE_VADDR, {shared_page_handler},
                                                 Memory::SHARED_PAGE_SIZE, MemoryState::Shared)
                               .Unwrap();
    address_space.Reprotect(shared_page_vma, VMAPermission::Read);
}

void MemoryRegionInfo::Reset(u32 base, u32 size) {
    ASSERT(!is_locked);

    this->base = base;
    this->size = size;
    used = 0;
    free_blocks.clear();

    // mark the entire region as free
    free_blocks.insert(Interval::right_open(base, base + size));
}

MemoryRegionInfo::IntervalSet MemoryRegionInfo::HeapAllocate(u32 size) {
    ASSERT(!is_locked);

    IntervalSet result;
    u32 rest = size;

    // Try allocating from the higher address
    for (auto iter = free_blocks.rbegin(); iter != free_blocks.rend(); ++iter) {
        ASSERT(iter->bounds() == boost::icl::interval_bounds::right_open());
        if (iter->upper() - iter->lower() >= rest) {
            // Requested size is fulfilled with this block
            result += Interval(iter->upper() - rest, iter->upper());
            rest = 0;
            break;
        }
        result += *iter;
        rest -= iter->upper() - iter->lower();
    }

    if (rest != 0) {
        // There is no enough free space
        return {};
    }

    free_blocks -= result;
    used += size;
    return result;
}

bool MemoryRegionInfo::LinearAllocate(u32 offset, u32 size) {
    ASSERT(!is_locked);

    Interval interval(offset, offset + size);
    if (!boost::icl::contains(free_blocks, interval)) {
        // The requested range is already allocated
        return false;
    }
    free_blocks -= interval;
    used += size;
    return true;
}

std::optional<u32> MemoryRegionInfo::LinearAllocate(u32 size) {
    ASSERT(!is_locked);

    // Find the first sufficient continuous block from the lower address
    for (const auto& interval : free_blocks) {
        ASSERT(interval.bounds() == boost::icl::interval_bounds::right_open());
        if (interval.upper() - interval.lower() >= size) {
            Interval allocated(interval.lower(), interval.lower() + size);
            free_blocks -= allocated;
            used += size;
            return allocated.lower();
        }
    }

    // No sufficient block found
    return std::nullopt;
}

std::optional<u32> MemoryRegionInfo::RLinearAllocate(u32 size) {
    ASSERT(!is_locked);

    // Find the first sufficient continuous block from the upper address
    for (auto iter = free_blocks.rbegin(); iter != free_blocks.rend(); ++iter) {
        auto interval = *iter;
        ASSERT(interval.bounds() == boost::icl::interval_bounds::right_open());
        if (interval.upper() - interval.lower() >= size) {
            Interval allocated(interval.upper() - size, interval.upper());
            free_blocks -= allocated;
            used += size;
            return allocated.lower();
        }
    }

    // No sufficient block found
    return std::nullopt;
}

void MemoryRegionInfo::Free(u32 offset, u32 size) {
    if (is_locked) {
        return;
    }

    Interval interval(offset, offset + size);
    ASSERT(!boost::icl::intersects(free_blocks, interval)); // must be allocated blocks
    free_blocks += interval;
    used -= size;
}

void MemoryRegionInfo::Unlock() {
    is_locked = false;
}

template <class Archive>
void MemoryRegionInfo::serialize(Archive& ar, const unsigned int) {
    ar & base;
    ar & size;
    ar & used;
    ar & free_blocks;
    if (Archive::is_loading::value) {
        is_locked = true;
    }
}
SERIALIZE_IMPL(MemoryRegionInfo)

} // namespace Kernel
