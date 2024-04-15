// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>
#include "common/common_types.h"
#include "common/memory_ref.h"

namespace Kernel {
class Process;
}

namespace Core {
class System;
}

namespace AudioCore {
class DspInterface;
}

namespace Memory {

/**
 * Page size used by the ARM architecture. This is the smallest granularity with which memory can
 * be mapped.
 */
constexpr u32 CITRA_PAGE_SIZE = 0x1000;
constexpr u32 CITRA_PAGE_MASK = CITRA_PAGE_SIZE - 1;
constexpr int CITRA_PAGE_BITS = 12;
constexpr std::size_t PAGE_TABLE_NUM_ENTRIES = 1 << (32 - CITRA_PAGE_BITS);

enum class PageType {
    /// Page is unmapped and should cause an access error.
    Unmapped,
    /// Page is mapped to regular memory. This is the only type you can get pointers to.
    Memory,
    /// Page is mapped to regular memory, but also needs to check for rasterizer cache flushing and
    /// invalidation
    RasterizerCachedMemory,
};

/**
 * A (reasonably) fast way of allowing switchable and remappable process address spaces. It loosely
 * mimics the way a real CPU page table works, but instead is optimized for minimal decoding and
 * fetching requirements when accessing. In the usual case of an access to regular memory, it only
 * requires an indexed fetch and a check for NULL.
 */
struct PageTable {
    /**
     * Array of memory pointers backing each page. An entry can only be non-null if the
     * corresponding entry in the `attributes` array is of type `Memory`.
     */

    // The reason for this rigmarole is to keep the 'raw' and 'refs' arrays in sync.
    // We need 'raw' for dynarmic and 'refs' for serialization
    struct Pointers {

        struct Entry {
            Entry(Pointers& pointers_, VAddr idx_) : pointers(pointers_), idx(idx_) {}

            Entry& operator=(MemoryRef value) {
                pointers.raw[idx] = value.GetPtr();
                pointers.refs[idx] = std::move(value);
                return *this;
            }

            operator u8*() {
                return pointers.raw[idx];
            }

        private:
            Pointers& pointers;
            VAddr idx;
        };

        Entry operator[](std::size_t idx) {
            return Entry(*this, static_cast<VAddr>(idx));
        }

    private:
        std::array<u8*, PAGE_TABLE_NUM_ENTRIES> raw;
        std::array<MemoryRef, PAGE_TABLE_NUM_ENTRIES> refs;
        friend struct PageTable;
    };

    Pointers pointers;

    /**
     * Array of fine grained page attributes. If it is set to any value other than `Memory`, then
     * the corresponding entry in `pointers` MUST be set to null.
     */
    std::array<PageType, PAGE_TABLE_NUM_ENTRIES> attributes;

    std::array<u8*, PAGE_TABLE_NUM_ENTRIES>& GetPointerArray() {
        return pointers.raw;
    }

    void Clear();

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & pointers.refs;
        ar & attributes;
        for (std::size_t i = 0; i < PAGE_TABLE_NUM_ENTRIES; i++) {
            pointers.raw[i] = pointers.refs[i].GetPtr();
        }
    }
    friend class boost::serialization::access;
};

/// Physical memory regions as seen from the ARM11
enum : PAddr {
    /// IO register area
    IO_AREA_PADDR = 0x10100000,
    IO_AREA_SIZE = 0x00400000, ///< IO area size (4MB)
    IO_AREA_PADDR_END = IO_AREA_PADDR + IO_AREA_SIZE,

    /// MPCore internal memory region
    MPCORE_RAM_PADDR = 0x17E00000,
    MPCORE_RAM_SIZE = 0x00002000, ///< MPCore internal memory size (8KB)
    MPCORE_RAM_PADDR_END = MPCORE_RAM_PADDR + MPCORE_RAM_SIZE,

    /// Video memory
    VRAM_PADDR = 0x18000000,
    VRAM_SIZE = 0x00600000, ///< VRAM size (6MB)
    VRAM_PADDR_END = VRAM_PADDR + VRAM_SIZE,

    /// New 3DS additional memory. Supposedly faster than regular FCRAM. Part of it can be used by
    /// applications and system modules if mapped via the ExHeader.
    N3DS_EXTRA_RAM_PADDR = 0x1F000000,
    N3DS_EXTRA_RAM_SIZE = 0x00400000, ///< New 3DS additional memory size (4MB)
    N3DS_EXTRA_RAM_PADDR_END = N3DS_EXTRA_RAM_PADDR + N3DS_EXTRA_RAM_SIZE,

