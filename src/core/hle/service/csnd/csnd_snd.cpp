// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/archives.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/result.h"
#include "core/hle/service/csnd/csnd_snd.h"

SERVICE_CONSTRUCT_IMPL(Service::CSND::CSND_SND)
SERIALIZE_EXPORT_IMPL(Service::CSND::CSND_SND)

namespace Service::CSND {

enum class CommandId : u16 {
    Start = 0x000,
    Pause = 0x001,
    SetEncoding = 0x002,
    SetSecondBlock = 0x003,
    SetLoopMode = 0x004,
    // unknown = 0x005,
    SetLinearInterpolation = 0x006,
    SetPsgDuty = 0x007,
    SetSampleRate = 0x008,
    SetVolume = 0x009,
    SetFirstBlock = 0x00A,
    SetFirstBlockAdpcmState = 0x00B,
    SetSecondBlockAdpcmState = 0x00C,
    SetSecondBlockAdpcmReload = 0x00D,
    ConfigureChannel = 0x00E,
    ConfigurePsg = 0x00F,
    ConfigurePsgNoise = 0x010,
    // 0x10x commands are audio capture related
    // unknown = 0x200
    UpdateState = 0x300,
};

struct Type0Command {
    u16_le next_command_offset;
    enum_le<CommandId> command_id;
    u8 finished;
    INSERT_PADDING_BYTES(3);
    union {
        struct {
            u32_le channel;
            u32_le value;
            INSERT_PADDING_BYTES(0x10);
        } start;

        struct {
            u32_le channel;
            u32_le value;
            INSERT_PADDING_BYTES(0x10);
        } pause;

        struct {
            u32_le channel;
            Encoding value;
            INSERT_PADDING_BYTES(0x13);
        } set_encoding;

        struct {
            u32_le channel;
            LoopMode value;
            INSERT_PADDING_BYTES(0x13);
        } set_loop_mode;

        struct {
            u32_le channel;
            u32_le value;
            INSERT_PADDING_BYTES(0x10);
        } set_linear_interpolation;

        struct {
            u32_le channel;
            u8 value;
            INSERT_PADDING_BYTES(0x13);
        } set_psg_duty;

        struct {
            u32_le channel;
            u32_le value;
            INSERT_PADDING_BYTES(0x10);
        } set_sample_rate;

        struct {
            u32_le channel;
            u16_le left_channel_volume;
            u16_le right_channel_volume;
            u16_le left_capture_volume;
            u16_le right_capture_volume;
            INSERT_PADDING_BYTES(0xC);
        } set_volume;

        struct {
            u32_le channel;
            u32_le address;
            u32_le size;
            INSERT_PADDING_BYTES(0xC);
        } set_block; // for either first block or second block

        struct {
            u32_le channel;
            s16_le predictor;
            u8 step_index;
            INSERT_PADDING_BYTES(0x11);
        } set_adpcm_state; // for either first block or second block

        struct {
            u32_le channel;
            u8 value;
            INSERT_PADDING_BYTES(0x13);
        } set_second_block_adpcm_reload;

        struct {
            union {
                BitField<0, 6, u32> channel;
                BitField<6, 1, u32> linear_interpolation;

                BitField<10, 2, u32> loop_mode;
                BitField<12, 2, u32> encoding;
                BitField<14, 1, u32> enable_playback;

                BitField<16, 16, u32> sample_rate;
            };

            u16_le left_channel_volume;
            u16_le right_channel_volume;
            u16_le left_capture_volume;
            u16_le right_capture_volume;
            u32_le block1_address;
            u32_le block2_address;
            u32_le size;
        } configure_channel;

        struct {
            union {
                BitField<0, 6, u32> channel;
                BitField<14, 1, u32> enable_playback;
                BitField<16, 16, u32> sample_rate;
            };
            u16_le left_channel_volume;
            u16_le right_channel_volume;
            u16_le left_capture_volume;
            u16_le right_capture_volume;
            u32_le duty;
            INSERT_PADDING_BYTES(0x8);
        } configure_psg;

