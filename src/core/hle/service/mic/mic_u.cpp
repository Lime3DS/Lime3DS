// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>
#include <boost/serialization/weak_ptr.hpp>
#include "audio_core/input.h"
#include "audio_core/input_details.h"
#include "common/archives.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/mic/mic_u.h"

SERVICE_CONSTRUCT_IMPL(Service::MIC::MIC_U)
SERIALIZE_EXPORT_IMPL(Service::MIC::MIC_U)

namespace Service::MIC {

template <class Archive>
void MIC_U::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar&* impl.get();
}

/// Microphone audio encodings.
enum class Encoding : u8 {
    PCM8 = 0,        ///< Unsigned 8-bit PCM.
    PCM16 = 1,       ///< Unsigned 16-bit PCM.
    PCM8Signed = 2,  ///< Signed 8-bit PCM.
    PCM16Signed = 3, ///< Signed 16-bit PCM.
};

/// Microphone audio sampling rates. The actual accurate sampling rate can be calculated using
/// (16756991 / 512) / (SampleRate + 1) where SampleRate is one of the above values.
enum class SampleRate : u8 {
    Rate32730 = 0, ///< 32728.498 Hz
    Rate16360 = 1, ///< 16364.479 Hz
    Rate10910 = 2, ///< 10909.499 Hz
    Rate8180 = 3   ///< 8182.1245 Hz
};

constexpr u32 GetSampleRateInHz(SampleRate sample_rate) {
    switch (sample_rate) {
    case SampleRate::Rate8180:
        return 8182;
    case SampleRate::Rate10910:
        return 10909;
    case SampleRate::Rate16360:
        return 16364;
    case SampleRate::Rate32730:
        return 32728;
    default:
        UNREACHABLE();
    }
}

// The 3ds hardware was tested to write to the sharedmem every 15 samples regardless of sample_rate.
constexpr u64 GetBufferUpdatePeriod(SampleRate sample_rate) {
    return 15 * BASE_CLOCK_RATE_ARM11 / GetSampleRateInHz(sample_rate);
}

// Variables holding the current mic buffer writing state
struct State {
    std::weak_ptr<Kernel::SharedMemory> memory_ref{};
    u8* sharedmem_buffer = nullptr;
    u32 sharedmem_size = 0;
    std::size_t size = 0;
    u32 offset = 0;
    u32 initial_offset = 0;
    bool looped_buffer = false;
    u8 sample_size = 0;
    u8 gain = 0;
    bool power = false;
    SampleRate sample_rate = SampleRate::Rate16360;

    void WriteSamples(std::span<const u8> samples) {
        u32 bytes_total_written = 0;
        auto sample_buffer = sharedmem_buffer + initial_offset;
        // Do not let sampling buffer overrun shared memory space.
        const auto sample_buffer_size =
            std::min(size, sharedmem_size - initial_offset - sizeof(u32));

        // Write samples in a loop until the input runs out
        while (samples.size() > bytes_total_written) {
            // TODO: If the sample size is 16-bit, this could theoretically cut a sample in the case
            // where the application configures an odd size.
            std::size_t bytes_to_write =
                std::min(samples.size() - bytes_total_written, sample_buffer_size - offset);
            std::memcpy(sample_buffer + offset, samples.data() + bytes_total_written,
                        bytes_to_write);
            offset += static_cast<u32>(bytes_to_write);
            bytes_total_written += static_cast<u32>(bytes_to_write);

            if (offset >= sample_buffer_size && looped_buffer) {
                offset = 0;
            }

            if (!looped_buffer) {
                break;
            }
        }

        // The last 4 bytes of the shared memory contains the latest offset
        // so update that as well https://www.3dbrew.org/wiki/MIC_Shared_Memory
        u32_le off = offset;
        std::memcpy(sharedmem_buffer + (sharedmem_size - sizeof(u32)), reinterpret_cast<u8*>(&off),
                    sizeof(u32));
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        std::shared_ptr<Kernel::SharedMemory> _memory_ref = memory_ref.lock();
        ar & _memory_ref;
        memory_ref = _memory_ref;
        ar & sharedmem_size;
        ar & size;
        ar & offset;
        ar & initial_offset;
        ar & looped_buffer;
        ar & sample_size;
        ar & gain;
        ar & power;
        ar & sample_rate;
        sharedmem_buffer = _memory_ref ? _memory_ref->GetPointer() : nullptr;
    }
    friend class boost::serialization::access;
};

