// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/weak_ptr.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/hle/aac_decoder.h"
#include "audio_core/hle/common.h"
#include "audio_core/hle/decoder.h"
#include "audio_core/hle/hle.h"
#include "audio_core/hle/mixers.h"
#include "audio_core/hle/shared_memory.h"
#include "audio_core/hle/source.h"
#include "audio_core/sink.h"
#include "common/archives.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/hash.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"

using InterruptType = Service::DSP::InterruptType;

namespace AudioCore {

DspHle::DspHle(Core::System& system) : DspHle(system, system.Memory(), system.CoreTiming()) {}

template <class Archive>
void DspHle::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<DspInterface>(*this);
    ar&* impl.get();
}
SERIALIZE_IMPL(DspHle)

// The value below is the "perfect" mathematical ratio of ARM11 cycles per audio frame, samples per
// frame * teaklite cycles per sample * 2 ARM11 cycles/teaklite cycle
// (160 * 4096 * 2) = (1310720)
//
// This value has been verified against a rough hardware test with hardware and LLE
static constexpr u64 audio_frame_ticks = samples_per_frame * 4096 * 2ull; ///< Units: ARM11 cycles

struct DspHle::Impl final {
public:
    explicit Impl(DspHle& parent, Memory::MemorySystem& memory, Core::Timing& timing);
    ~Impl();

    DspState GetDspState() const;

    u16 RecvData(u32 register_number);
    bool RecvDataIsReady(u32 register_number) const;
    std::vector<u8> PipeRead(DspPipe pipe_number, std::size_t length);
    std::size_t GetPipeReadableSize(DspPipe pipe_number) const;
    void PipeWrite(DspPipe pipe_number, std::span<const u8> buffer);

    std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory();

    void SetInterruptHandler(
        std::function<void(Service::DSP::InterruptType type, DspPipe pipe)> handler);

private:
    void ResetPipes();
    void WriteU16(DspPipe pipe_number, u16 value);
    void AudioPipeWriteStructAddresses();

    std::size_t CurrentRegionIndex() const;
    HLE::SharedMemory& ReadRegion();
    HLE::SharedMemory& WriteRegion();

    StereoFrame16 GenerateCurrentFrame();
    bool Tick();
    void AudioTickCallback(s64 cycles_late);

    DspState dsp_state = DspState::Off;
    std::array<std::vector<u8>, num_dsp_pipe> pipe_data{};

    HLE::DspMemory dsp_memory;
    std::array<HLE::Source, HLE::num_sources> sources{{
        HLE::Source(0),  HLE::Source(1),  HLE::Source(2),  HLE::Source(3),  HLE::Source(4),
        HLE::Source(5),  HLE::Source(6),  HLE::Source(7),  HLE::Source(8),  HLE::Source(9),
        HLE::Source(10), HLE::Source(11), HLE::Source(12), HLE::Source(13), HLE::Source(14),
        HLE::Source(15), HLE::Source(16), HLE::Source(17), HLE::Source(18), HLE::Source(19),
        HLE::Source(20), HLE::Source(21), HLE::Source(22), HLE::Source(23),
    }};
    HLE::Mixers mixers{};

    DspHle& parent;
    Core::Timing& core_timing;
    Core::TimingEventType* tick_event{};

    std::unique_ptr<HLE::DecoderBase> aac_decoder{};

    std::function<void(Service::DSP::InterruptType type, DspPipe pipe)> interrupt_handler{};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & dsp_state;
        ar & pipe_data;
        ar & dsp_memory.raw_memory;
        ar & sources;
        ar & mixers;
        // interrupt_handler is reregistered when loading state from DSP_DSP
    }
    friend class boost::serialization::access;
};

DspHle::Impl::Impl(DspHle& parent_, Memory::MemorySystem& memory, Core::Timing& timing)
    : parent(parent_), core_timing(timing) {
    dsp_memory.raw_memory.fill(0);

    for (auto& source : sources) {
        source.SetMemory(memory);
    }

    aac_decoder = std::make_unique<HLE::AACDecoder>(memory);
    tick_event =
        core_timing.RegisterEvent("AudioCore::DspHle::tick_event", [this](u64, s64 cycles_late) {
            this->AudioTickCallback(cycles_late);
        });
    core_timing.ScheduleEvent(audio_frame_ticks, tick_event);
}

DspHle::Impl::~Impl() {
    core_timing.UnscheduleEvent(tick_event, 0);
}