        struct {
            union {
                BitField<0, 6, u32> channel;
                BitField<14, 1, u32> enable_playback;
            };
            u16_le left_channel_volume;
            u16_le right_channel_volume;
            u16_le left_capture_volume;
            u16_le right_capture_volume;
            INSERT_PADDING_BYTES(0xC);
        } configure_psg_noise;
    };
};
static_assert(sizeof(Type0Command) == 0x20, "Type0Command structure size is wrong");

struct MasterState {
    u32_le unknown_channel_flag;
    u32_le unknown;
};
static_assert(sizeof(MasterState) == 0x8, "MasterState structure size is wrong");

struct ChannelState {
    u8 active;
    INSERT_PADDING_BYTES(0x3);
    s16_le adpcm_predictor;
    u8 adpcm_step_index;
    INSERT_PADDING_BYTES(0x1);

    // 3dbrew says this is the current physical address. However the assembly of CSND module
    // from 11.3 system shows this is simply assigned as 0, which is also documented on ctrulib.
    u32_le zero;
};
static_assert(sizeof(ChannelState) == 0xC, "ChannelState structure size is wrong");

struct CaptureState {
    u8 active;
    INSERT_PADDING_BYTES(0x3);
    u32_le zero;
};
static_assert(sizeof(CaptureState) == 0x8, "CaptureState structure size is wrong");

void CSND_SND::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = Common::AlignUp(rp.Pop<u32>(), Memory::LIME3DS_PAGE_SIZE);
    master_state_offset = rp.Pop<u32>();
    channel_state_offset = rp.Pop<u32>();
    capture_state_offset = rp.Pop<u32>();
    type1_command_offset = rp.Pop<u32>();

    using Kernel::MemoryPermission;
    mutex = system.Kernel().CreateMutex(false, "CSND:mutex");
    shared_memory = system.Kernel()
                        .CreateSharedMemory(nullptr, size, MemoryPermission::ReadWrite,
                                            MemoryPermission::ReadWrite, 0,
                                            Kernel::MemoryRegion::BASE, "CSND:SharedMemory")
                        .Unwrap();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 3);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(mutex, shared_memory);

    LOG_WARNING(Service_CSND,
                "(STUBBED) called, size=0x{:08X} "
                "master_state_offset=0x{:08X} channel_state_offset=0x{:08X} "
                "capture_state_offset=0x{:08X} type1_command_offset=0x{:08X}",
                size, master_state_offset, channel_state_offset, capture_state_offset,
                type1_command_offset);
}

void CSND_SND::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    if (mutex)
        mutex = nullptr;
    if (shared_memory)
        shared_memory = nullptr;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