    /// DSP memory
    DSP_RAM_PADDR = 0x1FF00000,
    DSP_RAM_SIZE = 0x00080000, ///< DSP memory size (512KB)
    DSP_RAM_PADDR_END = DSP_RAM_PADDR + DSP_RAM_SIZE,

    /// AXI WRAM
    AXI_WRAM_PADDR = 0x1FF80000,
    AXI_WRAM_SIZE = 0x00080000, ///< AXI WRAM size (512KB)
    AXI_WRAM_PADDR_END = AXI_WRAM_PADDR + AXI_WRAM_SIZE,

    /// Main FCRAM
    FCRAM_PADDR = 0x20000000,
    FCRAM_SIZE = 0x08000000,      ///< FCRAM size on the Old 3DS (128MB)
    FCRAM_N3DS_SIZE = 0x10000000, ///< FCRAM size on the New 3DS (256MB)
    FCRAM_PADDR_END = FCRAM_PADDR + FCRAM_SIZE,
    FCRAM_N3DS_PADDR_END = FCRAM_PADDR + FCRAM_N3DS_SIZE,
};

enum class Region { FCRAM, VRAM, DSP, N3DS };

/// Virtual user-space memory regions
enum : VAddr {
    /// Where the application text, data and bss reside.
    PROCESS_IMAGE_VADDR = 0x00100000,
    PROCESS_IMAGE_MAX_SIZE = 0x03F00000,
    PROCESS_IMAGE_VADDR_END = PROCESS_IMAGE_VADDR + PROCESS_IMAGE_MAX_SIZE,

    /// Area where IPC buffers are mapped onto.
    IPC_MAPPING_VADDR = 0x04000000,
    IPC_MAPPING_SIZE = 0x04000000,
    IPC_MAPPING_VADDR_END = IPC_MAPPING_VADDR + IPC_MAPPING_SIZE,

    /// Application heap (includes stack).
    HEAP_VADDR = 0x08000000,
    HEAP_SIZE = 0x08000000,
    HEAP_VADDR_END = HEAP_VADDR + HEAP_SIZE,

    /// Area where shared memory buffers are mapped onto.
    SHARED_MEMORY_VADDR = 0x10000000,
    SHARED_MEMORY_SIZE = 0x04000000,
    SHARED_MEMORY_VADDR_END = SHARED_MEMORY_VADDR + SHARED_MEMORY_SIZE,

    /// Maps 1:1 to an offset in FCRAM. Used for HW allocations that need to be linear in physical
    /// memory.
    LINEAR_HEAP_VADDR = 0x14000000,
    LINEAR_HEAP_SIZE = 0x08000000,
    LINEAR_HEAP_VADDR_END = LINEAR_HEAP_VADDR + LINEAR_HEAP_SIZE,

    /// Maps 1:1 to New 3DS additional memory
    N3DS_EXTRA_RAM_VADDR = 0x1E800000,
    N3DS_EXTRA_RAM_VADDR_END = N3DS_EXTRA_RAM_VADDR + N3DS_EXTRA_RAM_SIZE,

    /// Maps 1:1 to the IO register area.
    IO_AREA_VADDR = 0x1EC00000,
    IO_AREA_VADDR_END = IO_AREA_VADDR + IO_AREA_SIZE,

    /// Maps 1:1 to VRAM.
    VRAM_VADDR = 0x1F000000,
    VRAM_VADDR_END = VRAM_VADDR + VRAM_SIZE,

    /// Maps 1:1 to DSP memory.
    DSP_RAM_VADDR = 0x1FF00000,
    DSP_RAM_VADDR_END = DSP_RAM_VADDR + DSP_RAM_SIZE,

    /// Read-only page containing kernel and system configuration values.
    CONFIG_MEMORY_VADDR = 0x1FF80000,
    CONFIG_MEMORY_SIZE = 0x00001000,
    CONFIG_MEMORY_VADDR_END = CONFIG_MEMORY_VADDR + CONFIG_MEMORY_SIZE,

    /// Usually read-only page containing mostly values read from hardware.
    SHARED_PAGE_VADDR = 0x1FF81000,
    SHARED_PAGE_SIZE = 0x00001000,
    SHARED_PAGE_VADDR_END = SHARED_PAGE_VADDR + SHARED_PAGE_SIZE,

    /// Area where TLS (Thread-Local Storage) buffers are allocated.
    TLS_AREA_VADDR = 0x1FF82000,
    TLS_ENTRY_SIZE = 0x200,

