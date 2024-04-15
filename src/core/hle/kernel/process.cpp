// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "common/serialization/boost_vector.hpp"
#include "core/core.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/service/plgldr/plgldr.h"
#include "core/loader/loader.h"
#include "core/memory.h"

SERIALIZE_EXPORT_IMPL(Kernel::AddressMapping)
SERIALIZE_EXPORT_IMPL(Kernel::Process)
SERIALIZE_EXPORT_IMPL(Kernel::CodeSet)
SERIALIZE_EXPORT_IMPL(Kernel::CodeSet::Segment)

namespace Kernel {

template <class Archive>
void AddressMapping::serialize(Archive& ar, const unsigned int) {
    ar & address;
    ar & size;
    ar & read_only;
    ar & unk_flag;
}
SERIALIZE_IMPL(AddressMapping)

template <class Archive>
void Process::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Object>(*this);
    ar & handle_table;
    ar & codeset; // TODO: Replace with apploader reference
    ar & resource_limit;
    ar & svc_access_mask;
    ar & handle_table_size;
    ar&(boost::container::vector<AddressMapping, boost::container::dtl::static_storage_allocator<
                                                     AddressMapping, 8, 0, true>>&)address_mappings;
    ar & flags.raw;
    ar & no_thread_restrictions;
    ar & kernel_version;
    ar & ideal_processor;
    ar & status;
    ar & process_id;
    ar & creation_time_ticks;
    ar & vm_manager;
    ar & memory_used;
    ar & memory_region;
    ar & holding_memory;
    ar & holding_tls_memory;
    ar & tls_slots;
}
SERIALIZE_IMPL(Process)

std::shared_ptr<CodeSet> KernelSystem::CreateCodeSet(std::string name, u64 program_id) {
    auto codeset{std::make_shared<CodeSet>(*this)};

    codeset->name = std::move(name);
    codeset->program_id = program_id;

    return codeset;
}

CodeSet::CodeSet(KernelSystem& kernel) : Object(kernel) {}
CodeSet::~CodeSet() {}

template <class Archive>
void CodeSet::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Object>(*this);
    ar & memory;
    ar & segments;
    ar & entrypoint;
    ar & name;
    ar & program_id;
}
SERIALIZE_IMPL(CodeSet)

template <class Archive>
void CodeSet::Segment::serialize(Archive& ar, const unsigned int) {
    ar & offset;
    ar & addr;
    ar & size;
}
SERIALIZE_IMPL(CodeSet::Segment)

std::shared_ptr<Process> KernelSystem::CreateProcess(std::shared_ptr<CodeSet> code_set) {
    auto process{std::make_shared<Process>(*this)};

    process->codeset = std::move(code_set);
    process->flags.raw = 0;
    process->flags.memory_region.Assign(MemoryRegion::APPLICATION);
    process->status = ProcessStatus::Created;
    process->process_id = ++next_process_id;
    process->creation_time_ticks = timing.GetTicks();

    process_list.push_back(process);
    return process;
}

void KernelSystem::TerminateProcess(std::shared_ptr<Process> process) {
    LOG_INFO(Kernel_SVC, "Process {} exiting", process->process_id);

    ASSERT_MSG(process->status == ProcessStatus::Running, "Process has already exited");
    process->status = ProcessStatus::Exited;

    // Stop all process threads.
    for (u32 core = 0; core < Core::GetNumCores(); core++) {
        GetThreadManager(core).TerminateProcessThreads(process);
    }

    process->Exit();
    std::erase(process_list, process);
}

