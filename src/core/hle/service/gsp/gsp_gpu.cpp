// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>
#include <vector>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "common/archives.h"
#include "common/bit_field.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/memory.h"
#include "video_core/gpu.h"
#include "video_core/gpu_debugger.h"
#include "video_core/pica/regs_lcd.h"

SERIALIZE_EXPORT_IMPL(Service::GSP::SessionData)
SERIALIZE_EXPORT_IMPL(Service::GSP::GSP_GPU)
SERVICE_CONSTRUCT_IMPL(Service::GSP::GSP_GPU)

namespace Service::GSP {

// Beginning address of HW regs
constexpr u32 REGS_BEGIN = 0x1EB00000;

namespace ErrCodes {
enum {
    // TODO(purpasmart): Check if this name fits its actual usage
    OutofRangeOrMisalignedAddress = 513,
    FirstInitialization = 519,
};
}

constexpr Result ResultFirstInitialization(ErrCodes::FirstInitialization, ErrorModule::GX,
                                           ErrorSummary::Success, ErrorLevel::Success);
constexpr Result ResultRegsOutOfRangeOrMisaligned(ErrCodes::OutofRangeOrMisalignedAddress,
                                                  ErrorModule::GX, ErrorSummary::InvalidArgument,
                                                  ErrorLevel::Usage); // 0xE0E02A01
constexpr Result ResultRegsMisaligned(ErrorDescription::MisalignedSize, ErrorModule::GX,
                                      ErrorSummary::InvalidArgument,
                                      ErrorLevel::Usage); // 0xE0E02BF2
constexpr Result ResultRegsInvalidSize(ErrorDescription::InvalidSize, ErrorModule::GX,
                                       ErrorSummary::InvalidArgument,
                                       ErrorLevel::Usage); // 0xE0E02BEC

u32 GSP_GPU::GetUnusedThreadId() const {
    for (u32 id = 0; id < MaxGSPThreads; ++id) {
        if (!used_thread_ids[id]) {
            return id;
        }
    }

    UNREACHABLE_MSG("All GSP threads are in use");
    return 0;
}

CommandBuffer* GSP_GPU::GetCommandBuffer(u32 thread_id) {
    auto* ptr = shared_memory->GetPointer(0x800 + (thread_id * sizeof(CommandBuffer)));
    return reinterpret_cast<CommandBuffer*>(ptr);
}

FrameBufferUpdate* GSP_GPU::GetFrameBufferInfo(u32 thread_id, u32 screen_index) {
    DEBUG_ASSERT_MSG(screen_index < 2, "Invalid screen index");

    // For each thread there are two FrameBufferUpdate fields
    const u32 offset = 0x200 + (2 * thread_id + screen_index) * sizeof(FrameBufferUpdate);
    u8* ptr = shared_memory->GetPointer(offset);
    return reinterpret_cast<FrameBufferUpdate*>(ptr);
}

InterruptRelayQueue* GSP_GPU::GetInterruptRelayQueue(u32 thread_id) {
    u8* ptr = shared_memory->GetPointer(sizeof(InterruptRelayQueue) * thread_id);
    return reinterpret_cast<InterruptRelayQueue*>(ptr);
}

void GSP_GPU::ClientDisconnected(std::shared_ptr<Kernel::ServerSession> server_session) {
    const SessionData* session_data = GetSessionData(server_session);
    if (active_thread_id == session_data->thread_id) {
        ReleaseRight(session_data);
    }
    SessionRequestHandler::ClientDisconnected(server_session);
}

/**
 * Writes sequential GSP GPU hardware registers using an array of source data
 *
 * @param base_address The address of the first register in the sequence
 * @param size_in_bytes The number of registers to update (size of data)
 * @param data A vector containing the source data
 * @return ResultSuccess if the parameters are valid, error code otherwise
 */
static Result WriteHWRegs(u32 base_address, u32 size_in_bytes, std::span<const u8> data,
                          VideoCore::GPU& gpu) {
    // This magic number is verified to be done by the gsp module
    const u32 max_size_in_bytes = 0x80;

    if (base_address & 3 || base_address >= 0x420000) {
        LOG_ERROR(Service_GSP,
                  "Write address was out of range or misaligned! (address=0x{:08x}, size=0x{:08x})",
                  base_address, size_in_bytes);
        return ResultRegsOutOfRangeOrMisaligned;
    }

    if (size_in_bytes > max_size_in_bytes) {
        LOG_ERROR(Service_GSP, "Out of range size 0x{:08x}", size_in_bytes);
        return ResultRegsInvalidSize;
    }

    if (size_in_bytes & 3) {
        LOG_ERROR(Service_GSP, "Misaligned size 0x{:08x}", size_in_bytes);
        return ResultRegsMisaligned;
    }

    std::size_t offset = 0;
    while (size_in_bytes > 0) {
        u32 value;
        std::memcpy(&value, &data[offset], sizeof(u32));
        gpu.WriteReg(REGS_BEGIN + base_address, value);

        size_in_bytes -= 4;
        offset += 4;
        base_address += 4;
    }

    return ResultSuccess;
}

/**
 * Updates sequential GSP GPU hardware registers using parallel arrays of source data and masks.
 * For each register, the value is updated only where the mask is high
 *
 * @param base_address  The address of the first register in the sequence
 * @param size_in_bytes The number of registers to update (size of data)
 * @param data    A vector containing the data to write
 * @param masks   A vector containing the masks
 * @return ResultSuccess if the parameters are valid, error code otherwise
 */
static Result WriteHWRegsWithMask(u32 base_address, u32 size_in_bytes, std::span<const u8> data,
                                  std::span<const u8> masks, VideoCore::GPU& gpu) {
    // This magic number is verified to be done by the gsp module
    const u32 max_size_in_bytes = 0x80;

    if (base_address & 3 || base_address >= 0x420000) {
        LOG_ERROR(Service_GSP,
                  "Write address was out of range or misaligned! (address=0x{:08x}, size=0x{:08x})",
                  base_address, size_in_bytes);
        return ResultRegsOutOfRangeOrMisaligned;
    }

    if (size_in_bytes > max_size_in_bytes) {
        LOG_ERROR(Service_GSP, "Out of range size 0x{:08x}", size_in_bytes);
        return ResultRegsInvalidSize;
    }

    if (size_in_bytes & 3) {
        LOG_ERROR(Service_GSP, "Misaligned size 0x{:08x}", size_in_bytes);
        return ResultRegsMisaligned;
    }

    std::size_t offset = 0;
    while (size_in_bytes > 0) {
        const u32 reg_address = base_address + REGS_BEGIN;
        u32 reg_value = gpu.ReadReg(reg_address);

        u32 value, mask;
        std::memcpy(&value, &data[offset], sizeof(u32));
        std::memcpy(&mask, &masks[offset], sizeof(u32));

        // Update the current value of the register only for set mask bits
        reg_value = (reg_value & ~mask) | (value & mask);
        gpu.WriteReg(reg_address, reg_value);

        size_in_bytes -= 4;
        offset += 4;
        base_address += 4;
    }

    return ResultSuccess;
}

void GSP_GPU::WriteHWRegs(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 reg_addr = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    const auto src_data = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::WriteHWRegs(reg_addr, size, src_data, system.GPU()));
}

