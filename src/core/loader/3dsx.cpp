// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>
#include "common/literals.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/loader/3dsx.h"
#include "core/memory.h"

namespace Loader {

/*
 * File layout:
 * - File header
 * - Code, rodata and data relocation table headers
 * - Code segment
 * - Rodata segment
 * - Loadable (non-BSS) part of the data segment
 * - Code relocation table
 * - Rodata relocation table
 * - Data relocation table
 *
 * Memory layout before relocations are applied:
 * [0..codeSegSize)             -> code segment
 * [codeSegSize..rodataSegSize) -> rodata segment
 * [rodataSegSize..dataSegSize) -> data segment
 *
 * Memory layout after relocations are applied: well, however the loader sets it up :)
 * The entrypoint is always the start of the code segment.
 * The BSS section must be cleared manually by the application.
 */

enum class THREEDSX_Error : u32 {
    None = 0,
    Read = 1,
    File = 2,
    Alloc = 3,
};

static constexpr u32 MAX_RELOCATIONS = 512;
static constexpr u32 NUM_SEGMENTS = 3;

// File header
struct THREEDSX_Header {
    u32 magic;
    u16 header_size;
    u16 reloc_hdr_size;
    u32 format_ver;
    u32 flags;

    // Sizes of the code, rodata and data segments +
    // size of the BSS section (uninitialized latter half of the data segment)
    u32 code_seg_size, rodata_seg_size, data_seg_size, bss_size;
    // offset and size of smdh
    u32 smdh_offset, smdh_size;
    // offset to filesystem
    u32 fs_offset;
};
static_assert(sizeof(THREEDSX_Header) == 44);

// Relocation header: all fields (even extra unknown fields) are guaranteed to be relocation counts.
struct THREEDSX_RelocHdr {
    // # of absolute relocations (that is, fix address to post-relocation memory layout)
    u32 cross_segment_absolute;
    // # of cross-segment relative relocations (that is, 32bit signed offsets that need to be
    // patched)
    u32 cross_segment_relative;
    // more?