struct MIC_U::Impl {
    explicit Impl(Core::System& system) : system(system), timing(system.CoreTiming()) {
        buffer_full_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "MIC_U::buffer_full_event");
        buffer_write_event = timing.RegisterEvent(
            "MIC_U::UpdateBuffer", [this](std::uintptr_t user_data, s64 cycles_late) {
                UpdateSharedMemBuffer(user_data, cycles_late);
            });
    }

    void MapSharedMem(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        const u32 size = rp.Pop<u32>();
        shared_memory = rp.PopObject<Kernel::SharedMemory>();

        if (shared_memory) {
            shared_memory->SetName("MIC_U:shared_memory");
            state.memory_ref = shared_memory;
            state.sharedmem_buffer = shared_memory->GetPointer();
            state.sharedmem_size = size;
        }

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);

        LOG_TRACE(Service_MIC, "called, size=0x{:X}", size);
    }

    void UnmapSharedMem(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        shared_memory = nullptr;
        rb.Push(ResultSuccess);
        LOG_TRACE(Service_MIC, "called");
    }

    void UpdateSharedMemBuffer(std::uintptr_t user_data, s64 cycles_late) {
        // If the event was scheduled before the application requested the mic to stop sampling
        if (!mic || !mic->IsSampling()) {
            return;
        }

        if (change_mic_impl_requested.exchange(false)) {
            CreateMic();
        }

        AudioCore::Samples samples = mic->Read();
        if (!samples.empty()) {
            // write the samples to sharedmem page
            state.WriteSamples(samples);
        }

        // schedule next run
        timing.ScheduleEvent(GetBufferUpdatePeriod(state.sample_rate) - cycles_late,
                             buffer_write_event);
    }

    void StartSampling() {
        auto sign = encoding == Encoding::PCM8Signed || encoding == Encoding::PCM16Signed
                        ? AudioCore::Signedness::Signed
                        : AudioCore::Signedness::Unsigned;
        mic->StartSampling({sign, state.sample_size, state.looped_buffer,
                            GetSampleRateInHz(state.sample_rate), state.initial_offset,
                            static_cast<u32>(state.size)});
    }

    void StartSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);

        encoding = rp.PopEnum<Encoding>();
        SampleRate sample_rate = rp.PopEnum<SampleRate>();
        u32 audio_buffer_offset = rp.PopRaw<u32>();
        u32 audio_buffer_size = rp.Pop<u32>();
        bool audio_buffer_loop = rp.Pop<bool>();

        if (mic && mic->IsSampling()) {
            LOG_CRITICAL(Service_MIC,
                         "Application started sampling again before stopping sampling");
            mic->StopSampling();
            mic.reset();
        }

        u8 sample_size = encoding == Encoding::PCM8Signed || encoding == Encoding::PCM8 ? 8 : 16;
        state.offset = 0;
        state.initial_offset = audio_buffer_offset;
        state.sample_rate = sample_rate;
        state.sample_size = sample_size;
        state.looped_buffer = audio_buffer_loop;
        state.size = audio_buffer_size;

        CreateMic();
        StartSampling();

        timing.ScheduleEvent(GetBufferUpdatePeriod(state.sample_rate), buffer_write_event);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_TRACE(Service_MIC,
                  "called, encoding={}, sample_rate={}, "
                  "audio_buffer_offset={}, audio_buffer_size={}, audio_buffer_loop={}",
                  encoding, sample_rate, audio_buffer_offset, audio_buffer_size, audio_buffer_loop);
    }

    void AdjustSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        SampleRate sample_rate = rp.PopEnum<SampleRate>();
        state.sample_rate = sample_rate;
        if (mic) {
            mic->AdjustSampleRate(GetSampleRateInHz(sample_rate));
        }

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_TRACE(Service_MIC, "sample_rate={}", sample_rate);
    }

    void StopSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        timing.RemoveEvent(buffer_write_event);
        if (mic) {
            mic->StopSampling();
            mic.reset();
        }
        LOG_TRACE(Service_MIC, "called");
    }

    void IsSampling(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        bool is_sampling = mic && mic->IsSampling();
        rb.Push<bool>(is_sampling);
        LOG_TRACE(Service_MIC, "IsSampling: {}", is_sampling);
    }

    void GetBufferFullEvent(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(buffer_full_event);
        LOG_WARNING(Service_MIC, "(STUBBED) called");
    }

    void SetGain(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        u8 gain = rp.Pop<u8>();
        state.gain = gain;

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_TRACE(Service_MIC, "gain={}", gain);
    }

    void GetGain(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        rb.Push<u8>(state.gain);
        LOG_TRACE(Service_MIC, "gain={}", state.gain);
    }

    void SetPower(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        bool power = rp.Pop<bool>();
        state.power = power;

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_TRACE(Service_MIC, "mic_power={}", power);
    }

    void GetPower(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        rb.Push<u8>(state.power);
        LOG_TRACE(Service_MIC, "called");
    }

    void SetIirFilterMic(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        const u32 size = rp.Pop<u32>();
        const Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultSuccess);
        rb.PushMappedBuffer(buffer);
        LOG_WARNING(Service_MIC, "(STUBBED) called, size=0x{:X}, buffer=0x{:08X}", size,
                    buffer.GetId());
    }

    void SetClamp(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        clamp = rp.Pop<bool>();

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_WARNING(Service_MIC, "(STUBBED) called, clamp={}", clamp);
    }

    void GetClamp(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);

        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        rb.Push<bool>(clamp);
        LOG_WARNING(Service_MIC, "(STUBBED) called");
    }

    void SetAllowShellClosed(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        allow_shell_closed = rp.Pop<bool>();

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_WARNING(Service_MIC, "(STUBBED) called, allow_shell_closed={}", allow_shell_closed);
    }

    void SetClientVersion(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp(ctx);
        const u32 version = rp.Pop<u32>();
        LOG_WARNING(Service_MIC, "(STUBBED) called, version: 0x{:08X}", version);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
    }

    void CreateMic() {
        const auto was_sampling = mic && mic->IsSampling();
        if (was_sampling) {
            mic->StopSampling();
            mic.reset();
        }

        mic = AudioCore::GetInputDetails(Settings::values.input_type.GetValue())
                  .create_input(system, Settings::values.input_device.GetValue());
        if (was_sampling) {
            StartSampling();
        }

        change_mic_impl_requested.store(false);
    }

    std::atomic<bool> change_mic_impl_requested = false;
    std::shared_ptr<Kernel::Event> buffer_full_event;
    Core::TimingEventType* buffer_write_event = nullptr;
    std::shared_ptr<Kernel::SharedMemory> shared_memory;
    u32 client_version = 0;
    bool allow_shell_closed = false;
    bool clamp = false;
    std::unique_ptr<AudioCore::Input> mic;
    Core::System& system;
    Core::Timing& timing;
    State state{};
    Encoding encoding{};

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar & change_mic_impl_requested;
        ar & buffer_full_event;
        // buffer_write_event set in constructor
        ar & shared_memory;
        ar & client_version;
        ar & allow_shell_closed;
        ar & clamp;
        // mic interface set in constructor
        ar & state;
        // Maintain the internal mic state
        ar & encoding;
        bool is_sampling = mic && mic->IsSampling();
        ar & is_sampling;
        if (Archive::is_loading::value) {
            if (is_sampling) {
                CreateMic();
                StartSampling();
            } else if (mic) {
                mic->StopSampling();
            }
        }
    }
    friend class boost::serialization::access;
};

