// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/audio_types.h"
#include "common/archives.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/dsp/dsp_dsp.h"

using DspPipe = AudioCore::DspPipe;
using InterruptType = Service::DSP::InterruptType;

SERIALIZE_EXPORT_IMPL(Service::DSP::DSP_DSP)
SERVICE_CONSTRUCT_IMPL(Service::DSP::DSP_DSP)

namespace AudioCore {
enum class DspPipe;
}

namespace Service::DSP {

void DSP_DSP::RecvData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 register_number = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(system.DSP().RecvData(register_number));

    LOG_DEBUG(Service_DSP, "register_number={}", register_number);
}

void DSP_DSP::RecvDataIsReady(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 register_number = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(system.DSP().RecvDataIsReady(register_number));

    LOG_DEBUG(Service_DSP, "register_number={}", register_number);
}

void DSP_DSP::SetSemaphore(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u16 semaphore_value = rp.Pop<u16>();

    system.DSP().SetSemaphore(semaphore_value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_INFO(Service_DSP, "called, semaphore_value={:04X}", semaphore_value);
}

void DSP_DSP::ConvertProcessAddressFromDspDram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 address = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);

    // TODO(merry): There is a per-region offset missing in this calculation (that seems to be
    // always zero).
    rb.Push<u32>((address << 1) + (Memory::DSP_RAM_VADDR + 0x40000));

    LOG_DEBUG(Service_DSP, "address=0x{:08X}", address);
}

void DSP_DSP::WriteProcessPipe(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 channel = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto buffer = rp.PopStaticBuffer();

    const DspPipe pipe = static_cast<DspPipe>(channel);

    // This behaviour was confirmed by RE.
    // The likely reason for this is that games tend to pass in garbage at these bytes
    // because they read random bytes off the stack.
    switch (pipe) {
    case DspPipe::Audio:
        ASSERT(buffer.size() >= 4);
        buffer[2] = 0;
        buffer[3] = 0;
        break;
    case DspPipe::Binary:
        ASSERT(buffer.size() >= 8);
        buffer[4] = 1;
        buffer[5] = 0;
        buffer[6] = 0;
        buffer[7] = 0;
        break;
    default:
        LOG_ERROR(Service_DSP, "Unknown pipe {}", pipe);
        break;
    }

    system.DSP().PipeWrite(pipe, buffer);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_DSP, "channel={}, size=0x{:X}, buffer_size={:X}", channel, size,
              buffer.size());
}

void DSP_DSP::ReadPipe(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 channel = rp.Pop<u32>();
    const u32 peer = rp.Pop<u32>();
    const u16 size = rp.Pop<u16>();

    const DspPipe pipe = static_cast<DspPipe>(channel);
    const u16 pipe_readable_size = static_cast<u16>(system.DSP().GetPipeReadableSize(pipe));

    std::vector<u8> pipe_buffer;
    if (pipe_readable_size >= size)
        pipe_buffer = system.DSP().PipeRead(pipe, size);
    else
        UNREACHABLE(); // No more data is in pipe. Hardware hangs in this case; Should never happen.

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushStaticBuffer(std::move(pipe_buffer), 0);

    LOG_DEBUG(Service_DSP, "channel={}, peer={}, size=0x{:04X}, pipe_readable_size=0x{:04X}",
              channel, peer, size, pipe_readable_size);
}

void DSP_DSP::GetPipeReadableSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 channel = rp.Pop<u32>();
    const u32 peer = rp.Pop<u32>();

    const DspPipe pipe = static_cast<DspPipe>(channel);
    const u16 pipe_readable_size = static_cast<u16>(system.DSP().GetPipeReadableSize(pipe));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u16>(pipe_readable_size);

    LOG_DEBUG(Service_DSP, "channel={}, peer={}, return pipe_readable_size=0x{:04X}", channel, peer,
              pipe_readable_size);
}

