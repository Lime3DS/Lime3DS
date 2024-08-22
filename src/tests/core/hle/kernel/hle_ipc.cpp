// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

static std::shared_ptr<Object> MakeObject(Kernel::KernelSystem& kernel) {
    return kernel.CreateEvent(ResetType::OneShot);
}

TEST_CASE("HLERequestContext::PopulateFromIncomingCommandBuffer", "[core][kernel]") {
    Core::Timing timing(1, 100);
    Core::System system;
    Memory::MemorySystem memory{system};
    Kernel::KernelSystem kernel(
        memory, timing, [] {}, Kernel::MemoryMode::Prod, 1,
        Kernel::New3dsHwCapabilities{false, false, Kernel::New3dsMemoryMode::Legacy});
    auto [server, client] = kernel.CreateSessionPair();
    HLERequestContext context(kernel, std::move(server), nullptr);

    auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));

    SECTION("works with empty cmdbuf") {
        const u32_le input[]{
            IPC::MakeHeader(0x1234, 0, 0),
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        REQUIRE(context.CommandBuffer()[0] == 0x12340000);
    }

    SECTION("translates regular params") {
        const u32_le input[]{
            IPC::MakeHeader(0, 3, 0),
            0x12345678,
            0x21122112,
            0xAABBCCDD,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(output[1] == 0x12345678);
        REQUIRE(output[2] == 0x21122112);
        REQUIRE(output[3] == 0xAABBCCDD);
    }

    SECTION("translates move handles") {
        auto a = MakeObject(kernel);
        Handle a_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MoveHandleDesc(1),
            a_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(a_handle) == nullptr);
    }

    SECTION("translates copy handles") {
        auto a = MakeObject(kernel);
        Handle a_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::CopyHandleDesc(1),
            a_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(a_handle) == a);
    }

    SECTION("translates multi-handle descriptors") {
        auto a = MakeObject(kernel);
        auto b = MakeObject(kernel);
        auto c = MakeObject(kernel);
        Handle a_handle, b_handle, c_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        process->handle_table.Create(std::addressof(b_handle), b);
        process->handle_table.Create(std::addressof(c_handle), c);
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 5),
            IPC::MoveHandleDesc(2),
            a_handle,
            b_handle,
            IPC::MoveHandleDesc(1),
            c_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(context.GetIncomingHandle(output[3]) == b);
        REQUIRE(context.GetIncomingHandle(output[5]) == c);
    }

    SECTION("translates null handles") {
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MoveHandleDesc(1),
            0,
        };

        auto result = context.PopulateFromIncomingCommandBuffer(input, process);

        REQUIRE(result == ResultSuccess);
        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == nullptr);
    }

    SECTION("translates CallingPid descriptors") {
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::CallingPidDesc(),
            0x98989898,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        REQUIRE(context.CommandBuffer()[2] == process->process_id);
    }

    SECTION("translates StaticBuffer descriptors") {
        auto mem = std::make_shared<BufferMem>(Memory::LIME3DS_PAGE_SIZE);
        MemoryRef buffer{mem};
        std::fill(buffer.GetPtr(), buffer.GetPtr() + buffer.GetSize(), 0xAB);

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address, buffer, static_cast<u32>(buffer.GetSize()), MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::StaticBufferDesc(buffer.GetSize(), 0),
            target_address,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        CHECK(context.GetStaticBuffer(0) == mem->Vector());

        REQUIRE(process->vm_manager.UnmapRange(
                    target_address, static_cast<u32>(buffer.GetSize())) == ResultSuccess);
    }

    SECTION("translates MappedBuffer descriptors") {
        auto mem = std::make_shared<BufferMem>(Memory::LIME3DS_PAGE_SIZE);
        MemoryRef buffer{mem};
        std::fill(buffer.GetPtr(), buffer.GetPtr() + buffer.GetSize(), 0xCD);

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address, buffer, static_cast<u32>(buffer.GetSize()), MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MappedBufferDesc(buffer.GetSize(), IPC::R),
            target_address,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        std::vector<u8> other_buffer(buffer.GetSize());
        context.GetMappedBuffer(0).Read(other_buffer.data(), 0, buffer.GetSize());

        CHECK(other_buffer == mem->Vector());

        REQUIRE(process->vm_manager.UnmapRange(
                    target_address, static_cast<u32>(buffer.GetSize())) == ResultSuccess);
    }

    SECTION("translates mixed params") {
        auto mem_static = std::make_shared<BufferMem>(Memory::LIME3DS_PAGE_SIZE);
        MemoryRef buffer_static{mem_static};
        std::fill(buffer_static.GetPtr(), buffer_static.GetPtr() + buffer_static.GetSize(), 0xCE);

        auto mem_mapped = std::make_shared<BufferMem>(Memory::LIME3DS_PAGE_SIZE);
        MemoryRef buffer_mapped{mem_mapped};
        std::fill(buffer_mapped.GetPtr(), buffer_mapped.GetPtr() + buffer_mapped.GetSize(), 0xDF);

        VAddr target_address_static = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address_static, buffer_static, static_cast<u32>(buffer_static.GetSize()),
            MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        VAddr target_address_mapped = 0x20000000;
        result = process->vm_manager.MapBackingMemory(target_address_mapped, buffer_mapped,
                                                      static_cast<u32>(buffer_mapped.GetSize()),
                                                      MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        auto a = MakeObject(kernel);
        Handle a_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        const u32_le input[]{
            IPC::MakeHeader(0, 2, 8),
            0x12345678,
            0xABCDEF00,
            IPC::MoveHandleDesc(1),
            a_handle,
            IPC::CallingPidDesc(),
            0,
            IPC::StaticBufferDesc(buffer_static.GetSize(), 0),
            target_address_static,
            IPC::MappedBufferDesc(buffer_mapped.GetSize(), IPC::R),
            target_address_mapped,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        CHECK(output[1] == 0x12345678);
        CHECK(output[2] == 0xABCDEF00);
        CHECK(context.GetIncomingHandle(output[4]) == a);
        CHECK(output[6] == process->process_id);
        CHECK(context.GetStaticBuffer(0) == mem_static->Vector());
        std::vector<u8> other_buffer(buffer_mapped.GetSize());
        context.GetMappedBuffer(0).Read(other_buffer.data(), 0, buffer_mapped.GetSize());
        CHECK(other_buffer == mem_mapped->Vector());

        REQUIRE(process->vm_manager.UnmapRange(target_address_static,
                                               static_cast<u32>(buffer_static.GetSize())) ==
                ResultSuccess);
        REQUIRE(process->vm_manager.UnmapRange(target_address_mapped,
                                               static_cast<u32>(buffer_mapped.GetSize())) ==
                ResultSuccess);
    }
}

TEST_CASE("HLERequestContext::WriteToOutgoingCommandBuffer", "[core][kernel]") {
    Core::Timing timing(1, 100);
    Core::System system;
    Memory::MemorySystem memory{system};
    Kernel::KernelSystem kernel(
        memory, timing, [] {}, Kernel::MemoryMode::Prod, 1,
        Kernel::New3dsHwCapabilities{false, false, Kernel::New3dsMemoryMode::Legacy});
    auto [server, client] = kernel.CreateSessionPair();
    HLERequestContext context(kernel, std::move(server), nullptr);

    auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
    auto* input = context.CommandBuffer();
    u32_le output[IPC::COMMAND_BUFFER_LENGTH];

    SECTION("works with empty cmdbuf") {
        input[0] = IPC::MakeHeader(0x1234, 0, 0);

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(output[0] == 0x12340000);
    }

    SECTION("translates regular params") {
        input[0] = IPC::MakeHeader(0, 3, 0);
        input[1] = 0x12345678;
        input[2] = 0x21122112;
        input[3] = 0xAABBCCDD;

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(output[1] == 0x12345678);
        REQUIRE(output[2] == 0x21122112);
        REQUIRE(output[3] == 0xAABBCCDD);
    }

    SECTION("translates move/copy handles") {
        auto a = MakeObject(kernel);
        auto b = MakeObject(kernel);
        input[0] = IPC::MakeHeader(0, 0, 4);
        input[1] = IPC::MoveHandleDesc(1);
        input[2] = context.AddOutgoingHandle(a);
        input[3] = IPC::CopyHandleDesc(1);
        input[4] = context.AddOutgoingHandle(b);

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(process->handle_table.GetGeneric(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(output[4]) == b);
    }

    SECTION("translates null handles") {
        input[0] = IPC::MakeHeader(0, 0, 2);
        input[1] = IPC::MoveHandleDesc(1);
        input[2] = context.AddOutgoingHandle(nullptr);

        auto result = context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(result == ResultSuccess);
        REQUIRE(output[2] == 0);
    }

    SECTION("translates multi-handle descriptors") {
        auto a = MakeObject(kernel);
        auto b = MakeObject(kernel);
        auto c = MakeObject(kernel);
        input[0] = IPC::MakeHeader(0, 0, 5);
        input[1] = IPC::MoveHandleDesc(2);
        input[2] = context.AddOutgoingHandle(a);
        input[3] = context.AddOutgoingHandle(b);
        input[4] = IPC::CopyHandleDesc(1);
        input[5] = context.AddOutgoingHandle(c);

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(process->handle_table.GetGeneric(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(output[3]) == b);
        REQUIRE(process->handle_table.GetGeneric(output[5]) == c);
    }

    SECTION("translates StaticBuffer descriptors") {
        std::vector<u8> input_buffer(Memory::LIME3DS_PAGE_SIZE);
        std::fill(input_buffer.begin(), input_buffer.end(), 0xAB);

        context.AddStaticBuffer(0, input_buffer);

        auto output_mem = std::make_shared<BufferMem>(Memory::LIME3DS_PAGE_SIZE);
        MemoryRef output_buffer{output_mem};

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address, output_buffer, static_cast<u32>(output_buffer.GetSize()),
            MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        input[0] = IPC::MakeHeader(0, 0, 2);
        input[1] = IPC::StaticBufferDesc(input_buffer.size(), 0);
        input[2] = target_address;

        // An entire command buffer plus enough space for one static buffer descriptor and its
        // target address
        std::array<u32_le, IPC::COMMAND_BUFFER_LENGTH + 2> output_cmdbuff;
        // Set up the output StaticBuffer
        output_cmdbuff[IPC::COMMAND_BUFFER_LENGTH] =
            IPC::StaticBufferDesc(output_buffer.GetSize(), 0);
        output_cmdbuff[IPC::COMMAND_BUFFER_LENGTH + 1] = target_address;

        context.WriteToOutgoingCommandBuffer(output_cmdbuff.data(), *process);

        CHECK(output_mem->Vector() == input_buffer);
        REQUIRE(process->vm_manager.UnmapRange(
                    target_address, static_cast<u32>(output_buffer.GetSize())) == ResultSuccess);
    }

    SECTION("translates StaticBuffer descriptors") {
        std::vector<u8> input_buffer(Memory::LIME3DS_PAGE_SIZE);
        std::fill(input_buffer.begin(), input_buffer.end(), 0xAB);

        auto output_mem = std::make_shared<BufferMem>(Memory::LIME3DS_PAGE_SIZE);
        MemoryRef output_buffer{output_mem};

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address, output_buffer, static_cast<u32>(output_buffer.GetSize()),
            MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        const u32_le input_cmdbuff[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MappedBufferDesc(output_buffer.GetSize(), IPC::W),
            target_address,
        };

        context.PopulateFromIncomingCommandBuffer(input_cmdbuff, process);

        context.GetMappedBuffer(0).Write(input_buffer.data(), 0, input_buffer.size());

        input[0] = IPC::MakeHeader(0, 0, 2);
        input[1] = IPC::MappedBufferDesc(output_buffer.GetSize(), IPC::W);
        input[2] = 0;

        context.WriteToOutgoingCommandBuffer(output, *process);

        CHECK(output[1] == IPC::MappedBufferDesc(output_buffer.GetSize(), IPC::W));
        CHECK(output[2] == target_address);
        CHECK(output_mem->Vector() == input_buffer);
        REQUIRE(process->vm_manager.UnmapRange(
                    target_address, static_cast<u32>(output_buffer.GetSize())) == ResultSuccess);
    }
}

} // namespace Kernel