void Process::ParseKernelCaps(const u32* kernel_caps, std::size_t len) {
    for (std::size_t i = 0; i < len; ++i) {
        u32 descriptor = kernel_caps[i];
        u32 type = descriptor >> 20;

        if (descriptor == 0xFFFFFFFF) {
            // Unused descriptor entry
            continue;
        } else if ((type & 0xF00) == 0xE00) { // 0x0FFF
            // Allowed interrupts list
            LOG_WARNING(Loader, "ExHeader allowed interrupts list ignored");
        } else if ((type & 0xF80) == 0xF00) { // 0x07FF
            // Allowed syscalls mask
            unsigned int index = ((descriptor >> 24) & 7) * 24;
            u32 bits = descriptor & 0xFFFFFF;

            while (bits && index < svc_access_mask.size()) {
                svc_access_mask.set(index, bits & 1);
                ++index;
                bits >>= 1;
            }
        } else if ((type & 0xFF0) == 0xFE0) { // 0x00FF
            // Handle table size
            handle_table_size = descriptor & 0x3FF;
        } else if ((type & 0xFF8) == 0xFF0) { // 0x007F
            // Misc. flags
            flags.raw = descriptor & 0xFFFF;
        } else if ((type & 0xFFE) == 0xFF8) { // 0x001F
            // Mapped memory range
            if (i + 1 >= len || ((kernel_caps[i + 1] >> 20) & 0xFFE) != 0xFF8) {
                LOG_WARNING(Loader, "Incomplete exheader memory range descriptor ignored.");
                continue;
            }
            u32 end_desc = kernel_caps[i + 1];
            ++i; // Skip over the second descriptor on the next iteration

            AddressMapping mapping;
            mapping.address = descriptor << 12;
            VAddr end_address = end_desc << 12;

            if (mapping.address < end_address) {
                mapping.size = end_address - mapping.address;
            } else {
                mapping.size = 0;
            }

            mapping.read_only = (descriptor & (1 << 20)) != 0;
            mapping.unk_flag = (end_desc & (1 << 20)) != 0;

            address_mappings.push_back(mapping);
        } else if ((type & 0xFFF) == 0xFFE) { // 0x000F
            // Mapped memory page
            AddressMapping mapping;
            mapping.address = descriptor << 12;
            mapping.size = Memory::CITRA_PAGE_SIZE;
            mapping.read_only = false;
            mapping.unk_flag = false;

            address_mappings.push_back(mapping);
        } else if ((type & 0xFE0) == 0xFC0) { // 0x01FF
            // Kernel version
            kernel_version = descriptor & 0xFFFF;

            int minor = kernel_version & 0xFF;
            int major = (kernel_version >> 8) & 0xFF;
            LOG_INFO(Loader, "ExHeader kernel version: {}.{}", major, minor);
        } else {
            LOG_ERROR(Loader, "Unhandled kernel caps descriptor: 0x{:08X}", descriptor);
        }
    }
}

void Process::Set3dsxKernelCaps() {
    svc_access_mask.set();

    address_mappings = {
        {0x1FF50000, 0x8000, true},    // part of DSP RAM
        {0x1FF70000, 0x8000, true},    // part of DSP RAM
        {0x1F000000, 0x600000, false}, // entire VRAM
    };

    // Similar to Rosalina, we set kernel version to a recent one.
    // This is 11.17.0, to be consistent with core/hle/kernel/config_mem.cpp
    // TODO: refactor kernel version out so it is configurable and consistent
    // among all relevant places.
    kernel_version = 0x23a;
}

void Process::Run(s32 main_thread_priority, u32 stack_size) {
    memory_region = kernel.GetMemoryRegion(flags.memory_region);

    // Ensure we can reserve a thread. Real kernel returns 0xC860180C if this fails.
    if (!resource_limit->Reserve(ResourceLimitType::Thread, 1)) {
        return;
    }

    VAddr out_addr{};

    auto MapSegment = [&](CodeSet::Segment& segment, VMAPermission permissions,
                          MemoryState memory_state) {
        HeapAllocate(std::addressof(out_addr), segment.addr, segment.size, permissions,
                     memory_state, true);
        kernel.memory.WriteBlock(*this, segment.addr, codeset->memory.data() + segment.offset,
                                 segment.size);
    };

    // Map CodeSet segments
    MapSegment(codeset->CodeSegment(), VMAPermission::ReadExecute, MemoryState::Code);
    MapSegment(codeset->RODataSegment(), VMAPermission::Read, MemoryState::Code);
    MapSegment(codeset->DataSegment(), VMAPermission::ReadWrite, MemoryState::Private);

    // Allocate and map stack
    HeapAllocate(std::addressof(out_addr), Memory::HEAP_VADDR_END - stack_size, stack_size,
                 VMAPermission::ReadWrite, MemoryState::Locked, true);

    // Map special address mappings
    kernel.MapSharedPages(vm_manager);
    for (const auto& mapping : address_mappings) {
        kernel.HandleSpecialMapping(vm_manager, mapping);
    }

    auto plgldr = Service::PLGLDR::GetService(Core::System::GetInstance());
    if (plgldr) {
        plgldr->OnProcessRun(*this, kernel);
    }

    status = ProcessStatus::Running;

    vm_manager.LogLayout(Common::Log::Level::Debug);
    Kernel::SetupMainThread(kernel, codeset->entrypoint, main_thread_priority, SharedFrom(this));
}

