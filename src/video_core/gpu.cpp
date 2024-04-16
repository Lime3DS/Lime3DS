// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/microprofile.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/hle/service/plgldr/plgldr.h"
#include "video_core/debug_utils/debug_utils.h"
#include "video_core/gpu.h"
#include "video_core/gpu_debugger.h"
#include "video_core/pica/pica_core.h"
#include "video_core/pica/regs_lcd.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_software/sw_blitter.h"
#include "video_core/video_core.h"

namespace VideoCore {

constexpr VAddr VADDR_LCD = 0x1ED02000;
constexpr VAddr VADDR_GPU = 0x1EF00000;

MICROPROFILE_DEFINE(GPU_DisplayTransfer, "GPU", "DisplayTransfer", MP_RGB(100, 100, 255));
MICROPROFILE_DEFINE(GPU_CmdlistProcessing, "GPU", "Cmdlist Processing", MP_RGB(100, 255, 100));

struct GPU::Impl {
    Core::Timing& timing;
    Core::System& system;
    Memory::MemorySystem& memory;
    std::shared_ptr<Pica::DebugContext> debug_context;
    Pica::PicaCore pica;
    GraphicsDebugger gpu_debugger;
    std::unique_ptr<RendererBase> renderer;
    RasterizerInterface* rasterizer;
    std::unique_ptr<SwRenderer::SwBlitter> sw_blitter;
    Core::TimingEventType* vblank_event;
    Service::GSP::InterruptHandler signal_interrupt;