void MIC_U::MapSharedMem(Kernel::HLERequestContext& ctx) {
    impl->MapSharedMem(ctx);
}

void MIC_U::UnmapSharedMem(Kernel::HLERequestContext& ctx) {
    impl->UnmapSharedMem(ctx);
}

void MIC_U::StartSampling(Kernel::HLERequestContext& ctx) {
    impl->StartSampling(ctx);
}

void MIC_U::AdjustSampling(Kernel::HLERequestContext& ctx) {
    impl->AdjustSampling(ctx);
}

void MIC_U::StopSampling(Kernel::HLERequestContext& ctx) {
    impl->StopSampling(ctx);
}

void MIC_U::IsSampling(Kernel::HLERequestContext& ctx) {
    impl->IsSampling(ctx);
}

void MIC_U::GetBufferFullEvent(Kernel::HLERequestContext& ctx) {
    impl->GetBufferFullEvent(ctx);
}

void MIC_U::SetGain(Kernel::HLERequestContext& ctx) {
    impl->SetGain(ctx);
}

void MIC_U::GetGain(Kernel::HLERequestContext& ctx) {
    impl->GetGain(ctx);
}

void MIC_U::SetPower(Kernel::HLERequestContext& ctx) {
    impl->SetPower(ctx);
}

void MIC_U::GetPower(Kernel::HLERequestContext& ctx) {
    impl->GetPower(ctx);
}