DspState DspHle::Impl::GetDspState() const {
    return dsp_state;
}

u16 DspHle::Impl::RecvData(u32 register_number) {
    ASSERT_MSG(register_number == 0, "Unknown register_number {}", register_number);

    // Application reads this after requesting DSP shutdown, to verify the DSP has indeed shutdown
    // or slept.

    switch (GetDspState()) {
    case AudioCore::DspState::On:
        return 0;
    case AudioCore::DspState::Off:
    case AudioCore::DspState::Sleeping:
        return 1;
    default:
        UNREACHABLE();
        break;
    }
}

bool DspHle::Impl::RecvDataIsReady(u32 register_number) const {
    ASSERT_MSG(register_number == 0, "Unknown register_number {}", register_number);
    return true;
}

std::vector<u8> DspHle::Impl::PipeRead(DspPipe pipe_number, std::size_t length) {
    const std::size_t pipe_index = static_cast<std::size_t>(pipe_number);

    if (pipe_index >= num_dsp_pipe) {
        LOG_ERROR(Audio_DSP, "pipe_number = {} invalid", pipe_index);
        return {};
    }

    if (length > UINT16_MAX) { // Can only read at most UINT16_MAX from the pipe
        LOG_ERROR(Audio_DSP, "length of {} greater than max of {}", length, UINT16_MAX);
        return {};
    }

    std::vector<u8>& data = pipe_data[pipe_index];

    if (length > data.size()) {
        LOG_WARNING(
            Audio_DSP,
            "pipe_number = {} is out of data, application requested read of {} but {} remain",
            pipe_index, length, data.size());
        length = static_cast<u32>(data.size());
    }

    if (length == 0)
        return {};

    std::vector<u8> ret(data.begin(), data.begin() + length);
    data.erase(data.begin(), data.begin() + length);
    return ret;
}

size_t DspHle::Impl::GetPipeReadableSize(DspPipe pipe_number) const {
    const std::size_t pipe_index = static_cast<std::size_t>(pipe_number);

    if (pipe_index >= num_dsp_pipe) {
        LOG_ERROR(Audio_DSP, "pipe_number = {} invalid", pipe_index);
        return 0;
    }

    return pipe_data[pipe_index].size();
}

void DspHle::Impl::PipeWrite(DspPipe pipe_number, std::span<const u8> buffer) {
    switch (pipe_number) {
    case DspPipe::Audio: {
        if (buffer.size() != 4) {
            LOG_ERROR(Audio_DSP, "DspPipe::Audio: Unexpected buffer length {} was written",
                      buffer.size());
            return;
        }

        enum class StateChange {
            Initialize = 0,
            Shutdown = 1,
            Wakeup = 2,
            Sleep = 3,
        };

        // The difference between Initialize and Wakeup is that Input state is maintained
        // when sleeping but isn't when turning it off and on again. (TODO: Implement this.)
        // Waking up from sleep garbles some of the structs in the memory region. (TODO:
        // Implement this.) Applications store away the state of these structs before
        // sleeping and reset it back after wakeup on behalf of the DSP.

        switch (static_cast<StateChange>(buffer[0])) {
        case StateChange::Initialize:
            LOG_INFO(Audio_DSP, "Application has requested initialization of DSP hardware");
            ResetPipes();
            AudioPipeWriteStructAddresses();
            dsp_state = DspState::On;
            break;
        case StateChange::Shutdown:
            LOG_INFO(Audio_DSP, "Application has requested shutdown of DSP hardware");
            dsp_state = DspState::Off;
            break;
        case StateChange::Wakeup:
            LOG_INFO(Audio_DSP, "Application has requested wakeup of DSP hardware");
            ResetPipes();
            AudioPipeWriteStructAddresses();
            dsp_state = DspState::On;
            break;
        case StateChange::Sleep:
            LOG_INFO(Audio_DSP, "Application has requested sleep of DSP hardware");
            UNIMPLEMENTED();
            AudioPipeWriteStructAddresses();
            dsp_state = DspState::Sleeping;
            break;
        default:
            LOG_ERROR(Audio_DSP,
                      "Application has requested unknown state transition of DSP hardware {}",
                      buffer[0]);
            dsp_state = DspState::Off;
            break;
        }

        return;
    }
    case DspPipe::Binary: {
        // TODO(B3N30): Make this async, and signal the interrupt
        HLE::BinaryMessage request{};
        if (sizeof(request) != buffer.size()) {
            LOG_CRITICAL(Audio_DSP, "got binary pipe with wrong size {}", buffer.size());
            UNIMPLEMENTED();
            return;
        }
        std::memcpy(&request, buffer.data(), buffer.size());
        if (request.header.codec != HLE::DecoderCodec::DecodeAAC) {
            LOG_CRITICAL(Audio_DSP, "got unknown codec {}", static_cast<u16>(request.header.codec));
            UNIMPLEMENTED();
            return;
        }
        const HLE::BinaryMessage response = aac_decoder->ProcessRequest(request);
        pipe_data[static_cast<u32>(pipe_number)].resize(sizeof(response));
        std::memcpy(pipe_data[static_cast<u32>(pipe_number)].data(), &response, sizeof(response));

        interrupt_handler(InterruptType::Pipe, DspPipe::Binary);
        break;
    }
    default:
        LOG_CRITICAL(Audio_DSP, "pipe_number = {} unimplemented",
                     static_cast<std::size_t>(pipe_number));
        UNIMPLEMENTED();
        return;
    }
}