void Process::Exit() {
    auto plgldr = Service::PLGLDR::GetService(Core::System::GetInstance());
    if (plgldr) {
        plgldr->OnProcessExit(*this, kernel);
    }
}

VAddr Process::GetLinearHeapAreaAddress() const {
    // Starting from system version 8.0.0 a new linear heap layout is supported to allow usage of
    // the extra RAM in the n3DS.
    return kernel_version < 0x22C ? Memory::LINEAR_HEAP_VADDR : Memory::NEW_LINEAR_HEAP_VADDR;
}

VAddr Process::GetLinearHeapBase() const {
    return GetLinearHeapAreaAddress() + memory_region->base;
}

VAddr Process::GetLinearHeapLimit() const {
    return GetLinearHeapBase() + memory_region->size;
}

Result Process::HeapAllocate(VAddr* out_addr, VAddr target, u32 size, VMAPermission perms,
                             MemoryState memory_state, bool skip_range_check) {
    LOG_DEBUG(Kernel, "Allocate heap target={:08X}, size={:08X}", target, size);
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        if (!skip_range_check) {
            LOG_ERROR(Kernel, "Invalid heap address");
            return ResultInvalidAddress;
        }
    }
    {
        auto vma = vm_manager.FindVMA(target);
        if (vma->second.type != VMAType::Free ||
            vma->second.base + vma->second.size < target + size) {
            LOG_ERROR(Kernel, "Trying to allocate already allocated memory");
            return ResultInvalidAddressState;
        }
    }
    auto allocated_fcram = memory_region->HeapAllocate(size);
    if (allocated_fcram.empty()) {
        LOG_ERROR(Kernel, "Not enough space");
        return ResultOutOfHeapMemory;
    }

    // Maps heap block by block
    VAddr interval_target = target;
    for (const auto& interval : allocated_fcram) {
        u32 interval_size = interval.upper() - interval.lower();
        LOG_DEBUG(Kernel, "Allocated FCRAM region lower={:08X}, upper={:08X}", interval.lower(),
                  interval.upper());
        std::fill(kernel.memory.GetFCRAMPointer(interval.lower()),
                  kernel.memory.GetFCRAMPointer(interval.upper()), 0);
        auto vma = vm_manager.MapBackingMemory(interval_target,
                                               kernel.memory.GetFCRAMRef(interval.lower()),
                                               interval_size, memory_state);
        ASSERT(vma.Succeeded());
        vm_manager.Reprotect(vma.Unwrap(), perms);
        interval_target += interval_size;
    }

    holding_memory += allocated_fcram;
    memory_used += size;
    resource_limit->Reserve(ResourceLimitType::Commit, size);

    *out_addr = target;
    return ResultSuccess;
}

Result Process::HeapFree(VAddr target, u32 size) {
    LOG_DEBUG(Kernel, "Free heap target={:08X}, size={:08X}", target, size);
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        LOG_ERROR(Kernel, "Invalid heap address");
        return ResultInvalidAddress;
    }

    R_SUCCEED_IF(size == 0);

    // Free heaps block by block
    CASCADE_RESULT(auto backing_blocks, vm_manager.GetBackingBlocksForRange(target, size));
    for (const auto& [backing_memory, block_size] : backing_blocks) {
        const auto backing_offset = kernel.memory.GetFCRAMOffset(backing_memory.GetPtr());
        memory_region->Free(backing_offset, block_size);
        holding_memory -= MemoryRegionInfo::Interval(backing_offset, backing_offset + block_size);
    }

    Result result = vm_manager.UnmapRange(target, size);
    ASSERT(result.IsSuccess());

    memory_used -= size;
    resource_limit->Release(ResourceLimitType::Commit, size);

    return ResultSuccess;
}