    // Relocations are written in this order:
    // - Absolute relocations
    // - Relative relocations
};
static_assert(sizeof(THREEDSX_RelocHdr) == 8);

// Relocation entry: from the current pointer, skip X words and patch Y words
struct THREEDSX_Reloc {
    u16 skip;
    u16 patch;
};
static_assert(sizeof(THREEDSX_Reloc) == 4);

struct PrmStruct {
    u32 magic;
    VAddr srv_override;
    u32 apt_app_id;
    u32 heap_size;
    u32 linear_heap_size;
    VAddr arg_list;
    u32 run_flags;
};
static_assert(sizeof(PrmStruct) == 28);

struct THREEloadinfo {
    u8* seg_ptrs[3]; // code, rodata & data
    u32 seg_addrs[3];
    u32 seg_sizes[3];
};

static u32 TranslateAddr(u32 addr, const THREEloadinfo* loadinfo, std::span<const u32, 2> offsets) {
    if (addr < offsets[0])
        return loadinfo->seg_addrs[0] + addr;
    if (addr < offsets[1])
        return loadinfo->seg_addrs[1] + addr - offsets[0];
    return loadinfo->seg_addrs[2] + addr - offsets[1];
}

using Kernel::CodeSet;
using namespace Common::Literals;

static THREEDSX_Error Load3DSXFile(Core::System& system, FileUtil::IOFile& file, u32 base_addr,
                                   std::shared_ptr<CodeSet>* out_codeset) {
    if (!file.IsOpen()) {
        return THREEDSX_Error::File;
    }

    // Reset read pointer in case this file has been read before.
    file.Seek(0, SEEK_SET);

    THREEDSX_Header hdr;
    if (file.ReadBytes(&hdr, sizeof(hdr)) != sizeof(hdr)) {
        return THREEDSX_Error::Read;
    }

    static constexpr size_t PageSize = Memory::CITRA_PAGE_SIZE;

    // Loadinfo segments must be a multiple of 0x1000
    THREEloadinfo loadinfo;
    loadinfo.seg_sizes[0] = Common::AlignUp(hdr.code_seg_size, PageSize);
    loadinfo.seg_sizes[1] = Common::AlignUp(hdr.rodata_seg_size, PageSize);
    loadinfo.seg_sizes[2] = Common::AlignUp(hdr.data_seg_size, PageSize);

    // Prevent integer overflow leading to heap-buffer-overflow
    if (loadinfo.seg_sizes[0] < hdr.code_seg_size || loadinfo.seg_sizes[1] < hdr.rodata_seg_size ||
        loadinfo.seg_sizes[2] < hdr.data_seg_size) {
        return THREEDSX_Error::Read;
    }

    const u32 n_reloc_tables = hdr.reloc_hdr_size / sizeof(u32);
    std::array<u32, 2> offsets = {loadinfo.seg_sizes[0],
                                  loadinfo.seg_sizes[0] + loadinfo.seg_sizes[1]};
    std::vector<u8> program_image(loadinfo.seg_sizes[0] + loadinfo.seg_sizes[1] +
                                  loadinfo.seg_sizes[2] + PageSize);

    loadinfo.seg_addrs[0] = base_addr;
    loadinfo.seg_addrs[1] = loadinfo.seg_addrs[0] + loadinfo.seg_sizes[0];
    loadinfo.seg_addrs[2] = loadinfo.seg_addrs[1] + loadinfo.seg_sizes[1];
    loadinfo.seg_ptrs[0] = program_image.data();
    loadinfo.seg_ptrs[1] = loadinfo.seg_ptrs[0] + loadinfo.seg_sizes[0];
    loadinfo.seg_ptrs[2] = loadinfo.seg_ptrs[1] + loadinfo.seg_sizes[1];

    // Skip header for future compatibility
    file.Seek(hdr.header_size, SEEK_SET);

    // Read the relocation headers
    std::vector<u32> relocs(n_reloc_tables * NUM_SEGMENTS);
    for (u32 current_segment = 0; current_segment < NUM_SEGMENTS; ++current_segment) {
        const std::size_t size = n_reloc_tables * sizeof(u32);
        if (file.ReadBytes(&relocs[current_segment * n_reloc_tables], size) != size) {
            return THREEDSX_Error::Read;
        }
    }

    // Read the segments
    if (file.ReadBytes(loadinfo.seg_ptrs[0], hdr.code_seg_size) != hdr.code_seg_size) {
        return THREEDSX_Error::Read;
    }
    if (file.ReadBytes(loadinfo.seg_ptrs[1], hdr.rodata_seg_size) != hdr.rodata_seg_size) {
        return THREEDSX_Error::Read;
    }
    if (file.ReadBytes(loadinfo.seg_ptrs[2], hdr.data_seg_size - hdr.bss_size) !=
        hdr.data_seg_size - hdr.bss_size) {
        return THREEDSX_Error::Read;
    }

    // BSS clear
    std::memset(loadinfo.seg_ptrs[2] + hdr.data_seg_size - hdr.bss_size, 0, hdr.bss_size);

    // Relocate the segments
    for (u32 current_segment = 0; current_segment < NUM_SEGMENTS; ++current_segment) {
        for (u32 current_segment_reloc_table = 0; current_segment_reloc_table < n_reloc_tables;
             current_segment_reloc_table++) {
            u32 n_relocs = relocs[current_segment * n_reloc_tables + current_segment_reloc_table];
            if (current_segment_reloc_table >= 2) {
                // We are not using this table - ignore it because we don't know what it dose
                file.Seek(n_relocs * sizeof(THREEDSX_Reloc), SEEK_CUR);
                continue;
            }
            THREEDSX_Reloc reloc_table[MAX_RELOCATIONS];

            u32* pos = (u32*)loadinfo.seg_ptrs[current_segment];
            const u32* end_pos = pos + (loadinfo.seg_sizes[current_segment] / 4);

            while (n_relocs) {
                const u32 remaining = std::min(MAX_RELOCATIONS, n_relocs);
                n_relocs -= remaining;

                if (file.ReadBytes(reloc_table, remaining * sizeof(THREEDSX_Reloc)) !=
                    remaining * sizeof(THREEDSX_Reloc)) {
                    return THREEDSX_Error::Read;
                }

                for (u32 current_inprogress = 0; current_inprogress < remaining && pos < end_pos;
                     current_inprogress++) {
                    const auto& table = reloc_table[current_inprogress];
                    LOG_TRACE(Loader, "(t={},skip={},patch={})", current_segment_reloc_table,
                              static_cast<u32>(table.skip), static_cast<u32>(table.patch));
                    pos += table.skip;
                    s32 num_patches = table.patch;
                    while (0 < num_patches && pos < end_pos) {
                        u32 in_addr = base_addr + static_cast<u32>(reinterpret_cast<u8*>(pos) -
                                                                   program_image.data());
                        u32 orig_data = *pos;
                        u32 sub_type = orig_data >> (32 - 4);
                        u32 addr = TranslateAddr(orig_data & ~0xF0000000, &loadinfo, offsets);
                        LOG_TRACE(Loader, "Patching {:08X} <-- rel({:08X},{}) ({:08X})", in_addr,
                                  addr, current_segment_reloc_table, *pos);
                        switch (current_segment_reloc_table) {
                        case 0: {
                            if (sub_type != 0) {
                                return THREEDSX_Error::Read;
                            }
                            *pos = addr;
                            break;
                        }
                        case 1: {
                            const u32 data = addr - in_addr;
                            switch (sub_type) {
                            case 0: // 32-bit signed offset
                                *pos = data;
                                break;
                            case 1: // 31-bit signed offset
                                *pos = data & ~(1U << 31);
                                break;
                            default:
                                return THREEDSX_Error::Read;
                            }
                            break;
                        }
                        default:
                            break; // this should never happen
                        }
                        pos++;
                        num_patches--;
                    }
                }
            }
        }
    }

    // Detect and fill _prm structure
    PrmStruct pst;
    std::memcpy(&pst, program_image.data() + sizeof(u32), sizeof(PrmStruct));
    if (pst.magic == MakeMagic('_', 'p', 'r', 'm')) {
        static constexpr u32 Argc = 1;
        static constexpr std::string_view Argv = "Citra";

        // Initialize the argument buffer
        const size_t extra_page_offset = program_image.size() - PageSize;
        const auto argv_buf = std::span{program_image}.subspan(extra_page_offset, PageSize);
        std::memcpy(argv_buf.data(), &Argc, sizeof(Argc));
        std::memcpy(argv_buf.data() + sizeof(Argc), Argv.data(), Argv.size());

        // Pass the arguments to the application.
        static constexpr u32 RUNFLAG_APTREINIT = 1 << 1;
        const VAddr extra_page_addr = loadinfo.seg_addrs[2] + loadinfo.seg_sizes[2];
        pst.arg_list = extra_page_addr;
        pst.run_flags |= RUNFLAG_APTREINIT;
        if (Settings::values.is_new_3ds) {
            pst.heap_size = u32(48_MiB);
            pst.linear_heap_size = u32(64_MiB);
        } else {
            pst.heap_size = u32(24_MiB);
            pst.linear_heap_size = u32(32_MiB);
        }
        std::memcpy(program_image.data() + sizeof(u32), &pst, sizeof(PrmStruct));
    }

    // Create the CodeSet
    const auto code_set = system.Kernel().CreateCodeSet("", 0);

    code_set->CodeSegment().offset = loadinfo.seg_ptrs[0] - program_image.data();
    code_set->CodeSegment().addr = loadinfo.seg_addrs[0];
    code_set->CodeSegment().size = loadinfo.seg_sizes[0];

    code_set->RODataSegment().offset = loadinfo.seg_ptrs[1] - program_image.data();
    code_set->RODataSegment().addr = loadinfo.seg_addrs[1];
    code_set->RODataSegment().size = loadinfo.seg_sizes[1];

    code_set->DataSegment().offset = loadinfo.seg_ptrs[2] - program_image.data();
    code_set->DataSegment().addr = loadinfo.seg_addrs[2];
    code_set->DataSegment().size = loadinfo.seg_sizes[2] + PageSize;

    code_set->entrypoint = code_set->CodeSegment().addr;
    code_set->memory = std::move(program_image);

    LOG_DEBUG(Loader, "code size:   {:#X}", loadinfo.seg_sizes[0]);
    LOG_DEBUG(Loader, "rodata size: {:#X}", loadinfo.seg_sizes[1]);
    LOG_DEBUG(Loader, "data size:   {:#X} (including {:#X} of bss)", loadinfo.seg_sizes[2],
              hdr.bss_size);

    *out_codeset = code_set;
    return THREEDSX_Error::None;
}

FileType AppLoader_THREEDSX::IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0, SEEK_SET);
    if (1 != file.ReadArray<u32>(&magic, 1)) {
        return FileType::Error;
    }

    if (MakeMagic('3', 'D', 'S', 'X') == magic) {
        return FileType::THREEDSX;
    }

    return FileType::Error;
}