void DSP_DSP::ReadPipeIfPossible(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 channel = rp.Pop<u32>();
    const u32 peer = rp.Pop<u32>();
    const u16 size = rp.Pop<u16>();

    const DspPipe pipe = static_cast<DspPipe>(channel);
    const u16 pipe_readable_size = static_cast<u16>(system.DSP().GetPipeReadableSize(pipe));

    std::vector<u8> pipe_buffer;
    if (pipe_readable_size >= size) {
        pipe_buffer = system.DSP().PipeRead(pipe, size);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push<u16>(static_cast<u16>(pipe_buffer.size()));
    rb.PushStaticBuffer(std::move(pipe_buffer), 0);

    LOG_DEBUG(Service_DSP, "channel={}, peer={}, size=0x{:04X}, pipe_readable_size=0x{:04X}",
              channel, peer, size, pipe_readable_size);
}

void DSP_DSP::LoadComponent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto size = rp.Pop<u32>();
    const auto prog_mask = rp.Pop<u16>();
    const auto data_mask = rp.Pop<u16>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push(true);
    rb.PushMappedBuffer(buffer);

    std::vector<u8> component_data(size);
    buffer.Read(component_data.data(), 0, size);

    system.DSP().LoadComponent(component_data);

    LOG_INFO(Service_DSP, "called size=0x{:X}, prog_mask=0x{:04X}, data_mask=0x{:04X}", size,
             prog_mask, data_mask);
}

void DSP_DSP::UnloadComponent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    system.DSP().UnloadComponent();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_INFO(Service_DSP, "called");
}

void DSP_DSP::FlushDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const VAddr address = rp.Pop<u32>();
    [[maybe_unused]] const u32 size = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_DSP, "called address=0x{:08X}, size=0x{:X}, process={}", address, size,
              process->process_id);
}

void DSP_DSP::InvalidateDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const VAddr address = rp.Pop<u32>();
    [[maybe_unused]] const u32 size = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_DSP, "called address=0x{:08X}, size=0x{:X}, process={}", address, size,
              process->process_id);
}

void DSP_DSP::RegisterInterruptEvents(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 interrupt = rp.Pop<u32>();
    const u32 channel = rp.Pop<u32>();
    auto event = rp.PopObject<Kernel::Event>();

    ASSERT_MSG(interrupt < static_cast<u32>(InterruptType::Count) &&
                   channel < AudioCore::num_dsp_pipe,
               "Invalid type or pipe: interrupt = {}, channel = {}", interrupt, channel);

    const InterruptType type = static_cast<InterruptType>(interrupt);
    const DspPipe pipe = static_cast<DspPipe>(channel);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (event) { /// Register interrupt event
        if (HasTooManyEventsRegistered()) {
            LOG_INFO(Service_DSP,
                     "Ran out of space to register interrupts (Attempted to register "
                     "interrupt={}, channel={}, event={})",
                     interrupt, channel, event->GetName());
            rb.Push(Result(ErrorDescription::InvalidResultValue, ErrorModule::DSP,
                           ErrorSummary::OutOfResource, ErrorLevel::Status));
            return;
        } else {
            GetInterruptEvent(type, pipe) = event;
            LOG_INFO(Service_DSP, "Registered interrupt={}, channel={}, event={}", interrupt,
                     channel, event->GetName());
        }
    } else { /// Otherwise unregister event
        GetInterruptEvent(type, pipe) = nullptr;
        LOG_INFO(Service_DSP, "Unregistered interrupt={}, channel={}", interrupt, channel);
    }
    rb.Push(ResultSuccess);
}

void DSP_DSP::GetSemaphoreEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(semaphore_event);

    LOG_WARNING(Service_DSP, "(STUBBED) called");
}

void DSP_DSP::SetSemaphoreMask(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    preset_semaphore = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_DSP, "(STUBBED) called mask=0x{:04X}", preset_semaphore);
}

void DSP_DSP::GetHeadphoneStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(false); /// u8, 0 = not inserted, 1 = inserted

    LOG_DEBUG(Service_DSP, "called");
}

void DSP_DSP::ForceHeadphoneOut(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u8 force = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_DSP, "(STUBBED) called, force={}", force);
}

// DSP Interrupts:
// The audio-pipe interrupt occurs every frame tick. Userland programs normally have a thread
// that's waiting for an interrupt event. Immediately after this interrupt event, userland
// normally updates the state in the next region and increments the relevant frame counter by two.
void DSP_DSP::SignalInterrupt(InterruptType type, DspPipe pipe) {
    LOG_TRACE(Service_DSP, "called, type={}, pipe={}", type, pipe);
    const auto& event = GetInterruptEvent(type, pipe);
    if (event)
        event->Signal();
}