    /// Equivalent to LINEAR_HEAP_VADDR, but expanded to cover the extra memory in the New 3DS.
    NEW_LINEAR_HEAP_VADDR = 0x30000000,
    NEW_LINEAR_HEAP_SIZE = 0x10000000,
    NEW_LINEAR_HEAP_VADDR_END = NEW_LINEAR_HEAP_VADDR + NEW_LINEAR_HEAP_SIZE,

    /// Area where 3GX plugin framebuffers are stored
    PLUGIN_3GX_FB_VADDR = 0x06000000,
    PLUGIN_3GX_FB_SIZE = 0x000A9000,
    PLUGIN_3GX_FB_VADDR_END = PLUGIN_3GX_FB_VADDR + PLUGIN_3GX_FB_SIZE
};

enum class FlushMode {
    /// Write back modified surfaces to RAM
    Flush,
    /// Remove region from the cache
    Invalidate,
    /// Write back modified surfaces to RAM, and also remove them from the cache
    FlushAndInvalidate,
};

class MemorySystem {
public:
    explicit MemorySystem(Core::System& system);
    ~MemorySystem();

    /**
     * Maps an allocated buffer onto a region of the emulated process address space.
     *
     * @param page_table The page table of the emulated process.
     * @param base The address to start mapping at. Must be page-aligned.
     * @param size The amount of bytes to map. Must be page-aligned.
     * @param target Buffer with the memory backing the mapping. Must be of length at least `size`.
     */
    void MapMemoryRegion(PageTable& page_table, VAddr base, u32 size, MemoryRef target);

    void UnmapRegion(PageTable& page_table, VAddr base, u32 size);

    /// Currently active page table
    void SetCurrentPageTable(std::shared_ptr<PageTable> page_table);
    std::shared_ptr<PageTable> GetCurrentPageTable() const;

    /**
     * Gets a pointer to the given address.
     *
     * @param vaddr Virtual address to retrieve a pointer to.
     *
     * @returns The pointer to the given address, if the address is valid.
     *          If the address is not valid, nullptr will be returned.
     */
    u8* GetPointer(VAddr vaddr);

    /**
     * Gets a pointer to the given address.
     *
     * @param vaddr Virtual address to retrieve a pointer to.
     *
     * @returns The pointer to the given address, if the address is valid.
     *          If the address is not valid, nullptr will be returned.
     */
    const u8* GetPointer(VAddr vaddr) const;

    /**
     * Reads an 8-bit unsigned value from the current process' address space
     * at the given virtual address.
     *
     * @param addr The virtual address to read the 8-bit value from.
     *
     * @returns the read 8-bit unsigned value.
     */
    u8 Read8(VAddr addr);

    /**
     * Reads a 16-bit unsigned value from the current process' address space
     * at the given virtual address.
     *
     * @param addr The virtual address to read the 16-bit value from.
     *
     * @returns the read 16-bit unsigned value.
     */
    u16 Read16(VAddr addr);

    /**
     * Reads a 32-bit unsigned value from the current process' address space
     * at the given virtual address.
     *
     * @param addr The virtual address to read the 32-bit value from.
     *
     * @returns the read 32-bit unsigned value.
     */
    u32 Read32(VAddr addr);

    /**
     * Reads a 64-bit unsigned value from the current process' address space
     * at the given virtual address.
     *
     * @param addr The virtual address to read the 64-bit value from.
     *
     * @returns the read 64-bit value.
     */
    u64 Read64(VAddr addr);

    /**
     * Writes an 8-bit unsigned integer to the given virtual address in
     * the current process' address space.
     *
     * @param addr The virtual address to write the 8-bit unsigned integer to.
     * @param data The 8-bit unsigned integer to write to the given virtual address.
     *
     * @post The memory at the given virtual address contains the specified data value.
     */
    void Write8(VAddr addr, u8 data);

    /**
     * Writes a 16-bit unsigned integer to the given virtual address in
     * the current process' address space.
     *
     * @param addr The virtual address to write the 16-bit unsigned integer to.
     * @param data The 16-bit unsigned integer to write to the given virtual address.
     *
     * @post The memory range [addr, sizeof(data)) contains the given data value.
     */
    void Write16(VAddr addr, u16 data);

    /**
     * Writes a 32-bit unsigned integer to the given virtual address in
     * the current process' address space.
     *
     * @param addr The virtual address to write the 32-bit unsigned integer to.
     * @param data The 32-bit unsigned integer to write to the given virtual address.
     *
     * @post The memory range [addr, sizeof(data)) contains the given data value.
     */
    void Write32(VAddr addr, u32 data);