void MIC_U::SetIirFilterMic(Kernel::HLERequestContext& ctx) {
    impl->SetIirFilterMic(ctx);
}

void MIC_U::SetClamp(Kernel::HLERequestContext& ctx) {
    impl->SetClamp(ctx);
}

void MIC_U::GetClamp(Kernel::HLERequestContext& ctx) {
    impl->GetClamp(ctx);
}

void MIC_U::SetAllowShellClosed(Kernel::HLERequestContext& ctx) {
    impl->SetAllowShellClosed(ctx);
}

void MIC_U::SetClientVersion(Kernel::HLERequestContext& ctx) {
    impl->SetClientVersion(ctx);
}

MIC_U::MIC_U(Core::System& system)
    : ServiceFramework{"mic:u", 1}, impl{std::make_unique<Impl>(system)} {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &MIC_U::MapSharedMem, "MapSharedMem"},
        {0x0002, &MIC_U::UnmapSharedMem, "UnmapSharedMem"},
        {0x0003, &MIC_U::StartSampling, "StartSampling"},
        {0x0004, &MIC_U::AdjustSampling, "AdjustSampling"},
        {0x0005, &MIC_U::StopSampling, "StopSampling"},
        {0x0006, &MIC_U::IsSampling, "IsSampling"},
        {0x0007, &MIC_U::GetBufferFullEvent, "GetBufferFullEvent"},
        {0x0008, &MIC_U::SetGain, "SetGain"},
        {0x0009, &MIC_U::GetGain, "GetGain"},
        {0x000A, &MIC_U::SetPower, "SetPower"},
        {0x000B, &MIC_U::GetPower, "GetPower"},
        {0x000C, &MIC_U::SetIirFilterMic, "SetIirFilterMic"},
        {0x000D, &MIC_U::SetClamp, "SetClamp"},
        {0x000E, &MIC_U::GetClamp, "GetClamp"},
        {0x000F, &MIC_U::SetAllowShellClosed, "SetAllowShellClosed"},
        {0x0010, &MIC_U::SetClientVersion, "SetClientVersion"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

MIC_U::~MIC_U() {
    if (impl->mic) {
        impl->mic->StopSampling();
        impl->mic.reset();
    }
}

void MIC_U::ReloadMic() {
    impl->change_mic_impl_requested.store(true);
}

void ReloadMic(Core::System& system) {
    auto micu = system.ServiceManager().GetService<Service::MIC::MIC_U>("mic:u");
    if (!micu)
        return;
    micu->ReloadMic();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<MIC_U>(system)->InstallAsService(service_manager);
}

} // namespace Service::MIC