std::array<u8, Memory::DSP_RAM_SIZE>& DspHle::Impl::GetDspMemory() {
    return dsp_memory.raw_memory;
}

void DspHle::Impl::SetInterruptHandler(
    std::function<void(Service::DSP::InterruptType type, DspPipe pipe)> handler) {
    interrupt_handler = handler;
}

void DspHle::Impl::ResetPipes() {
    for (auto& data : pipe_data) {
        data.clear();
    }
    dsp_state = DspState::Off;
}

void DspHle::Impl::WriteU16(DspPipe pipe_number, u16 value) {
    const std::size_t pipe_index = static_cast<std::size_t>(pipe_number);

    std::vector<u8>& data = pipe_data.at(pipe_index);
    // Little endian
    data.emplace_back(value & 0xFF);
    data.emplace_back(value >> 8);
}

void DspHle::Impl::AudioPipeWriteStructAddresses() {
    // These struct addresses are DSP dram addresses.
    // See also: DSP_DSP::ConvertProcessAddressFromDspDram
    static const std::array<u16, 15> struct_addresses = {
        0x8000 + offsetof(HLE::SharedMemory, frame_counter) / 2,
        0x8000 + offsetof(HLE::SharedMemory, source_configurations) / 2,
        0x8000 + offsetof(HLE::SharedMemory, source_statuses) / 2,
        0x8000 + offsetof(HLE::SharedMemory, adpcm_coefficients) / 2,
        0x8000 + offsetof(HLE::SharedMemory, dsp_configuration) / 2,
        0x8000 + offsetof(HLE::SharedMemory, dsp_status) / 2,
        0x8000 + offsetof(HLE::SharedMemory, final_samples) / 2,
        0x8000 + offsetof(HLE::SharedMemory, intermediate_mix_samples) / 2,
        0x8000 + offsetof(HLE::SharedMemory, compressor) / 2,
        0x8000 + offsetof(HLE::SharedMemory, dsp_debug) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown10) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown11) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown12) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown13) / 2,
        0x8000 + offsetof(HLE::SharedMemory, unknown14) / 2,
    };

    // Begin with a u16 denoting the number of structs.
    WriteU16(DspPipe::Audio, static_cast<u16>(struct_addresses.size()));
    // Then write the struct addresses.
    for (u16 addr : struct_addresses) {
        WriteU16(DspPipe::Audio, addr);
    }
    // Signal that we have data on this pipe.
    interrupt_handler(InterruptType::Pipe, DspPipe::Audio);
}

size_t DspHle::Impl::CurrentRegionIndex() const {
    // The region with the higher frame counter is chosen unless there is wraparound.
    // This function only returns a 0 or 1.
    const u16 frame_counter_0 = dsp_memory.region_0.frame_counter;
    const u16 frame_counter_1 = dsp_memory.region_1.frame_counter;

    if (frame_counter_0 == 0xFFFFu && frame_counter_1 != 0xFFFEu) {
        // Wraparound has occurred.
        return 1;
    }

    if (frame_counter_1 == 0xFFFFu && frame_counter_0 != 0xFFFEu) {
        // Wraparound has occurred.
        return 0;
    }

    return (frame_counter_0 > frame_counter_1) ? 0 : 1;
}

HLE::SharedMemory& DspHle::Impl::ReadRegion() {
    return CurrentRegionIndex() == 0 ? dsp_memory.region_0 : dsp_memory.region_1;
}

