// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2022 The Pixellizer Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "core/file_sys/file_backend.h"
#include "core/file_sys/plugin_3gx.h"
#include "core/file_sys/plugin_3gx_bootloader.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/loader/loader.h"

static std::string ReadTextInfo(FileUtil::IOFile& file, std::size_t offset, std::size_t max_size) {
    if (offset == 0 || max_size == 0 ||
        max_size > 0x400) { // Limit read string size to 0x400 bytes, just in case
        return "";
    }
    std::vector<char> char_data(max_size);

    const u64 prev_offset = file.Tell();
    if (!file.Seek(offset, SEEK_SET)) {
        return "";
    }
    if (file.ReadBytes(char_data.data(), max_size) != max_size) {
        file.Seek(prev_offset, SEEK_SET);
        return "";
    }
    char_data[max_size - 1] = '\0';
    return std::string(char_data.data());
}

static bool ReadSection(std::vector<u8>& data_out, FileUtil::IOFile& file, std::size_t offset,
                        std::size_t size) {
    if (size > 0x5000000) { // Limit read section size to 5MiB, just in case
        return false;
    }
    data_out.resize(size);

    const u64 prev_offset = file.Tell();

    if (!file.Seek(offset, SEEK_SET)) {
        return false;
    }
    if (file.ReadBytes(data_out.data(), size) != size) {
        file.Seek(prev_offset, SEEK_SET);
        return false;
    }
    return true;
}