void GSP_GPU::WriteHWRegsWithMask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 reg_addr = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    const auto src_data = rp.PopStaticBuffer();
    const auto mask_data = rp.PopStaticBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(GSP::WriteHWRegsWithMask(reg_addr, size, src_data, mask_data, system.GPU()));
}

void GSP_GPU::ReadHWRegs(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 reg_addr = rp.Pop<u32>();
    u32 input_size = rp.Pop<u32>();

    static constexpr u32 MaxReadSize = 0x80;
    u32 size = std::min(input_size, MaxReadSize);

    if ((reg_addr % 4) != 0 || reg_addr >= 0x420000) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultRegsOutOfRangeOrMisaligned);
        LOG_ERROR(Service_GSP, "Invalid address 0x{:08x}", reg_addr);
        return;
    }

    // Size should be word-aligned
    if ((size % 4) != 0) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultRegsMisaligned);
        LOG_ERROR(Service_GSP, "Invalid size 0x{:08x}", size);
        return;
    }

    std::vector<u8> buffer(size);
    for (u32 word = 0; word < size / sizeof(u32); ++word) {
        const u32 data = system.GPU().ReadReg(REGS_BEGIN + reg_addr + word * sizeof(u32));
        std::memcpy(buffer.data() + word * sizeof(u32), &data, sizeof(u32));
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushStaticBuffer(std::move(buffer), 0);
}