Result Process::LinearAllocate(VAddr* out_addr, VAddr target, u32 size, VMAPermission perms) {
    LOG_DEBUG(Kernel, "Allocate linear heap target={:08X}, size={:08X}", target, size);
    u32 physical_offset;
    if (target == 0) {
        auto offset = memory_region->LinearAllocate(size);
        if (!offset) {
            LOG_ERROR(Kernel, "Not enough space");
            return ResultOutOfHeapMemory;
        }
        physical_offset = *offset;
        target = physical_offset + GetLinearHeapAreaAddress();
    } else {
        if (target < GetLinearHeapBase() || target + size > GetLinearHeapLimit() ||
            target + size < target) {
            LOG_ERROR(Kernel, "Invalid linear heap address");
            return ResultInvalidAddress;
        }

        // Kernel would crash/return error when target doesn't meet some requirement.
        // It seems that target is required to follow immediately after the allocated linear heap,
        // or cover the entire hole if there is any.
        // Right now we just ignore these checks because they are still unclear. Further more,
        // games and homebrew only ever seem to pass target = 0 here (which lets the kernel decide
        // the address), so this not important.

        physical_offset = target - GetLinearHeapAreaAddress(); // relative to FCRAM
        if (!memory_region->LinearAllocate(physical_offset, size)) {
            LOG_ERROR(Kernel, "Trying to allocate already allocated memory");
            return ResultInvalidAddressState;
        }
    }

    auto backing_memory = kernel.memory.GetFCRAMRef(physical_offset);

    std::fill(backing_memory.GetPtr(), backing_memory.GetPtr() + size, 0);
    auto vma = vm_manager.MapBackingMemory(target, backing_memory, size, MemoryState::Continuous);
    ASSERT(vma.Succeeded());
    vm_manager.Reprotect(vma.Unwrap(), perms);

    holding_memory += MemoryRegionInfo::Interval(physical_offset, physical_offset + size);
    memory_used += size;
    resource_limit->Reserve(ResourceLimitType::Commit, size);

    LOG_DEBUG(Kernel, "Allocated at target={:08X}", target);
    *out_addr = target;
    return ResultSuccess;
}

Result Process::LinearFree(VAddr target, u32 size) {
    LOG_DEBUG(Kernel, "Free linear heap target={:08X}, size={:08X}", target, size);
    if (target < GetLinearHeapBase() || target + size > GetLinearHeapLimit() ||
        target + size < target) {
        LOG_ERROR(Kernel, "Invalid linear heap address");
        return ResultInvalidAddress;
    }

    R_SUCCEED_IF(size == 0);
    R_TRY(vm_manager.UnmapRange(target, size));

    u32 physical_offset = target - GetLinearHeapAreaAddress(); // relative to FCRAM
    memory_region->Free(physical_offset, size);

    holding_memory -= MemoryRegionInfo::Interval(physical_offset, physical_offset + size);
    memory_used -= size;
    resource_limit->Release(ResourceLimitType::Commit, size);

    return ResultSuccess;
}