ResultStatus AppLoader_THREEDSX::Load(std::shared_ptr<Kernel::Process>& process) {
    if (is_loaded) {
        return ResultStatus::ErrorAlreadyLoaded;
    }

    if (!file.IsOpen()) {
        return ResultStatus::Error;
    }

    std::shared_ptr<CodeSet> codeset;
    if (Load3DSXFile(system, file, Memory::PROCESS_IMAGE_VADDR, &codeset) != THREEDSX_Error::None) {
        return ResultStatus::Error;
    }
    codeset->name = filename;

    process = system.Kernel().CreateProcess(std::move(codeset));
    process->Set3dsxKernelCaps();

    // Attach the default resource limit (APPLICATION) to the process
    process->resource_limit =
        system.Kernel().ResourceLimit().GetForCategory(Kernel::ResourceLimitCategory::Application);

    // On real HW this is done with FS:Reg, but we can be lazy
    auto fs_user = system.ServiceManager().GetService<Service::FS::FS_USER>("fs:USER");
    fs_user->RegisterProgramInfo(process->GetObjectId(), process->codeset->program_id, filepath);

    process->Run(48, Kernel::DEFAULT_STACK_SIZE);

    system.ArchiveManager().RegisterSelfNCCH(*this);

    is_loaded = true;
    return ResultStatus::Success;
}