void GSP_GPU::SetBufferSwap(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 screen_id = rp.Pop<u32>();
    auto fb_info = rp.PopRaw<FrameBufferInfo>();

    system.GPU().SetBufferSwap(screen_id, fb_info);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void GSP_GPU::FlushDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] u32 address = rp.Pop<u32>();
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    [[maybe_unused]] auto process = rp.PopObject<Kernel::Process>();

    // TODO(purpasmart96): Verify return header on HW

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::InvalidateDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] u32 address = rp.Pop<u32>();
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    [[maybe_unused]] auto process = rp.PopObject<Kernel::Process>();

    // TODO(purpasmart96): Verify return header on HW

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::SetAxiConfigQoSMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 mode = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_GSP, "(STUBBED) called mode=0x{:08X}", mode);
}

void GSP_GPU::RegisterInterruptRelayQueue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 flags = rp.Pop<u32>();

    auto interrupt_event = rp.PopObject<Kernel::Event>();
    ASSERT_MSG(interrupt_event, "handle is not valid!");

    interrupt_event->SetName("GSP_GPU::interrupt_event");

    SessionData* session_data = GetSessionData(ctx.Session());
    session_data->interrupt_event = std::move(interrupt_event);
    session_data->registered = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    if (first_initialization) {
        // This specific code is required for a successful initialization, rather than 0
        first_initialization = false;
        rb.Push(ResultFirstInitialization);
    } else {
        rb.Push(ResultSuccess);
    }

    rb.Push(session_data->thread_id);
    rb.PushCopyObjects(shared_memory);

    LOG_DEBUG(Service_GSP, "called, flags=0x{:08X}", flags);
}

void GSP_GPU::UnregisterInterruptRelayQueue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    SessionData* session_data = GetSessionData(ctx.Session());
    session_data->interrupt_event = nullptr;
    session_data->registered = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_GSP, "called");
}

void GSP_GPU::SignalInterruptForThread(InterruptId interrupt_id, u32 thread_id) {
    SessionData* session_data = FindRegisteredThreadData(thread_id);
    if (!session_data) {
        return;
    }

    auto interrupt_event = session_data->interrupt_event;
    if (interrupt_event == nullptr) {
        LOG_WARNING(Service_GSP, "cannot synchronize until GSP event has been created!");
        return;
    }

    auto* interrupt_relay_queue = GetInterruptRelayQueue(thread_id);
    u8 next = interrupt_relay_queue->index;
    next += interrupt_relay_queue->number_interrupts;
    next = next % 0x34; // 0x34 is the number of interrupt slots

    interrupt_relay_queue->number_interrupts += 1;

    interrupt_relay_queue->slot[next] = interrupt_id;
    interrupt_relay_queue->error_code = 0x0; // No error

    // Update framebuffer information if requested
    const s32 screen_id = (interrupt_id == InterruptId::PDC0)   ? 0
                          : (interrupt_id == InterruptId::PDC1) ? 1
                                                                : -1;
    if (screen_id != -1) {
        auto* info = GetFrameBufferInfo(thread_id, screen_id);
        if (info->is_dirty) {
            system.GPU().SetBufferSwap(screen_id, info->framebuffer_info[info->index]);
            info->is_dirty.Assign(false);
        }
    }

    interrupt_event->Signal();
}

void GSP_GPU::SignalInterrupt(InterruptId interrupt_id) {
    if (nullptr == shared_memory) {
        LOG_WARNING(Service_GSP, "cannot synchronize until GSP shared memory has been created!");
        return;
    }

    // The PDC0 and PDC1 interrupts are fired even if the GPU right hasn't been acquired.
    // Normal interrupts are only signaled for the active thread (ie, the thread that has the GPU
    // right), but the PDC0/1 interrupts are signaled for every registered thread.
    if (interrupt_id == InterruptId::PDC0 || interrupt_id == InterruptId::PDC1) {
        for (u32 thread_id = 0; thread_id < MaxGSPThreads; ++thread_id) {
            SignalInterruptForThread(interrupt_id, thread_id);
        }
        return;
    }

    // For normal interrupts, don't do anything if no process has acquired the GPU right.
    if (active_thread_id == std::numeric_limits<u32>::max()) {
        return;
    }

    SignalInterruptForThread(interrupt_id, active_thread_id);
}