std::shared_ptr<Kernel::Event>& DSP_DSP::GetInterruptEvent(InterruptType type, DspPipe pipe) {
    switch (type) {
    case InterruptType::Zero:
        return interrupt_zero;
    case InterruptType::One:
        return interrupt_one;
    case InterruptType::Pipe: {
        const std::size_t pipe_index = static_cast<std::size_t>(pipe);
        ASSERT(pipe_index < AudioCore::num_dsp_pipe);
        return pipes[pipe_index];
    }
    case InterruptType::Count:
    default:
        break;
    }
    UNREACHABLE_MSG("Invalid interrupt type = {}", type);
}

bool DSP_DSP::HasTooManyEventsRegistered() const {
    std::size_t number =
        std::count_if(pipes.begin(), pipes.end(), [](const auto& evt) { return evt != nullptr; });

    if (interrupt_zero != nullptr)
        number++;
    if (interrupt_one != nullptr)
        number++;

    LOG_DEBUG(Service_DSP, "Number of events registered = {}", number);
    return number >= max_number_of_interrupt_events;
}

DSP_DSP::DSP_DSP(Core::System& system)
    : ServiceFramework("dsp::DSP", DefaultMaxSessions), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &DSP_DSP::RecvData, "RecvData"},
        {0x0002, &DSP_DSP::RecvDataIsReady, "RecvDataIsReady"},
        {0x0003, nullptr, "SendData"},
        {0x0004, nullptr, "SendDataIsEmpty"},
        {0x0005, nullptr, "SendFifoEx"},
        {0x0006, nullptr, "RecvFifoEx"},
        {0x0007, &DSP_DSP::SetSemaphore, "SetSemaphore"},
        {0x0008, nullptr, "GetSemaphore"},
        {0x0009, nullptr, "ClearSemaphore"},
        {0x000A, nullptr, "MaskSemaphore"},
        {0x000B, nullptr, "CheckSemaphoreRequest"},
        {0x000C, &DSP_DSP::ConvertProcessAddressFromDspDram, "ConvertProcessAddressFromDspDram"},
        {0x000D, &DSP_DSP::WriteProcessPipe, "WriteProcessPipe"},
        {0x000E, &DSP_DSP::ReadPipe, "ReadPipe"},
        {0x000F, &DSP_DSP::GetPipeReadableSize, "GetPipeReadableSize"},
        {0x0010, &DSP_DSP::ReadPipeIfPossible, "ReadPipeIfPossible"},
        {0x0011, &DSP_DSP::LoadComponent, "LoadComponent"},
        {0x0012, &DSP_DSP::UnloadComponent, "UnloadComponent"},
        {0x0013, &DSP_DSP::FlushDataCache, "FlushDataCache"},
        {0x0014, &DSP_DSP::InvalidateDataCache, "InvalidateDCache"},
        {0x0015, &DSP_DSP::RegisterInterruptEvents, "RegisterInterruptEvents"},
        {0x0016, &DSP_DSP::GetSemaphoreEventHandle, "GetSemaphoreEventHandle"},
        {0x0017, &DSP_DSP::SetSemaphoreMask, "SetSemaphoreMask"},
        {0x0018, nullptr, "GetPhysicalAddress"},
        {0x0019, nullptr, "GetVirtualAddress"},
        {0x001A, nullptr, "SetIirFilterI2S1_cmd1"},
        {0x001B, nullptr, "SetIirFilterI2S1_cmd2"},
        {0x001C, nullptr, "SetIirFilterEQ"},
        {0x001D, nullptr, "ReadMultiEx_SPI2"},
        {0x001E, nullptr, "WriteMultiEx_SPI2"},
        {0x001F, &DSP_DSP::GetHeadphoneStatus, "GetHeadphoneStatus"},
        {0x0020, &DSP_DSP::ForceHeadphoneOut, "ForceHeadphoneOut"},
        {0x0021, nullptr, "GetIsDspOccupied"},
        // clang-format on
    };

    RegisterHandlers(functions);

    semaphore_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "DSP_DSP::semaphore_event");

    semaphore_event->SetHLENotifier(
        [this]() { this->system.DSP().SetSemaphore(preset_semaphore); });

    system.DSP().SetInterruptHandler([dsp_ref = this, &system](InterruptType type, DspPipe pipe) {
        std::scoped_lock lock{system.Kernel().GetHLELock()};
        if (dsp_ref) {
            dsp_ref->SignalInterrupt(type, pipe);
        }
    });
}

DSP_DSP::~DSP_DSP() {
    semaphore_event = nullptr;
    pipes = {};
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto dsp = std::make_shared<DSP_DSP>(system);
    dsp->InstallAsService(service_manager);
}

} // namespace Service::DSP