ResultVal<VAddr> Process::AllocateThreadLocalStorage() {
    std::size_t tls_page;
    std::size_t tls_slot;
    bool needs_allocation = true;

    // Iterate over all the allocated pages, and try to find one where not all slots are used.
    for (tls_page = 0; tls_page < tls_slots.size(); ++tls_page) {
        const auto& page_tls_slots = tls_slots[tls_page];
        if (!page_tls_slots.all()) {
            // We found a page with at least one free slot, find which slot it is.
            for (tls_slot = 0; tls_slot < page_tls_slots.size(); ++tls_slot) {
                if (!page_tls_slots.test(tls_slot)) {
                    needs_allocation = false;
                    break;
                }
            }

            if (!needs_allocation) {
                break;
            }
        }
    }

    if (needs_allocation) {
        tls_page = tls_slots.size();
        tls_slot = 0;

        LOG_DEBUG(Kernel, "Allocating new TLS page in slot {}", tls_page);

        // There are no already-allocated pages with free slots, lets allocate a new one.
        // TLS pages are allocated from the BASE region in the linear heap.
        auto base_memory_region = kernel.GetMemoryRegion(MemoryRegion::BASE);

        // Allocate some memory from the end of the linear heap for this region.
        auto offset = base_memory_region->LinearAllocate(Memory::CITRA_PAGE_SIZE);
        if (!offset) {
            LOG_ERROR(Kernel_SVC,
                      "Not enough space in BASE linear region to allocate a new TLS page");
            return ResultOutOfMemory;
        }

        holding_tls_memory +=
            MemoryRegionInfo::Interval(*offset, *offset + Memory::CITRA_PAGE_SIZE);
        memory_used += Memory::CITRA_PAGE_SIZE;

        // The page is completely available at the start.
        tls_slots.emplace_back(0);

        // Map the page to the current process' address space.
        auto tls_page_addr =
            Memory::TLS_AREA_VADDR + static_cast<VAddr>(tls_page) * Memory::CITRA_PAGE_SIZE;
        vm_manager.MapBackingMemory(tls_page_addr, kernel.memory.GetFCRAMRef(*offset),
                                    Memory::CITRA_PAGE_SIZE, MemoryState::Locked);

        LOG_DEBUG(Kernel, "Allocated TLS page at addr={:08X}", tls_page_addr);
    } else {
        LOG_DEBUG(Kernel, "Allocating TLS in existing page slot {}", tls_page);
    }

    // Mark the slot as used
    tls_slots[tls_page].set(tls_slot);

    auto tls_address = Memory::TLS_AREA_VADDR +
                       static_cast<VAddr>(tls_page) * Memory::CITRA_PAGE_SIZE +
                       static_cast<VAddr>(tls_slot) * Memory::TLS_ENTRY_SIZE;
    kernel.memory.ZeroBlock(*this, tls_address, Memory::TLS_ENTRY_SIZE);

    return tls_address;
}

Result Process::Map(VAddr target, VAddr source, u32 size, VMAPermission perms, bool privileged) {
    LOG_DEBUG(Kernel, "Map memory target={:08X}, source={:08X}, size={:08X}, perms={:08X}", target,
              source, size, perms);
    if (!privileged && (source < Memory::HEAP_VADDR || source + size > Memory::HEAP_VADDR_END ||
                        source + size < source)) {
        LOG_ERROR(Kernel, "Invalid source address");
        return ResultInvalidAddress;
    }

    // TODO(wwylele): check target address range. Is it also restricted to heap region?

    // Check range overlapping
    if (source - target < size || target - source < size) {
        if (privileged) {
            if (source == target) {
                // privileged Map allows identical source and target address, which simply changes
                // the state and the permission of the memory
                return vm_manager.ChangeMemoryState(source, size, MemoryState::Private,
                                                    VMAPermission::ReadWrite,
                                                    MemoryState::AliasCode, perms);
            } else {
                return ResultInvalidAddress;
            }
        } else {
            return ResultInvalidAddressState;
        }
    }

    auto vma = vm_manager.FindVMA(target);
    if (vma->second.type != VMAType::Free || vma->second.base + vma->second.size < target + size) {
        LOG_ERROR(Kernel, "Trying to map to already allocated memory");
        return ResultInvalidAddressState;
    }

    MemoryState source_state = privileged ? MemoryState::Locked : MemoryState::Aliased;
    MemoryState target_state = privileged ? MemoryState::AliasCode : MemoryState::Alias;
    VMAPermission source_perm = privileged ? VMAPermission::None : VMAPermission::ReadWrite;

    // Mark source region as Aliased
    R_TRY(vm_manager.ChangeMemoryState(source, size, MemoryState::Private, VMAPermission::ReadWrite,
                                       source_state, source_perm));

    CASCADE_RESULT(auto backing_blocks, vm_manager.GetBackingBlocksForRange(source, size));
    VAddr interval_target = target;
    for (const auto& [backing_memory, block_size] : backing_blocks) {
        auto target_vma =
            vm_manager.MapBackingMemory(interval_target, backing_memory, block_size, target_state);
        ASSERT(target_vma.Succeeded());
        vm_manager.Reprotect(target_vma.Unwrap(), perms);
        interval_target += block_size;
    }

    return ResultSuccess;
}
Result Process::Unmap(VAddr target, VAddr source, u32 size, VMAPermission perms, bool privileged) {
    LOG_DEBUG(Kernel, "Unmap memory target={:08X}, source={:08X}, size={:08X}, perms={:08X}",
              target, source, size, perms);
    if (!privileged && (source < Memory::HEAP_VADDR || source + size > Memory::HEAP_VADDR_END ||
                        source + size < source)) {
        LOG_ERROR(Kernel, "Invalid source address");
        return ResultInvalidAddress;
    }

    // TODO(wwylele): check target address range. Is it also restricted to heap region?

    if (source - target < size || target - source < size) {
        if (privileged) {
            if (source == target) {
                // privileged Unmap allows identical source and target address, which simply changes
                // the state and the permission of the memory
                return vm_manager.ChangeMemoryState(source, size, MemoryState::AliasCode,
                                                    VMAPermission::None, MemoryState::Private,
                                                    perms);
            } else {
                return ResultInvalidAddress;
            }
        } else {
            return ResultInvalidAddressState;
        }
    }

    // TODO(wwylele): check that the source and the target are actually a pair created by Map
    // Should return error 0xD8E007F5 in this case

    MemoryState source_state = privileged ? MemoryState::Locked : MemoryState::Aliased;

    R_TRY(vm_manager.UnmapRange(target, size));

    // Change back source region state. Note that the permission is reprotected according to param
    R_TRY(vm_manager.ChangeMemoryState(source, size, source_state, VMAPermission::None,
                                       MemoryState::Private, perms));

    return ResultSuccess;
}