void GSP_GPU::SetLcdForceBlack(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const bool enable_black = rp.Pop<bool>();

    Pica::ColorFill data{};
    data.is_enabled.Assign(enable_black);
    system.GPU().SetColorFill(data);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void GSP_GPU::TriggerCmdReqQueue(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    // Iterate through each command.
    auto* command_buffer = GetCommandBuffer(active_thread_id);
    auto& gpu = system.GPU();
    for (u32 i = 0; i < command_buffer->number_commands; i++) {
        gpu.Debugger().GXCommandProcessed(command_buffer->commands[i]);

        // Decode and execute command
        gpu.Execute(command_buffer->commands[i]);

        // Indicates that command has completed
        command_buffer->number_commands.Assign(command_buffer->number_commands - 1);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void GSP_GPU::ImportDisplayCaptureInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    if (active_thread_id == std::numeric_limits<u32>::max()) {
        LOG_WARNING(Service_GSP, "Called without an active thread.");

        // TODO: Find the right error code.
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(-1);
        return;
    }

    FrameBufferUpdate* top_screen = GetFrameBufferInfo(active_thread_id, 0);
    FrameBufferUpdate* bottom_screen = GetFrameBufferInfo(active_thread_id, 1);

    struct CaptureInfoEntry {
        u32_le address_left;
        u32_le address_right;
        u32_le format;
        u32_le stride;
    };

    CaptureInfoEntry top_entry, bottom_entry;
    // Top Screen
    top_entry.address_left = top_screen->framebuffer_info[top_screen->index].address_left;
    top_entry.address_right = top_screen->framebuffer_info[top_screen->index].address_right;
    top_entry.format = top_screen->framebuffer_info[top_screen->index].format;
    top_entry.stride = top_screen->framebuffer_info[top_screen->index].stride;
    // Bottom Screen
    bottom_entry.address_left = bottom_screen->framebuffer_info[bottom_screen->index].address_left;
    bottom_entry.address_right =
        bottom_screen->framebuffer_info[bottom_screen->index].address_right;
    bottom_entry.format = bottom_screen->framebuffer_info[bottom_screen->index].format;
    bottom_entry.stride = bottom_screen->framebuffer_info[bottom_screen->index].stride;

    IPC::RequestBuilder rb = rp.MakeBuilder(9, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(top_entry);
    rb.PushRaw(bottom_entry);

    LOG_WARNING(Service_GSP, "called");
}

static void CopyFrameBuffer(Core::System& system, VAddr dst, VAddr src, u32 stride, u32 lines) {
    auto dst_ptr = system.Memory().GetPointer(dst);
    const auto src_ptr = system.Memory().GetPointer(src);
    if (!dst_ptr || !src_ptr) {
        LOG_WARNING(Service_GSP,
                    "Could not resolve pointers for framebuffer capture, skipping screen.");
        return;
    }

    system.Memory().RasterizerFlushVirtualRegion(src, stride * lines, Memory::FlushMode::Flush);
    std::memcpy(dst_ptr, src_ptr, stride * lines);
    system.Memory().RasterizerFlushVirtualRegion(dst, stride * lines,
                                                 Memory::FlushMode::Invalidate);
}

void GSP_GPU::SaveVramSysArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    if (active_thread_id == std::numeric_limits<u32>::max()) {
        LOG_WARNING(Service_GSP, "Called without an active thread.");

        // TODO: Find the right error code.
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(-1);
        return;
    }

    LOG_INFO(Service_GSP, "called");

    // TODO: This should also save LCD register state.
    system.Memory().RasterizerFlushVirtualRegion(Memory::VRAM_VADDR, Memory::VRAM_SIZE,
                                                 Memory::FlushMode::Flush);
    const auto vram = system.Memory().GetPointer(Memory::VRAM_VADDR);
    saved_vram.emplace(std::vector<u8>(Memory::VRAM_SIZE));
    std::memcpy(saved_vram.get().data(), vram, Memory::VRAM_SIZE);

    const auto top_screen = GetFrameBufferInfo(active_thread_id, 0);
    if (top_screen) {
        const auto top_fb = top_screen->framebuffer_info[top_screen->index];
        if (top_fb.address_left) {
            CopyFrameBuffer(system, FRAMEBUFFER_SAVE_AREA_TOP_LEFT, top_fb.address_left,
                            top_fb.stride, TOP_FRAMEBUFFER_HEIGHT);
        } else {
            LOG_WARNING(Service_GSP, "No framebuffer bound to top left screen, skipping capture.");
        }
        if (top_fb.address_right) {
            CopyFrameBuffer(system, FRAMEBUFFER_SAVE_AREA_TOP_RIGHT, top_fb.address_right,
                            top_fb.stride, TOP_FRAMEBUFFER_HEIGHT);
        } else {
            LOG_WARNING(Service_GSP, "No framebuffer bound to top right screen, skipping capture.");
        }
    } else {
        LOG_WARNING(Service_GSP, "No top screen bound, skipping capture.");
    }

    const auto bottom_screen = GetFrameBufferInfo(active_thread_id, 1);
    if (bottom_screen) {
        const auto bottom_fb = bottom_screen->framebuffer_info[bottom_screen->index];
        if (bottom_fb.address_left) {
            CopyFrameBuffer(system, FRAMEBUFFER_SAVE_AREA_BOTTOM, bottom_fb.address_left,
                            bottom_fb.stride, BOTTOM_FRAMEBUFFER_HEIGHT);
        } else {
            LOG_WARNING(Service_GSP, "No framebuffer bound to bottom screen, skipping capture.");
        }
    } else {
        LOG_WARNING(Service_GSP, "No bottom screen bound, skipping capture.");
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void GSP_GPU::RestoreVramSysArea(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_GSP, "called");

    if (saved_vram) {
        // TODO: This should also restore LCD register state.
        auto vram = system.Memory().GetPointer(Memory::VRAM_VADDR);
        std::memcpy(vram, saved_vram.get().data(), Memory::VRAM_SIZE);
        system.Memory().RasterizerFlushVirtualRegion(Memory::VRAM_VADDR, Memory::VRAM_SIZE,
                                                     Memory::FlushMode::Invalidate);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

Result GSP_GPU::AcquireGpuRight(const Kernel::HLERequestContext& ctx,
                                const std::shared_ptr<Kernel::Process>& process, u32 flag,
                                bool blocking) {
    const auto session_data = GetSessionData(ctx.Session());

    LOG_DEBUG(Service_GSP, "called flag={:08X} process={} thread_id={}", flag, process->process_id,
              session_data->thread_id);

    if (active_thread_id == session_data->thread_id) {
        return {ErrorDescription::AlreadyDone, ErrorModule::GX, ErrorSummary::Success,
                ErrorLevel::Success};
    }

    if (blocking) {
        // TODO: The thread should be put to sleep until acquired.
        ASSERT_MSG(active_thread_id == std::numeric_limits<u32>::max(),
                   "Sleeping for GPU right is not yet supported.");
    } else if (active_thread_id != std::numeric_limits<u32>::max()) {
        return {ErrorDescription::Busy, ErrorModule::GX, ErrorSummary::WouldBlock,
                ErrorLevel::Status};
    }

    active_thread_id = session_data->thread_id;
    return ResultSuccess;
}

void GSP_GPU::TryAcquireRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto process = rp.PopObject<Kernel::Process>();

    const auto result = AcquireGpuRight(ctx, process, 0, false);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void GSP_GPU::AcquireRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto flag = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    const auto result = AcquireGpuRight(ctx, process, flag, true);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void GSP_GPU::ReleaseRight(const SessionData* session_data) {
    ASSERT_MSG(active_thread_id == session_data->thread_id,
               "Wrong thread tried to release GPU right");
    active_thread_id = std::numeric_limits<u32>::max();
}

void GSP_GPU::ReleaseRight(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    const SessionData* session_data = GetSessionData(ctx.Session());
    ReleaseRight(session_data);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_GSP, "called");
}

void GSP_GPU::StoreDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    [[maybe_unused]] u32 address = rp.Pop<u32>();
    [[maybe_unused]] u32 size = rp.Pop<u32>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_GSP, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void GSP_GPU::SetLedForceOff(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u8 state = rp.Pop<u8>();

    system.Kernel().GetSharedPageHandler().Set3DLed(state);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_DEBUG(Service_GSP, "(STUBBED) called");
}

void GSP_GPU::SetInternalPriorities(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto priority = rp.Pop<u32>();
    const auto priority_with_rights = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_GSP, "(STUBBED) called priority={:#02X}, priority_with_rights={:#02X}",
              priority, priority_with_rights);
}

SessionData* GSP_GPU::FindRegisteredThreadData(u32 thread_id) {
    for (auto& session_info : connected_sessions) {
        SessionData* data = static_cast<SessionData*>(session_info.data.get());
        if (!data->registered)
            continue;
        if (data->thread_id == thread_id)
            return data;
    }
    return nullptr;
}

template <class Archive>
void GSP_GPU::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar& shared_memory;
    ar& active_thread_id;
    ar& first_initialization;
    ar& used_thread_ids;
    ar& saved_vram;
}
SERIALIZE_IMPL(GSP_GPU)

GSP_GPU::GSP_GPU(Core::System& system) : ServiceFramework("gsp::Gpu", 4), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &GSP_GPU::WriteHWRegs, "WriteHWRegs"},
        {0x0002, &GSP_GPU::WriteHWRegsWithMask, "WriteHWRegsWithMask"},
        {0x0003, nullptr, "WriteHWRegRepeat"},
        {0x0004, &GSP_GPU::ReadHWRegs, "ReadHWRegs"},
        {0x0005, &GSP_GPU::SetBufferSwap, "SetBufferSwap"},
        {0x0006, nullptr, "SetCommandList"},
        {0x0007, nullptr, "RequestDma"},
        {0x0008, &GSP_GPU::FlushDataCache, "FlushDataCache"},
        {0x0009, &GSP_GPU::InvalidateDataCache, "InvalidateDataCache"},
        {0x000A, nullptr, "RegisterInterruptEvents"},
        {0x000B, &GSP_GPU::SetLcdForceBlack, "SetLcdForceBlack"},
        {0x000C, &GSP_GPU::TriggerCmdReqQueue, "TriggerCmdReqQueue"},
        {0x000D, nullptr, "SetDisplayTransfer"},
        {0x000E, nullptr, "SetTextureCopy"},
        {0x000F, nullptr, "SetMemoryFill"},
        {0x0010, &GSP_GPU::SetAxiConfigQoSMode, "SetAxiConfigQoSMode"},
        {0x0011, nullptr, "SetPerfLogMode"},
        {0x0012, nullptr, "GetPerfLog"},
        {0x0013, &GSP_GPU::RegisterInterruptRelayQueue, "RegisterInterruptRelayQueue"},
        {0x0014, &GSP_GPU::UnregisterInterruptRelayQueue, "UnregisterInterruptRelayQueue"},
        {0x0015, &GSP_GPU::TryAcquireRight, "TryAcquireRight"},
        {0x0016, &GSP_GPU::AcquireRight, "AcquireRight"},
        {0x0017, &GSP_GPU::ReleaseRight, "ReleaseRight"},
        {0x0018, &GSP_GPU::ImportDisplayCaptureInfo, "ImportDisplayCaptureInfo"},
        {0x0019, &GSP_GPU::SaveVramSysArea, "SaveVramSysArea"},
        {0x001A, &GSP_GPU::RestoreVramSysArea, "RestoreVramSysArea"},
        {0x001B, nullptr, "ResetGpuCore"},
        {0x001C, &GSP_GPU::SetLedForceOff, "SetLedForceOff"},
        {0x001D, nullptr, "SetTestCommand"},
        {0x001E, &GSP_GPU::SetInternalPriorities, "SetInternalPriorities"},
        {0x001F, &GSP_GPU::StoreDataCache, "StoreDataCache"},
        // clang-format on
    };
    RegisterHandlers(functions);

    using Kernel::MemoryPermission;
    shared_memory = system.Kernel()
                        .CreateSharedMemory(nullptr, 0x1000, MemoryPermission::ReadWrite,
                                            MemoryPermission::ReadWrite, 0,
                                            Kernel::MemoryRegion::BASE, "GSP:SharedMemory")
                        .Unwrap();

    first_initialization = true;
};

std::unique_ptr<Kernel::SessionRequestHandler::SessionDataBase> GSP_GPU::MakeSessionData() {
    return std::make_unique<SessionData>(this);
}

template <class Archive>
void SessionData::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(*this);
    ar& gsp;
    ar& interrupt_event;
    ar& thread_id;
    ar& registered;
}
SERIALIZE_IMPL(SessionData)

SessionData::SessionData(GSP_GPU* gsp) : gsp(gsp) {
    // Assign a new thread id to this session when it connects. Note: In the real GSP service this
    // is done through a real thread (svcCreateThread) but we have to simulate it since our HLE
    // services don't have threads.
    thread_id = gsp->GetUnusedThreadId();
    gsp->used_thread_ids[thread_id] = true;
}

SessionData::~SessionData() {
    // Free the thread id slot so that other sessions can use it.
    gsp->used_thread_ids[thread_id] = false;
}

} // namespace Service::GSP