    /**
     * Writes a 64-bit unsigned integer to the given virtual address in
     * the current process' address space.
     *
     * @param addr The virtual address to write the 64-bit unsigned integer to.
     * @param data The 64-bit unsigned integer to write to the given virtual address.
     *
     * @post The memory range [addr, sizeof(data)) contains the given data value.
     */
    void Write64(VAddr addr, u64 data);

    /**
     * Writes a {8, 16, 32, 64}-bit unsigned integer to the given virtual address in
     * the current process' address space if and only if the address contains
     * the expected value. This operation is atomic.
     *
     * @param addr The virtual address to write the X-bit unsigned integer to.
     * @param data The X-bit unsigned integer to write to the given virtual address.
     * @param expected The X-bit unsigned integer to check against the given virtual address.
     * @returns true if the operation failed
     *
     * @post The memory range [addr, sizeof(data)) contains the given data value.
     */
    bool WriteExclusive8(const VAddr addr, const u8 data, const u8 expected);
    bool WriteExclusive16(const VAddr addr, const u16 data, const u16 expected);
    bool WriteExclusive32(const VAddr addr, const u32 data, const u32 expected);
    bool WriteExclusive64(const VAddr addr, const u64 data, const u64 expected);

    /**
     * Reads a null-terminated string from the given virtual address.
     * This function will continually read characters until either:
     *
     * - A null character ('\0') is reached.
     * - max_length characters have been read.
     *
     * @note The final null-terminating character (if found) is not included
     *       in the returned string.
     *
     * @param vaddr      The address to begin reading the string from.
     * @param max_length The maximum length of the string to read in characters.
     *
     * @returns The read string.
     */
    std::string ReadCString(VAddr vaddr, std::size_t max_length);

    /**
     * Reads a contiguous block of bytes from a specified process' address space.
     *
     * @param process     The process to read the data from.
     * @param src_addr    The virtual address to begin reading from.
     * @param dest_buffer The buffer to place the read bytes into.
     * @param size        The amount of data to read, in bytes.
     *
     * @note If a size of 0 is specified, then this function reads nothing and
     *       no attempts to access memory are made at all.
     *
     * @pre dest_buffer must be at least size bytes in length, otherwise a
     *      buffer overrun will occur.
     *
     * @post The range [dest_buffer, size) contains the read bytes from the
     *       process' address space.
     */
    void ReadBlock(const Kernel::Process& process, VAddr src_addr, void* dest_buffer,
                   std::size_t size);

    /**
     * Reads a contiguous block of bytes from the current process' address space.
     *
     * @param src_addr    The virtual address to begin reading from.
     * @param dest_buffer The buffer to place the read bytes into.
     * @param size        The amount of data to read, in bytes.
     *
     * @note If a size of 0 is specified, then this function reads nothing and
     *       no attempts to access memory are made at all.
     *
     * @pre dest_buffer must be at least size bytes in length, otherwise a
     *      buffer overrun will occur.
     *
     * @post The range [dest_buffer, size) contains the read bytes from the
     *       current process' address space.
     */
    void ReadBlock(VAddr src_addr, void* dest_buffer, std::size_t size);

    /**
     * Writes a range of bytes into a given process' address space at the specified
     * virtual address.
     *
     * @param process    The process to write data into the address space of.
     * @param dest_addr  The destination virtual address to begin writing the data at.
     * @param src_buffer The data to write into the process' address space.
     * @param size       The size of the data to write, in bytes.
     *
     * @post The address range [dest_addr, size) in the process' address space
     *       contains the data that was within src_buffer.
     *
     * @post If an attempt is made to write into an unmapped region of memory, the writes
     *       will be ignored and an error will be logged.
     *
     * @post If a write is performed into a region of memory that is considered cached
     *       rasterizer memory, will cause the currently active rasterizer to be notified
     *       and will mark that region as invalidated to caches that the active
     *       graphics backend may be maintaining over the course of execution.
     */
    void WriteBlock(const Kernel::Process& process, VAddr dest_addr, const void* src_buffer,
                    std::size_t size);

    /**
     * Writes a range of bytes into a given process' address space at the specified
     * virtual address.
     *
     * @param dest_addr  The destination virtual address to begin writing the data at.
     * @param src_buffer The data to write into the process' address space.
     * @param size       The size of the data to write, in bytes.
     *
     * @post The address range [dest_addr, size) in the process' address space
     *       contains the data that was within src_buffer.
     *
     * @post If an attempt is made to write into an unmapped region of memory, the writes
     *       will be ignored and an error will be logged.
     *
     * @post If a write is performed into a region of memory that is considered cached
     *       rasterizer memory, will cause the currently active rasterizer to be notified
     *       and will mark that region as invalidated to caches that the active
     *       graphics backend may be maintaining over the course of execution.
     */
    void WriteBlock(VAddr dest_addr, const void* src_buffer, std::size_t size);