    explicit Impl(Core::System& system, Frontend::EmuWindow& emu_window,
                  Frontend::EmuWindow* secondary_window)
        : timing{system.CoreTiming()}, system{system}, memory{system.Memory()},
          debug_context{Pica::g_debug_context}, pica{memory, debug_context},
          renderer{VideoCore::CreateRenderer(emu_window, secondary_window, pica, system)},
          rasterizer{renderer->Rasterizer()},
          sw_blitter{std::make_unique<SwRenderer::SwBlitter>(memory, rasterizer)} {}
    ~Impl() = default;
};

GPU::GPU(Core::System& system, Frontend::EmuWindow& emu_window,
         Frontend::EmuWindow* secondary_window)
    : impl{std::make_unique<Impl>(system, emu_window, secondary_window)} {
    impl->vblank_event = impl->timing.RegisterEvent(
        "GPU::VBlankCallback",
        [this](uintptr_t user_data, s64 cycles_late) { VBlankCallback(user_data, cycles_late); });
    impl->timing.ScheduleEvent(FRAME_TICKS, impl->vblank_event);

    // Bind the rasterizer to the PICA GPU
    impl->pica.BindRasterizer(impl->rasterizer);
}

GPU::~GPU() = default;

PAddr GPU::VirtualToPhysicalAddress(VAddr addr) {
    if (addr == 0) {
        return 0;
    }

    if (addr >= Memory::VRAM_VADDR && addr <= Memory::VRAM_VADDR_END) {
        return addr - Memory::VRAM_VADDR + Memory::VRAM_PADDR;
    }
    if (addr >= Memory::LINEAR_HEAP_VADDR && addr <= Memory::LINEAR_HEAP_VADDR_END) {
        return addr - Memory::LINEAR_HEAP_VADDR + Memory::FCRAM_PADDR;
    }
    if (addr >= Memory::NEW_LINEAR_HEAP_VADDR && addr <= Memory::NEW_LINEAR_HEAP_VADDR_END) {
        return addr - Memory::NEW_LINEAR_HEAP_VADDR + Memory::FCRAM_PADDR;
    }
    if (addr >= Memory::PLUGIN_3GX_FB_VADDR && addr <= Memory::PLUGIN_3GX_FB_VADDR_END) {
        auto plg_ldr = Service::PLGLDR::GetService(impl->system);
        if (plg_ldr) {
            return addr - Memory::PLUGIN_3GX_FB_VADDR + plg_ldr->GetPluginFBAddr();
        }
    }

    LOG_ERROR(HW_Memory, "Unknown virtual address @ 0x{:08X}", addr);
    return addr;
}

void GPU::SetInterruptHandler(Service::GSP::InterruptHandler handler) {
    impl->signal_interrupt = handler;
    impl->pica.SetInterruptHandler(handler);
}

void GPU::FlushRegion(PAddr addr, u32 size) {
    impl->rasterizer->FlushRegion(addr, size);
}

void GPU::InvalidateRegion(PAddr addr, u32 size) {
    impl->rasterizer->InvalidateRegion(addr, size);
}

void GPU::ClearAll(bool flush) {
    impl->rasterizer->ClearAll(flush);
}

void GPU::Execute(const Service::GSP::Command& command) {
    using Service::GSP::CommandId;
    auto& regs = impl->pica.regs;

    switch (command.id) {
    case CommandId::RequestDma: {
        impl->system.Memory().RasterizerFlushVirtualRegion(
            command.dma_request.source_address, command.dma_request.size, Memory::FlushMode::Flush);
        impl->system.Memory().RasterizerFlushVirtualRegion(command.dma_request.dest_address,
                                                           command.dma_request.size,
                                                           Memory::FlushMode::Invalidate);

        // TODO(Subv): These memory accesses should not go through the application's memory mapping.
        // They should go through the GSP module's memory mapping.
        const auto process = impl->system.Kernel().GetCurrentProcess();
        impl->memory.CopyBlock(*process, command.dma_request.dest_address,
                               command.dma_request.source_address, command.dma_request.size);
        impl->signal_interrupt(Service::GSP::InterruptId::DMA);
        break;
    }
    case CommandId::SubmitCmdList: {
        auto& params = command.submit_gpu_cmdlist;
        auto& cmdbuffer = regs.internal.pipeline.command_buffer;

        // Write to the command buffer GPU registers
        cmdbuffer.addr[0].Assign(VirtualToPhysicalAddress(params.address) >> 3);
        cmdbuffer.size[0].Assign(params.size >> 3);
        cmdbuffer.trigger[0] = 1;

        // Trigger processing of the command list
        SubmitCmdList(0);
        break;
    }
    case CommandId::MemoryFill: {
        auto& params = command.memory_fill;
        auto& memfill = regs.memory_fill_config;

        // Write to the memory fill GPU registers.
        if (params.start1 != 0) {
            memfill[0].address_start = VirtualToPhysicalAddress(params.start1) >> 3;
            memfill[0].address_end = VirtualToPhysicalAddress(params.end1) >> 3;
            memfill[0].value_32bit = params.value1;
            memfill[0].control = params.control1;
            MemoryFill(0);
        }
        if (params.start2 != 0) {
            memfill[1].address_start = VirtualToPhysicalAddress(params.start2) >> 3;
            memfill[1].address_end = VirtualToPhysicalAddress(params.end2) >> 3;
            memfill[1].value_32bit = params.value2;
            memfill[1].control = params.control2;
            MemoryFill(1);
        }
        break;
    }
    case CommandId::DisplayTransfer: {
        auto& params = command.display_transfer;
        auto& display_transfer = regs.display_transfer_config;

        // Write to the transfer engine GPU registers.
        display_transfer.input_address = VirtualToPhysicalAddress(params.in_buffer_address) >> 3;
        display_transfer.output_address = VirtualToPhysicalAddress(params.out_buffer_address) >> 3;
        display_transfer.input_size = params.in_buffer_size;
        display_transfer.output_size = params.out_buffer_size;
        display_transfer.flags = params.flags;
        display_transfer.trigger.Assign(1);

        // Trigger the display transfer.
        MemoryTransfer();
        break;
    }
    case CommandId::TextureCopy: {
        auto& params = command.texture_copy;
        auto& texture_copy = regs.display_transfer_config;

        // Write to the transfer engine GPU registers.
        texture_copy.input_address = VirtualToPhysicalAddress(params.in_buffer_address) >> 3;
        texture_copy.output_address = VirtualToPhysicalAddress(params.out_buffer_address) >> 3;
        texture_copy.texture_copy.size = params.size;
        texture_copy.texture_copy.input_size = params.in_width_gap;
        texture_copy.texture_copy.output_size = params.out_width_gap;
        texture_copy.flags = params.flags;
        texture_copy.trigger.Assign(1);

        // Trigger the texture copy.
        MemoryTransfer();
        break;
    }
    case CommandId::CacheFlush: {
        // Rasterizer flushing handled elsewhere in CPU read/write and other GPU handlers
        // Use command.cache_flush.regions to implement this handler
        break;
    }
    default:
        LOG_ERROR(HW_GPU, "Unknown command {:#08X}", command.id.Value());
    }

    // Notify debugger that a GSP command was processed.
    if (impl->debug_context) {
        impl->debug_context->OnEvent(Pica::DebugContext::Event::GSPCommandProcessed, &command);
    }
}

void GPU::SetBufferSwap(u32 screen_id, const Service::GSP::FrameBufferInfo& info) {
    const PAddr phys_address_left = VirtualToPhysicalAddress(info.address_left);
    const PAddr phys_address_right = VirtualToPhysicalAddress(info.address_right);

    // Update framebuffer properties.
    auto& framebuffer = impl->pica.regs.framebuffer_config[screen_id];
    if (info.active_fb == 0) {
        framebuffer.address_left1 = phys_address_left;
        framebuffer.address_right1 = phys_address_right;
    } else {
        framebuffer.address_left2 = phys_address_left;
        framebuffer.address_right2 = phys_address_right;
    }

    framebuffer.stride = info.stride;
    framebuffer.format = info.format;
    framebuffer.active_fb = info.shown_fb;

    // Notify debugger about the buffer swap.
    if (impl->debug_context) {
        impl->debug_context->OnEvent(Pica::DebugContext::Event::BufferSwapped, nullptr);
    }

    if (screen_id == 0) {
        MicroProfileFlip();
        impl->system.perf_stats->EndGameFrame();
    }
}

void GPU::SetColorFill(const Pica::ColorFill& fill) {
    impl->pica.regs_lcd.color_fill_top = fill;
    impl->pica.regs_lcd.color_fill_bottom = fill;
}

u32 GPU::ReadReg(VAddr addr) {
    switch (addr & 0xFFFFF000) {
    case VADDR_LCD: {
        const u32 offset = addr - VADDR_LCD;
        const u32 index = offset / sizeof(u32);
        ASSERT(addr % sizeof(u32) == 0);
        ASSERT(index < Pica::RegsLcd::NumIds());
        return impl->pica.regs_lcd[index];
    }
    case VADDR_GPU:
    case VADDR_GPU + 0x1000: {
        const u32 offset = addr - VADDR_GPU;
        const u32 index = offset / sizeof(u32);
        ASSERT(addr % sizeof(u32) == 0);
        ASSERT(index < Pica::PicaCore::Regs::NUM_REGS);
        return impl->pica.regs.reg_array[index];
    }
    default:
        UNREACHABLE_MSG("Read from unknown GPU address {:#08X}", addr);
    }
}

void GPU::WriteReg(VAddr addr, u32 data) {
    switch (addr & 0xFFFFF000) {
    case VADDR_LCD: {
        const u32 offset = addr - VADDR_LCD;
        const u32 index = offset / sizeof(u32);
        ASSERT(addr % sizeof(u32) == 0);
        ASSERT(index < Pica::RegsLcd::NumIds());
        impl->pica.regs_lcd[index] = data;
        break;
    }
    case VADDR_GPU:
    case VADDR_GPU + 0x1000: {
        const u32 offset = addr - VADDR_GPU;
        const u32 index = offset / sizeof(u32);

        ASSERT(addr % sizeof(u32) == 0);
        ASSERT(index < Pica::PicaCore::Regs::NUM_REGS);
        impl->pica.regs.reg_array[index] = data;

        // Handle registers that trigger GPU actions
        switch (index) {
        case GPU_REG_INDEX(memory_fill_config[0].trigger):
            MemoryFill(0);
            break;
        case GPU_REG_INDEX(memory_fill_config[1].trigger):
            MemoryFill(1);
            break;
        case GPU_REG_INDEX(display_transfer_config.trigger):
            MemoryTransfer();
            break;
        case GPU_REG_INDEX(internal.pipeline.command_buffer.trigger[0]):
            SubmitCmdList(0);
            break;
        case GPU_REG_INDEX(internal.pipeline.command_buffer.trigger[1]):
            SubmitCmdList(1);
            break;
        default:
            break;
        }
        break;
    }
    default:
        UNREACHABLE_MSG("Write to unknown GPU address {:#08X}", addr);
    }
}

void GPU::Sync() {
    impl->renderer->Sync();
}

VideoCore::RendererBase& GPU::Renderer() {
    return *impl->renderer;
}

Pica::PicaCore& GPU::PicaCore() {
    return impl->pica;
}

const Pica::PicaCore& GPU::PicaCore() const {
    return impl->pica;
}

Pica::DebugContext& GPU::DebugContext() {
    return *Pica::g_debug_context;
}

GraphicsDebugger& GPU::Debugger() {
    return impl->gpu_debugger;
}

void GPU::SubmitCmdList(u32 index) {
    // Check if a command list was triggered.
    auto& config = impl->pica.regs.internal.pipeline.command_buffer;
    if (!config.trigger[index]) {
        return;
    }

    MICROPROFILE_SCOPE(GPU_CmdlistProcessing);

    // Forward command list processing to the PICA core.
    const PAddr addr = config.GetPhysicalAddress(index);
    const u32 size = config.GetSize(index);
    impl->pica.ProcessCmdList(addr, size);
    config.trigger[index] = 0;
}

void GPU::MemoryFill(u32 index) {
    // Check if a memory fill was triggered.
    auto& config = impl->pica.regs.memory_fill_config[index];
    if (!config.trigger) {
        return;
    }

    // Perform memory fill.
    if (!impl->rasterizer->AccelerateFill(config)) {
        impl->sw_blitter->MemoryFill(config);
    }

    // It seems that it won't signal interrupt if "address_start" is zero.
    // TODO: hwtest this
    if (config.GetStartAddress() != 0) {
        if (!index) {
            impl->signal_interrupt(Service::GSP::InterruptId::PSC0);
        } else {
            impl->signal_interrupt(Service::GSP::InterruptId::PSC1);
        }
    }

    // Reset "trigger" flag and set the "finish" flag
    // This was confirmed to happen on hardware even if "address_start" is zero.
    config.trigger.Assign(0);
    config.finished.Assign(1);
}

void GPU::MemoryTransfer() {
    // Check if a transfer was triggered.
    auto& config = impl->pica.regs.display_transfer_config;
    if (!config.trigger.Value()) {
        return;
    }

    MICROPROFILE_SCOPE(GPU_DisplayTransfer);

    // Notify debugger about the display transfer.
    if (impl->debug_context) {
        impl->debug_context->OnEvent(Pica::DebugContext::Event::IncomingDisplayTransfer, nullptr);
    }

    // Perform memory transfer
    if (config.is_texture_copy) {
        if (!impl->rasterizer->AccelerateTextureCopy(config)) {
            impl->sw_blitter->TextureCopy(config);
        }
    } else {
        if (!impl->rasterizer->AccelerateDisplayTransfer(config)) {
            impl->sw_blitter->DisplayTransfer(config);
        }
    }

    // Complete transfer.
    config.trigger.Assign(0);
    impl->signal_interrupt(Service::GSP::InterruptId::PPF);
}

void GPU::VBlankCallback(std::uintptr_t user_data, s64 cycles_late) {
    // Present renderered frame.
    impl->renderer->SwapBuffers();

    // Signal to GSP that GPU interrupt has occurred
    impl->signal_interrupt(Service::GSP::InterruptId::PDC0);
    impl->signal_interrupt(Service::GSP::InterruptId::PDC1);

    // Reschedule recurrent event
    impl->timing.ScheduleEvent(FRAME_TICKS - cycles_late, impl->vblank_event);
}

template <class Archive>
void GPU::serialize(Archive& ar, const u32 file_version) {
    ar & impl->pica;
}

SERIALIZE_IMPL(GPU)

} // namespace VideoCore