void Process::FreeAllMemory() {
    if (memory_region == nullptr || resource_limit == nullptr) {
        return;
    }

    // Free any heap/linear memory allocations.
    for (auto& entry : holding_memory) {
        LOG_DEBUG(Kernel, "Freeing process memory region 0x{:08X} - 0x{:08X}", entry.lower(),
                  entry.upper());
        auto size = entry.upper() - entry.lower();
        memory_region->Free(entry.lower(), size);
        memory_used -= size;
        resource_limit->Release(ResourceLimitType::Commit, size);
    }
    holding_memory.clear();

    // Free any TLS memory allocations.
    auto base_memory_region = kernel.GetMemoryRegion(MemoryRegion::BASE);
    for (auto& entry : holding_tls_memory) {
        LOG_DEBUG(Kernel, "Freeing process TLS memory region 0x{:08X} - 0x{:08X}", entry.lower(),
                  entry.upper());
        auto size = entry.upper() - entry.lower();
        base_memory_region->Free(entry.lower(), size);
        memory_used -= size;
    }
    holding_tls_memory.clear();
    tls_slots.clear();

    // Diagnostics for debugging.
    // TODO: The way certain non-application shared memory is allocated can result in very slight
    // leaks in these values still.
    LOG_DEBUG(Kernel, "Remaining memory used after process cleanup: 0x{:08X}", memory_used);
    LOG_DEBUG(Kernel, "Remaining memory resource commit after process cleanup: 0x{:08X}",
              resource_limit->GetCurrentValue(ResourceLimitType::Commit));
}

Kernel::Process::Process(KernelSystem& kernel)
    : Object(kernel), handle_table(kernel), vm_manager(kernel.memory, *this), kernel(kernel) {
    kernel.memory.RegisterPageTable(vm_manager.page_table);
}
Kernel::Process::~Process() {
    LOG_INFO(Kernel, "Cleaning up process {}", process_id);

    // Release all objects this process owns first so that their potential destructor can do clean
    // up with this process before further destruction.
    // TODO(wwylele): explicitly destroy or invalidate objects this process owns (threads, shared
    // memory etc.) even if they are still referenced by other processes.
    handle_table.Clear();

    FreeAllMemory();
    kernel.memory.UnregisterPageTable(vm_manager.page_table);
}

std::shared_ptr<Process> KernelSystem::GetProcessById(u32 process_id) const {
    auto itr = std::find_if(
        process_list.begin(), process_list.end(),
        [&](const std::shared_ptr<Process>& process) { return process->process_id == process_id; });

    if (itr == process_list.end())
        return nullptr;

    return *itr;
}
} // namespace Kernel