    /**
     * Zeros a range of bytes within the current process' address space at the specified
     * virtual address.
     *
     * @param process   The process that will have data zeroed within its address space.
     * @param dest_addr The destination virtual address to zero the data from.
     * @param size      The size of the range to zero out, in bytes.
     *
     * @post The range [dest_addr, size) within the process' address space contains the
     *       value 0.
     */
    void ZeroBlock(const Kernel::Process& process, VAddr dest_addr, const std::size_t size);

    /**
     * Copies data within a process' address space to another location within the
     * same address space.
     *
     * @param process   The process that will have data copied within its address space.
     * @param dest_addr The destination virtual address to begin copying the data into.
     * @param src_addr  The source virtual address to begin copying the data from.
     * @param size      The size of the data to copy, in bytes.
     *
     * @post The range [dest_addr, size) within the process' address space contains the
     *       same data within the range [src_addr, size).
     */
    void CopyBlock(const Kernel::Process& process, VAddr dest_addr, VAddr src_addr,
                   std::size_t size);
    void CopyBlock(const Kernel::Process& dest_process, const Kernel::Process& src_process,
                   VAddr dest_addr, VAddr src_addr, std::size_t size);

    /**
     * Marks each page within the specified address range as cached or uncached.
     *
     * @param vaddr  The virtual address indicating the start of the address range.
     * @param size   The size of the address range in bytes.
     * @param cached Whether or not any pages within the address range should be
     *               marked as cached or uncached.
     */
    void RasterizerMarkRegionCached(PAddr start, u32 size, bool cached);

    /// For a rasterizer-accessible PAddr, gets a list of all possible VAddr
    std::vector<VAddr> PhysicalToVirtualAddressForRasterizer(PAddr addr);

    /// Gets a pointer to the memory region beginning at the specified physical address.
    u8* GetPhysicalPointer(PAddr address) const;

    /// Returns a reference to the memory region beginning at the specified physical address
    MemoryRef GetPhysicalRef(PAddr address) const;

    /// Determines if the given VAddr is valid for the specified process.
    bool IsValidVirtualAddress(const Kernel::Process& process, VAddr vaddr);

    /// Returns true if the address refers to a valid memory region
    bool IsValidPhysicalAddress(PAddr paddr) const;

    /// Gets offset in FCRAM from a pointer inside FCRAM range
    u32 GetFCRAMOffset(const u8* pointer) const;

    /// Gets pointer in FCRAM with given offset
    u8* GetFCRAMPointer(std::size_t offset);

    /// Gets pointer in FCRAM with given offset
    const u8* GetFCRAMPointer(std::size_t offset) const;

    /// Gets a serializable ref to FCRAM with the given offset
    MemoryRef GetFCRAMRef(std::size_t offset) const;

    /// Registers page table for rasterizer cache marking
    void RegisterPageTable(std::shared_ptr<PageTable> page_table);

    /// Unregisters page table for rasterizer cache marking
    void UnregisterPageTable(std::shared_ptr<PageTable> page_table);

    void SetDSP(AudioCore::DspInterface& dsp);

    void RasterizerFlushVirtualRegion(VAddr start, u32 size, FlushMode mode);

private:
    template <typename T>
    T Read(const VAddr vaddr);

    template <typename T>
    void Write(const VAddr vaddr, const T data);

    template <typename T>
    bool WriteExclusive(const VAddr vaddr, const T data, const T expected);

    /**
     * Gets the pointer for virtual memory where the page is marked as RasterizerCachedMemory.
     * This is used to access the memory where the page pointer is nullptr due to rasterizer cache.
     * Since the cache only happens on linear heap or VRAM, we know the exact physical address and
     * pointer of such virtual address
     */
    MemoryRef GetPointerForRasterizerCache(VAddr addr) const;

    void MapPages(PageTable& page_table, u32 base, u32 size, MemoryRef memory, PageType type);

private:
    class Impl;
    std::unique_ptr<Impl> impl;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version);

public:
    template <Region R>
    class BackingMemImpl;
};

} // namespace Memory

BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::FCRAM>)
BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::VRAM>)
BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::DSP>)
BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::N3DS>)