ResultStatus AppLoader_THREEDSX::ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    if (!file.IsOpen()) {
        return ResultStatus::Error;
    }

    // Reset read pointer in case this file has been read before.
    file.Seek(0, SEEK_SET);

    THREEDSX_Header hdr;
    if (file.ReadBytes(&hdr, sizeof(THREEDSX_Header)) != sizeof(THREEDSX_Header)) {
        return ResultStatus::Error;
    }

    if (hdr.header_size != sizeof(THREEDSX_Header)) {
        return ResultStatus::Error;
    }

    // Check if the 3DSX has a RomFS...
    if (hdr.fs_offset != 0) {
        u32 romfs_offset = hdr.fs_offset;
        u32 romfs_size = static_cast<u32>(file.GetSize()) - hdr.fs_offset;

        LOG_DEBUG(Loader, "RomFS offset:           {:#010X}", romfs_offset);
        LOG_DEBUG(Loader, "RomFS size:             {:#010X}", romfs_size);

        // We reopen the file, to allow its position to be independent from file's
        FileUtil::IOFile romfs_file_inner(filepath, "rb");
        if (!romfs_file_inner.IsOpen())
            return ResultStatus::Error;

        romfs_file = std::make_shared<FileSys::DirectRomFSReader>(std::move(romfs_file_inner),
                                                                  romfs_offset, romfs_size);

        return ResultStatus::Success;
    }

    LOG_DEBUG(Loader, "3DSX has no RomFS");
    return ResultStatus::ErrorNotUsed;
}

ResultStatus AppLoader_THREEDSX::ReadIcon(std::vector<u8>& buffer) {
    if (!file.IsOpen()) {
        return ResultStatus::Error;
    }

    // Reset read pointer in case this file has been read before.
    file.Seek(0, SEEK_SET);

    THREEDSX_Header hdr;
    if (file.ReadBytes(&hdr, sizeof(THREEDSX_Header)) != sizeof(THREEDSX_Header)) {
        return ResultStatus::Error;
    }

    if (hdr.header_size != sizeof(THREEDSX_Header)) {
        return ResultStatus::Error;
    }

    // Check if the 3DSX has a SMDH...
    if (hdr.smdh_offset != 0) {
        file.Seek(hdr.smdh_offset, SEEK_SET);
        buffer.resize(hdr.smdh_size);

        if (file.ReadBytes(buffer.data(), hdr.smdh_size) != hdr.smdh_size) {
            return ResultStatus::Error;
        }

        return ResultStatus::Success;
    }
    return ResultStatus::ErrorNotUsed;
}

} // namespace Loader