HLE::SharedMemory& DspHle::Impl::WriteRegion() {
    return CurrentRegionIndex() != 0 ? dsp_memory.region_0 : dsp_memory.region_1;
}

StereoFrame16 DspHle::Impl::GenerateCurrentFrame() {
    HLE::SharedMemory& read = ReadRegion();
    HLE::SharedMemory& write = WriteRegion();

    std::array<QuadFrame32, 3> intermediate_mixes = {};

    // Generate intermediate mixes
    for (std::size_t i = 0; i < HLE::num_sources; i++) {
        write.source_statuses.status[i] =
            sources[i].Tick(read.source_configurations.config[i], read.adpcm_coefficients.coeff[i]);
        for (std::size_t mix = 0; mix < 3; mix++) {
            sources[i].MixInto(intermediate_mixes[mix], mix);
        }
    }

    // Generate final mix
    write.dsp_status = mixers.Tick(read.dsp_configuration, read.intermediate_mix_samples,
                                   write.intermediate_mix_samples, intermediate_mixes);

    StereoFrame16 output_frame = mixers.GetOutput();

    // Write current output frame to the shared memory region
    for (std::size_t samplei = 0; samplei < output_frame.size(); samplei++) {
        for (std::size_t channeli = 0; channeli < output_frame[0].size(); channeli++) {
            write.final_samples.pcm16[samplei][channeli] = s16_le(output_frame[samplei][channeli]);
        }
    }

    return output_frame;
}

bool DspHle::Impl::Tick() {
    StereoFrame16 current_frame = {};

    // TODO: Check dsp::DSP semaphore (which indicates emulated application has finished writing to
    // shared memory region)
    current_frame = GenerateCurrentFrame();

    parent.OutputFrame(std::move(current_frame));

    return GetDspState() == DspState::On;
}

void DspHle::Impl::AudioTickCallback(s64 cycles_late) {
    if (Tick()) {
        // TODO(merry): Signal all the other interrupts as appropriate.
        interrupt_handler(InterruptType::Pipe, DspPipe::Audio);
    }

    // Reschedule recurrent event
    const double time_scale =
        Settings::values.enable_realtime_audio
            ? std::clamp(Core::System::GetInstance().GetStableFrameTimeScale(),
                         100. / Settings::values.frame_limit.GetValue(), 3.0)
            : 1.0;
    s64 adjusted_ticks = static_cast<s64>(audio_frame_ticks / time_scale - cycles_late);
    core_timing.ScheduleEvent(adjusted_ticks, tick_event);
}

DspHle::DspHle(Core::System& system, Memory::MemorySystem& memory, Core::Timing& timing)
    : DspInterface(system), impl(std::make_unique<Impl>(*this, memory, timing)) {}
DspHle::~DspHle() = default;

u16 DspHle::RecvData(u32 register_number) {
    return impl->RecvData(register_number);
}

bool DspHle::RecvDataIsReady(u32 register_number) const {
    return impl->RecvDataIsReady(register_number);
}

void DspHle::SetSemaphore(u16 semaphore_value) {
    // Do nothing in HLE
}

std::vector<u8> DspHle::PipeRead(DspPipe pipe_number, std::size_t length) {
    return impl->PipeRead(pipe_number, length);
}

size_t DspHle::GetPipeReadableSize(DspPipe pipe_number) const {
    return impl->GetPipeReadableSize(pipe_number);
}

void DspHle::PipeWrite(DspPipe pipe_number, std::span<const u8> buffer) {
    impl->PipeWrite(pipe_number, buffer);
}

std::array<u8, Memory::DSP_RAM_SIZE>& DspHle::GetDspMemory() {
    return impl->GetDspMemory();
}

void DspHle::SetInterruptHandler(
    std::function<void(Service::DSP::InterruptType type, DspPipe pipe)> handler) {
    impl->SetInterruptHandler(handler);
};

void DspHle::LoadComponent(std::span<const u8> component_data) {
    // HLE doesn't need DSP program. Only log some info here
    LOG_INFO(Service_DSP, "Firmware hash: {:#018x}",
             Common::ComputeHash64(component_data.data(), component_data.size()));
    // Some versions of the firmware have the location of DSP structures listed here.
    if (component_data.size() > 0x37C) {
        LOG_INFO(Service_DSP, "Structures hash: {:#018x}",
                 Common::ComputeHash64(component_data.data() + 0x340, 60));
    }
}

void DspHle::UnloadComponent() {
    // Do nothing
}

} // namespace AudioCore