void CSND_SND::ExecuteCommands(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 addr = rp.Pop<u32>();
    LOG_DEBUG(Service_CSND, "(STUBBED) called, addr=0x{:08X}", addr);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (!shared_memory) {
        rb.Push(Result(ErrorDescription::InvalidResultValue, ErrorModule::CSND,
                       ErrorSummary::InvalidState, ErrorLevel::Status));
        LOG_ERROR(Service_CSND, "called, shared memory not allocated");
        return;
    }

    u32 offset = addr;
    while (offset != 0xFFFF) {
        Type0Command command;
        u8* ptr = shared_memory->GetPointer(offset);
        std::memcpy(&command, ptr, sizeof(Type0Command));
        offset = command.next_command_offset;

        switch (command.command_id) {
        case CommandId::Start:
            // TODO: start/stop the sound
            break;
        case CommandId::Pause:
            // TODO: pause/resume the sound
            break;
        case CommandId::SetEncoding:
            channels[command.set_encoding.channel].encoding = command.set_encoding.value;
            break;
        case CommandId::SetSecondBlock:
            channels[command.set_block.channel].block2_address = command.set_block.address;
            channels[command.set_block.channel].block2_size = command.set_block.size;
            break;
        case CommandId::SetLoopMode:
            channels[command.set_loop_mode.channel].loop_mode = command.set_loop_mode.value;
            break;
        case CommandId::SetLinearInterpolation:
            channels[command.set_linear_interpolation.channel].linear_interpolation =
                command.set_linear_interpolation.value != 0;
            break;
        case CommandId::SetPsgDuty:
            channels[command.set_psg_duty.channel].psg_duty = command.set_psg_duty.value;
            break;
        case CommandId::SetSampleRate:
            channels[command.set_sample_rate.channel].sample_rate = command.set_sample_rate.value;
            break;
        case CommandId::SetVolume:
            channels[command.set_volume.channel].left_channel_volume =
                command.set_volume.left_channel_volume;
            channels[command.set_volume.channel].right_channel_volume =
                command.set_volume.right_channel_volume;
            channels[command.set_volume.channel].left_capture_volume =
                command.set_volume.left_capture_volume;
            channels[command.set_volume.channel].right_capture_volume =
                command.set_volume.right_capture_volume;
            break;
        case CommandId::SetFirstBlock:
            channels[command.set_block.channel].block1_address = command.set_block.address;
            channels[command.set_block.channel].block1_size = command.set_block.size;
            break;
        case CommandId::SetFirstBlockAdpcmState:
            channels[command.set_adpcm_state.channel].block1_adpcm_state = {
                command.set_adpcm_state.predictor, command.set_adpcm_state.step_index};
            channels[command.set_adpcm_state.channel].block2_adpcm_state = {};
            channels[command.set_adpcm_state.channel].block2_adpcm_reload = false;
            break;
        case CommandId::SetSecondBlockAdpcmState:
            channels[command.set_adpcm_state.channel].block2_adpcm_state = {
                command.set_adpcm_state.predictor, command.set_adpcm_state.step_index};
            channels[command.set_adpcm_state.channel].block2_adpcm_reload = true;
            break;
        case CommandId::SetSecondBlockAdpcmReload:
            channels[command.set_second_block_adpcm_reload.channel].block2_adpcm_reload =
                command.set_second_block_adpcm_reload.value != 0;
            break;
        case CommandId::ConfigureChannel: {
            auto& configure = command.configure_channel;
            auto& channel = channels[configure.channel];
            channel.linear_interpolation = configure.linear_interpolation != 0;
            channel.loop_mode = static_cast<LoopMode>(configure.loop_mode.Value());
            channel.encoding = static_cast<Encoding>(configure.encoding.Value());
            channel.sample_rate = configure.sample_rate;
            channel.left_channel_volume = configure.left_channel_volume;
            channel.right_channel_volume = configure.right_channel_volume;
            channel.left_capture_volume = configure.left_capture_volume;
            channel.right_capture_volume = configure.right_capture_volume;
            channel.block1_address = configure.block1_address;
            channel.block2_address = configure.block2_address;
            channel.block1_size = channel.block2_size = configure.size;
            if (configure.enable_playback) {
                // TODO: startthe sound
            }
            break;
        }
        case CommandId::ConfigurePsg: {
            auto& configure = command.configure_psg;
            auto& channel = channels[configure.channel];
            channel.encoding = Encoding::Psg;
            channel.psg_duty = configure.duty;
            channel.sample_rate = configure.sample_rate;
            channel.left_channel_volume = configure.left_channel_volume;
            channel.right_channel_volume = configure.right_channel_volume;
            channel.left_capture_volume = configure.left_capture_volume;
            channel.right_capture_volume = configure.right_capture_volume;
            if (configure.enable_playback) {
                // TODO: startthe sound
            }
            break;
        }
        case CommandId::ConfigurePsgNoise: {
            auto& configure = command.configure_psg_noise;
            auto& channel = channels[configure.channel];
            channel.encoding = Encoding::Psg;
            channel.left_channel_volume = configure.left_channel_volume;
            channel.right_channel_volume = configure.right_channel_volume;
            channel.left_capture_volume = configure.left_capture_volume;
            channel.right_capture_volume = configure.right_capture_volume;
            if (configure.enable_playback) {
                // TODO: startthe sound
            }
            break;
        }
        case CommandId::UpdateState: {
            MasterState master{0, 0};
            std::memcpy(shared_memory->GetPointer(master_state_offset), &master, sizeof(master));

            u32 output_index = 0;
            for (u32 i = 0; i < ChannelCount; ++i) {
                if ((acquired_channel_mask & (1 << i)) == 0)
                    continue;
                ChannelState state;
                state.active = false;
                state.adpcm_predictor = channels[i].block1_adpcm_state.predictor;
                state.adpcm_predictor = channels[i].block1_adpcm_state.step_index;
                state.zero = 0;
                std::memcpy(
                    shared_memory->GetPointer(channel_state_offset + sizeof(state) * output_index),
                    &state, sizeof(state));
                ++output_index;
            }

            for (u32 i = 0; i < MaxCaptureUnits; ++i) {
                if (!capture_units[i])
                    continue;
                CaptureState state;
                state.active = false;
                state.zero = 0;
                std::memcpy(shared_memory->GetPointer(capture_state_offset + sizeof(state) * i),
                            &state, sizeof(state));
            }

            break;
        }
        default:
            LOG_ERROR(Service_CSND, "Unimplemented command ID 0x{:X}", command.command_id);
        }
    }

    *shared_memory->GetPointer(addr + offsetof(Type0Command, finished)) = 1;

    rb.Push(ResultSuccess);
}