Loader::ResultStatus FileSys::Plugin3GXLoader::Load(
    Service::PLGLDR::PLG_LDR::PluginLoaderContext& plg_context, Kernel::Process& process,
    Kernel::KernelSystem& kernel, Service::PLGLDR::PLG_LDR& plg_ldr) {
    FileUtil::IOFile file(plg_context.plugin_path, "rb");
    if (!file.IsOpen()) {
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. Not found: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::Error;
    }

    // Load CIA Header
    std::vector<u8> header_data(sizeof(_3gx_Header));
    if (file.ReadBytes(header_data.data(), sizeof(_3gx_Header)) != sizeof(_3gx_Header)) {
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. File corrupted: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::Error;
    }

    std::memcpy(&header, header_data.data(), sizeof(_3gx_Header));

    // Check magic value
    if (std::memcmp(&header.magic, _3GX_magic, 8) != 0) {
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. Outdated or invalid 3GX plugin: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::Error;
    }

    if (header.infos.flags.compatibility == static_cast<u32>(_3gx_Infos::Compatibility::CONSOLE)) {
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. Not compatible with Lime3DS: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::Error;
    }

    // Load strings
    author = ReadTextInfo(file, header.infos.author_msg_offset, header.infos.author_len);
    title = ReadTextInfo(file, header.infos.title_msg_offset, header.infos.title_len);
    description =
        ReadTextInfo(file, header.infos.description_msg_offset, header.infos.description_len);
    summary = ReadTextInfo(file, header.infos.summary_msg_offset, header.infos.summary_len);

    LOG_INFO(Service_PLGLDR, "Trying to load plugin - Title: {} - Author: {}", title, author);

    // Load compatible TIDs
    {
        std::vector<u8> raw_TID_data;
        if (!ReadSection(raw_TID_data, file, header.targets.title_offsets,
                         header.targets.count * sizeof(u32))) {
            return Loader::ResultStatus::Error;
        }
        compatible_TID.reserve(header.targets.count); // compatible_TID should be empty right now
        for (u32 i = 0; i < u32(header.targets.count); i++) {
            compatible_TID.push_back(
                u32_le(*reinterpret_cast<u32*>(raw_TID_data.data() + i * sizeof(u32))));
        }
    }

    if (!compatible_TID.empty() &&
        std::find(compatible_TID.begin(), compatible_TID.end(),
                  static_cast<u32>(process.codeset->program_id)) == compatible_TID.end()) {
        LOG_ERROR(Service_PLGLDR,
                  "Failed to load 3GX plugin. Not compatible with loaded process: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::Error;
    }

    // Load exe load func and args
    if (header.infos.flags.embedded_exe_func.Value() &&
        header.executable.exe_load_func_offset != 0) {
        exe_load_func.clear();
        std::vector<u8> out;
        for (int i = 0; i < 32; i++) {
            ReadSection(out, file, header.executable.exe_load_func_offset + i * sizeof(u32),
                        sizeof(u32));
            u32 instruction = *reinterpret_cast<u32_le*>(out.data());
            if (instruction == 0xE320F000) {
                break;
            }
            exe_load_func.push_back(instruction);
        }
        std::memcpy(exe_load_args, header.infos.builtin_load_exe_args,
                    sizeof(_3gx_Infos::builtin_load_exe_args));
    }

    // Load code sections
    if (!ReadSection(text_section, file, header.executable.code_offset,
                     header.executable.code_size) ||
        !ReadSection(rodata_section, file, header.executable.rodata_offset,
                     header.executable.rodata_size) ||
        !ReadSection(data_section, file, header.executable.data_offset,
                     header.executable.data_size)) {
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. File corrupted: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::Error;
    }

    return Map(plg_context, process, kernel, plg_ldr);
}

Loader::ResultStatus FileSys::Plugin3GXLoader::Map(
    Service::PLGLDR::PLG_LDR::PluginLoaderContext& plg_context, Kernel::Process& process,
    Kernel::KernelSystem& kernel, Service::PLGLDR::PLG_LDR& plg_ldr) {

    // Verify exe load checksum function is available
    if (exe_load_func.empty() && plg_context.load_exe_func.empty()) {
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. Missing checksum function: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::Error;
    }

    const std::array<u32, 4> mem_region_sizes = {
        5 * 1024 * 1024, // 5 MiB
        2 * 1024 * 1024, // 2 MiB
        3 * 1024 * 1024, // 3 MiB
        4 * 1024 * 1024  // 4 MiB
    };

    // Map memory block. This behaviour mimics how plugins are loaded on 3DS as much as possible.
    // Calculate the sizes of the different memory regions
    const u32 block_size = mem_region_sizes[header.infos.flags.memory_region_size.Value()];
    const u32 exe_size = (sizeof(PluginHeader) + text_section.size() + rodata_section.size() +
                          data_section.size() + header.executable.bss_size + 0x1000) &
                         ~0xFFFu;

    // Allocate the framebuffer block so that is in the highest FCRAM position possible
    auto offset_fb =
        kernel.GetMemoryRegion(Kernel::MemoryRegion::SYSTEM)->RLinearAllocate(_3GX_fb_size);
    if (!offset_fb) {
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. Not enough memory: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::ErrorMemoryAllocationFailed;
    }
    auto backing_memory_fb = kernel.memory.GetFCRAMRef(*offset_fb);
    plg_ldr.SetPluginFBAddr(Memory::FCRAM_PADDR + *offset_fb);
    std::fill(backing_memory_fb.GetPtr(), backing_memory_fb.GetPtr() + _3GX_fb_size, 0);

    auto vma_heap_fb = process.vm_manager.MapBackingMemory(
        _3GX_heap_load_addr, backing_memory_fb, _3GX_fb_size, Kernel::MemoryState::Continuous);
    ASSERT(vma_heap_fb.Succeeded());
    process.vm_manager.Reprotect(vma_heap_fb.Unwrap(), Kernel::VMAPermission::ReadWrite);

    // Allocate a block from the end of FCRAM and clear it
    auto offset = kernel.GetMemoryRegion(Kernel::MemoryRegion::SYSTEM)
                      ->RLinearAllocate(block_size - _3GX_fb_size);
    if (!offset) {
        kernel.GetMemoryRegion(Kernel::MemoryRegion::SYSTEM)->Free(*offset_fb, _3GX_fb_size);
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. Not enough memory: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::ErrorMemoryAllocationFailed;
    }
    auto backing_memory = kernel.memory.GetFCRAMRef(*offset);
    std::fill(backing_memory.GetPtr(), backing_memory.GetPtr() + block_size - _3GX_fb_size, 0);

    // Then we map part of the memory, which contains the executable
    auto vma = process.vm_manager.MapBackingMemory(_3GX_exe_load_addr, backing_memory, exe_size,
                                                   Kernel::MemoryState::Continuous);
    ASSERT(vma.Succeeded());
    process.vm_manager.Reprotect(vma.Unwrap(), Kernel::VMAPermission::ReadWriteExecute);

    // Write text section
    kernel.memory.WriteBlock(process, _3GX_exe_load_addr + sizeof(PluginHeader),
                             text_section.data(), header.executable.code_size);
    // Write rodata section
    kernel.memory.WriteBlock(
        process, _3GX_exe_load_addr + sizeof(PluginHeader) + header.executable.code_size,
        rodata_section.data(), header.executable.rodata_size);
    // Write data section
    kernel.memory.WriteBlock(process,
                             _3GX_exe_load_addr + sizeof(PluginHeader) +
                                 header.executable.code_size + header.executable.rodata_size,
                             data_section.data(), header.executable.data_size);
    // Prepare plugin header and write it
    PluginHeader plugin_header = {0};
    plugin_header.version = header.version;
    plugin_header.exe_size = exe_size;
    plugin_header.heap_VA = _3GX_heap_load_addr;
    plugin_header.heap_size = block_size - exe_size;
    plg_context.plg_event = _3GX_exe_load_addr - 0x4;
    plg_context.plg_reply = _3GX_exe_load_addr - 0x8;
    plugin_header.plgldr_event = plg_context.plg_event;
    plugin_header.plgldr_reply = plg_context.plg_reply;
    plugin_header.is_default_plugin = plg_context.is_default_path;
    if (plg_context.use_user_load_parameters) {
        std::memcpy(plugin_header.config, plg_context.user_load_parameters.config,
                    sizeof(PluginHeader::config));
    }
    kernel.memory.WriteBlock(process, _3GX_exe_load_addr, &plugin_header, sizeof(PluginHeader));

    // Map plugin heap
    auto backing_memory_heap = kernel.memory.GetFCRAMRef(*offset + exe_size);

    // Map the rest of the memory at the heap location
    auto vma_heap = process.vm_manager.MapBackingMemory(
        _3GX_heap_load_addr + _3GX_fb_size, backing_memory_heap,
        block_size - exe_size - _3GX_fb_size, Kernel::MemoryState::Continuous);
    ASSERT(vma_heap.Succeeded());
    process.vm_manager.Reprotect(vma_heap.Unwrap(), Kernel::VMAPermission::ReadWriteExecute);

    // Allocate a block from the end of FCRAM and clear it
    auto bootloader_offset = kernel.GetMemoryRegion(Kernel::MemoryRegion::SYSTEM)
                                 ->RLinearAllocate(bootloader_memory_size);
    if (!bootloader_offset) {
        kernel.GetMemoryRegion(Kernel::MemoryRegion::SYSTEM)->Free(*offset_fb, _3GX_fb_size);
        kernel.GetMemoryRegion(Kernel::MemoryRegion::SYSTEM)
            ->Free(*offset, block_size - _3GX_fb_size);
        LOG_ERROR(Service_PLGLDR, "Failed to load 3GX plugin. Not enough memory: {}",
                  plg_context.plugin_path);
        return Loader::ResultStatus::ErrorMemoryAllocationFailed;
    }
    const bool use_internal = plg_context.load_exe_func.empty();
    MapBootloader(
        process, kernel, *bootloader_offset,
        (use_internal) ? exe_load_func : plg_context.load_exe_func,
        (use_internal) ? exe_load_args : plg_context.load_exe_args,
        header.executable.code_size + header.executable.rodata_size + header.executable.data_size,
        header.infos.exe_load_checksum,
        plg_context.use_user_load_parameters ? plg_context.user_load_parameters.no_flash : 0);

    plg_context.plugin_loaded = true;
    plg_context.use_user_load_parameters = false;
    return Loader::ResultStatus::Success;
}

void FileSys::Plugin3GXLoader::MapBootloader(Kernel::Process& process, Kernel::KernelSystem& kernel,
                                             u32 memory_offset, std::span<const u32> exe_load_func,
                                             const u32_le* exe_load_args, u32 checksum_size,
                                             u32 exe_checksum, bool no_flash) {

    u32_le game_instructions[2];
    kernel.memory.ReadBlock(process, process.codeset->CodeSegment().addr, game_instructions,
                            sizeof(u32) * 2);

    std::array<u32_le, g_plugin_loader_bootloader.size() / sizeof(u32)> bootloader;
    std::memcpy(bootloader.data(), g_plugin_loader_bootloader.data(),
                g_plugin_loader_bootloader.size());

    for (auto it = bootloader.begin(); it < bootloader.end(); it++) {
        switch (static_cast<u32>(*it)) {
        case 0xDEAD0000: {
            *it = game_instructions[0];
        } break;
        case 0xDEAD0001: {
            *it = game_instructions[1];
        } break;
        case 0xDEAD0002: {
            *it = process.codeset->CodeSegment().addr;
        } break;
        case 0xDEAD0003: {
            for (u32 i = 0;
                 i <
                 sizeof(Service::PLGLDR::PLG_LDR::PluginLoaderContext::load_exe_args) / sizeof(u32);
                 i++) {
                bootloader[i + (it - bootloader.begin())] = exe_load_args[i];
            }
        } break;
        case 0xDEAD0004: {
            *it = _3GX_exe_load_addr + sizeof(PluginHeader);
        } break;
        case 0xDEAD0005: {
            *it = _3GX_exe_load_addr + sizeof(PluginHeader) + checksum_size;
        } break;
        case 0xDEAD0006: {
            *it = exe_checksum;
        } break;
        case 0xDEAD0007: {
            *it = _3GX_exe_load_addr - 0xC;
        } break;
        case 0xDEAD0008: {
            *it = _3GX_exe_load_addr + sizeof(PluginHeader);
        } break;
        case 0xDEAD0009: {
            *it = no_flash ? 1 : 0;
        } break;
        case 0xDEAD000A: {
            for (u32 i = 0; i < exe_load_func.size(); i++) {
                bootloader[i + (it - bootloader.begin())] = exe_load_func[i];
            }
        } break;
        default:
            break;
        }
    }

    // Map bootloader to the offset provided
    auto backing_memory = kernel.memory.GetFCRAMRef(memory_offset);
    std::fill(backing_memory.GetPtr(), backing_memory.GetPtr() + bootloader_memory_size, 0);
    auto vma = process.vm_manager.MapBackingMemory(_3GX_exe_load_addr - bootloader_memory_size,
                                                   backing_memory, bootloader_memory_size,
                                                   Kernel::MemoryState::Continuous);
    ASSERT(vma.Succeeded());
    process.vm_manager.Reprotect(vma.Unwrap(), Kernel::VMAPermission::ReadWriteExecute);

    // Write bootloader
    kernel.memory.WriteBlock(
        process, _3GX_exe_load_addr - bootloader_memory_size, bootloader.data(),
        std::min<std::size_t>(bootloader.size() * sizeof(u32), bootloader_memory_size));

    game_instructions[0] = 0xE51FF004; // ldr pc, [pc, #-4]
    game_instructions[1] = _3GX_exe_load_addr - bootloader_memory_size;
    kernel.memory.WriteBlock(process, process.codeset->CodeSegment().addr, game_instructions,
                             sizeof(u32) * 2);
}
