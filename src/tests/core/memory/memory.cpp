// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/process.h"
#include "core/memory.h"

TEST_CASE("memory.IsValidVirtualAddress", "[core][memory]") {
    Core::Timing timing(1, 100);
    Core::System system;
    Memory::MemorySystem memory{system};
    Kernel::KernelSystem kernel(
        memory, timing, [] {}, Kernel::MemoryMode::Prod, 1,
        Kernel::New3dsHwCapabilities{false, false, Kernel::New3dsMemoryMode::Legacy});
    SECTION("these regions should not be mapped on an empty process") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        CHECK(memory.IsValidVirtualAddress(*process, Memory::PROCESS_IMAGE_VADDR) == false);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::HEAP_VADDR) == false);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::LINEAR_HEAP_VADDR) == false);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::VRAM_VADDR) == false);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::CONFIG_MEMORY_VADDR) == false);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::SHARED_PAGE_VADDR) == false);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::TLS_AREA_VADDR) == false);
    }

    SECTION("CONFIG_MEMORY_VADDR and SHARED_PAGE_VADDR should be valid after mapping them") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        kernel.MapSharedPages(process->vm_manager);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::CONFIG_MEMORY_VADDR) == true);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::SHARED_PAGE_VADDR) == true);
    }

    SECTION("special regions should be valid after mapping them") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        SECTION("VRAM") {
            kernel.HandleSpecialMapping(process->vm_manager,
                                        {Memory::VRAM_VADDR, Memory::VRAM_SIZE, false, false});
            CHECK(memory.IsValidVirtualAddress(*process, Memory::VRAM_VADDR) == true);
        }

        SECTION("IO (Not yet implemented)") {
            kernel.HandleSpecialMapping(
                process->vm_manager, {Memory::IO_AREA_VADDR, Memory::IO_AREA_SIZE, false, false});
            CHECK_FALSE(memory.IsValidVirtualAddress(*process, Memory::IO_AREA_VADDR) == true);
        }
    }

    SECTION("Unmapping a VAddr should make it invalid") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        kernel.MapSharedPages(process->vm_manager);
        process->vm_manager.UnmapRange(Memory::CONFIG_MEMORY_VADDR, Memory::CONFIG_MEMORY_SIZE);
        CHECK(memory.IsValidVirtualAddress(*process, Memory::CONFIG_MEMORY_VADDR) == false);
    }
}
