// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <boost/container/static_vector.hpp>
#include <boost/serialization/export.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/vm_manager.h"

namespace Kernel {

struct AddressMapping {
    // Address and size must be page-aligned
    VAddr address;
    u32 size;
    bool read_only;
    bool unk_flag;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

union ProcessFlags {
    u16 raw;

    BitField<0, 1, u16>
        allow_debug; ///< Allows other processes to attach to and debug this process.
    BitField<1, 1, u16> force_debug; ///< Allows this process to attach to processes even if they
                                     /// don't have allow_debug set.
    BitField<2, 1, u16> allow_nonalphanum;
    BitField<3, 1, u16> shared_page_writable; ///< Shared page is mapped with write permissions.
    BitField<4, 1, u16> privileged_priority;  ///< Can use priority levels higher than 24.
    BitField<5, 1, u16> allow_main_args;
    BitField<6, 1, u16> shared_device_mem;
    BitField<7, 1, u16> runnable_on_sleep;
    BitField<8, 4, MemoryRegion>
        memory_region;                ///< Default region for memory allocations for this process
    BitField<12, 1, u16> loaded_high; ///< Application loaded high (not at 0x00100000).
};

enum class ProcessStatus { Created, Running, Exited };

class ResourceLimit;
struct MemoryRegionInfo;

class CodeSet final : public Object {
public:
    explicit CodeSet(KernelSystem& kernel);
    ~CodeSet() override;

    struct Segment {
        std::size_t offset = 0;
        VAddr addr = 0;
        u32 size = 0;

    private:
        friend class boost::serialization::access;
        template <class Archive>
        void serialize(Archive& ar, const unsigned int);
    };

    std::string GetTypeName() const override {
        return "CodeSet";
    }
    std::string GetName() const override {
        return name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::CodeSet;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    Segment& CodeSegment() {
        return segments[0];
    }

    const Segment& CodeSegment() const {
        return segments[0];
    }

    Segment& RODataSegment() {
        return segments[1];
    }

    const Segment& RODataSegment() const {
        return segments[1];
    }

    Segment& DataSegment() {
        return segments[2];
    }

    const Segment& DataSegment() const {
        return segments[2];
    }

    std::vector<u8> memory;

    std::array<Segment, 3> segments;
    VAddr entrypoint;

    /// Name of the process
    std::string name;
    /// Title ID corresponding to the process
    u64 program_id;

private:
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

class Process final : public Object {
public:
    explicit Process(Kernel::KernelSystem& kernel);
    ~Process() override;

    std::string GetTypeName() const override {
        return "Process";
    }
    std::string GetName() const override {
        return codeset->name;
    }

    static constexpr HandleType HANDLE_TYPE = HandleType::Process;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    HandleTable handle_table;

    std::shared_ptr<CodeSet> codeset;
    /// Resource limit descriptor for this process
    std::shared_ptr<ResourceLimit> resource_limit;

    /// The process may only call SVCs which have the corresponding bit set.
    std::bitset<0x80> svc_access_mask;
    /// Maximum size of the handle table for the process.
    unsigned int handle_table_size = 0x200;
    /// Special memory ranges mapped into this processes address space. This is used to give
    /// processes access to specific I/O regions and device memory.
    boost::container::static_vector<AddressMapping, 8> address_mappings;
    ProcessFlags flags;
    bool no_thread_restrictions = false;
    /// Kernel compatibility version for this process
    u16 kernel_version = 0;
    /// The default CPU for this process, threads are scheduled on this cpu by default.
    u8 ideal_processor = 0;
    /// Current status of the process
    ProcessStatus status;

    /// The id of this process
    u32 process_id;

    // Creation time in ticks of the process.
    u64 creation_time_ticks;

    /**
     * Parses a list of kernel capability descriptors (as found in the ExHeader) and applies them
     * to this process.
     */
    void ParseKernelCaps(const u32* kernel_caps, std::size_t len);

    /**
     * Set up the default kernel capability for 3DSX.
     */
    void Set3dsxKernelCaps();

    /**
     * Applies address space changes and launches the process main thread.
     */
    void Run(s32 main_thread_priority, u32 stack_size);

    /**
     * Called when the process exits by svc
     */
    void Exit();

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Memory Management

    VMManager vm_manager;

    u32 memory_used = 0;

    std::shared_ptr<MemoryRegionInfo> memory_region = nullptr;
    MemoryRegionInfo::IntervalSet holding_memory;
    MemoryRegionInfo::IntervalSet holding_tls_memory;

    /// The Thread Local Storage area is allocated as processes create threads,
    /// each TLS area is 0x200 bytes, so one page (0x1000) is split up in 8 parts, and each part
    /// holds the TLS for a specific thread. This vector contains which parts are in use for each
    /// page as a bitmask.
    /// This vector will grow as more pages are allocated for new threads.
    std::vector<std::bitset<8>> tls_slots;

    VAddr GetLinearHeapAreaAddress() const;
    VAddr GetLinearHeapBase() const;
    VAddr GetLinearHeapLimit() const;

    Result HeapAllocate(VAddr* out_addr, VAddr target, u32 size, VMAPermission perms,
                        MemoryState memory_state = MemoryState::Private,
                        bool skip_range_check = false);
    Result HeapFree(VAddr target, u32 size);

    Result LinearAllocate(VAddr* out_addr, VAddr target, u32 size, VMAPermission perms);
    Result LinearFree(VAddr target, u32 size);

    ResultVal<VAddr> AllocateThreadLocalStorage();

    Result Map(VAddr target, VAddr source, u32 size, VMAPermission perms, bool privileged = false);
    Result Unmap(VAddr target, VAddr source, u32 size, VMAPermission perms,
                 bool privileged = false);

private:
    void FreeAllMemory();

    KernelSystem& kernel;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::AddressMapping)
BOOST_CLASS_EXPORT_KEY(Kernel::CodeSet)
BOOST_CLASS_EXPORT_KEY(Kernel::CodeSet::Segment)
BOOST_CLASS_EXPORT_KEY(Kernel::Process)
CONSTRUCT_KERNEL_OBJECT(Kernel::CodeSet)
CONSTRUCT_KERNEL_OBJECT(Kernel::Process)