void CSND_SND::AcquireSoundChannels(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    // This is "almost" hardcoded, as in CSND initializes this with some code during sysmodule
    // startup, but it always compute to the same value.
    acquired_channel_mask = 0xFFFFFF00;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(acquired_channel_mask);

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

void CSND_SND::ReleaseSoundChannels(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    acquired_channel_mask = 0;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

void CSND_SND::AcquireCapUnit(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (capture_units[0] && capture_units[1]) {
        LOG_WARNING(Service_CSND, "No more capture units available");
        rb.Push(Result(ErrorDescription::InvalidResultValue, ErrorModule::CSND,
                       ErrorSummary::OutOfResource, ErrorLevel::Status));
        rb.Skip(1, false);
        return;
    }
    rb.Push(ResultSuccess);

    if (capture_units[0]) {
        capture_units[1] = true;
        rb.Push<u32>(1);
    } else {
        capture_units[0] = true;
        rb.Push<u32>(0);
    }

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

void CSND_SND::ReleaseCapUnit(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 index = rp.Pop<u32>();

    capture_units[index] = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_CSND, "(STUBBED) called, capture_unit_index={}", index);
}

void CSND_SND::FlushDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const VAddr address = rp.Pop<u32>();
    [[maybe_unused]] const u32 size = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_CSND, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void CSND_SND::StoreDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const VAddr address = rp.Pop<u32>();
    [[maybe_unused]] const u32 size = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_CSND, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void CSND_SND::InvalidateDataCache(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const VAddr address = rp.Pop<u32>();
    [[maybe_unused]] const u32 size = rp.Pop<u32>();
    const auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_TRACE(Service_CSND, "(STUBBED) called address=0x{:08X}, size=0x{:08X}, process={}", address,
              size, process->process_id);
}

void CSND_SND::Reset(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_CSND, "(STUBBED) called");
}

CSND_SND::CSND_SND(Core::System& system) : ServiceFramework("csnd:SND", 4), system(system) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &CSND_SND::Initialize, "Initialize"},
        {0x0002, &CSND_SND::Shutdown, "Shutdown"},
        {0x0003, &CSND_SND::ExecuteCommands, "ExecuteCommands"},
        {0x0004, nullptr, "ExecuteType1Commands"},
        {0x0005, &CSND_SND::AcquireSoundChannels, "AcquireSoundChannels"},
        {0x0006, &CSND_SND::ReleaseSoundChannels, "ReleaseSoundChannels"},
        {0x0007, &CSND_SND::AcquireCapUnit, "AcquireCapUnit"},
        {0x0008, &CSND_SND::ReleaseCapUnit, "ReleaseCapUnit"},
        {0x0009, &CSND_SND::FlushDataCache, "FlushDataCache"},
        {0x000A, &CSND_SND::StoreDataCache, "StoreDataCache"},
        {0x000B, &CSND_SND::InvalidateDataCache, "InvalidateDataCache"},
        {0x000C, &CSND_SND::Reset, "Reset"},
        // clang-format on
    };

    RegisterHandlers(functions);
};

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<CSND_SND>(system)->InstallAsService(service_manager);
}

} // namespace Service::CSND
