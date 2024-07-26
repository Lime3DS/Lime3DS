// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include "audio_core/hle/mixers.h"
#include "common/assert.h"
#include "common/logging/log.h"

namespace AudioCore::HLE {

void Mixers::Reset() {
    current_frame.fill({});
    state = {};
}

DspStatus Mixers::Tick(DspConfiguration& config, const IntermediateMixSamples& read_samples,
                       IntermediateMixSamples& write_samples,
                       const std::array<QuadFrame32, 3>& input) {
    ParseConfig(config);

    AuxReturn(read_samples);
    AuxSend(write_samples, input);

    MixCurrentFrame();

    return GetCurrentStatus();
}

void Mixers::ParseConfig(DspConfiguration& config) {
    if (!config.dirty_raw) {
        return;
    }

    if (config.aux_bus_enable_0_dirty) {
        config.aux_bus_enable_0_dirty.Assign(0);
        state.aux_bus_enable[0] = config.aux_bus_enable[0] != 0;
        LOG_TRACE(Audio_DSP, "mixers aux_bus_enable[0] = {}", config.aux_bus_enable[0]);
    }

    if (config.aux_bus_enable_1_dirty) {
        config.aux_bus_enable_1_dirty.Assign(0);
        state.aux_bus_enable[1] = config.aux_bus_enable[1] != 0;
        LOG_TRACE(Audio_DSP, "mixers aux_bus_enable[1] = {}", config.aux_bus_enable[1]);
    }

    if (config.master_volume_dirty) {
        config.master_volume_dirty.Assign(0);
        state.intermediate_mixer_volume[0] = config.master_volume;
        LOG_TRACE(Audio_DSP, "mixers master_volume = {}", config.master_volume);
    }

    if (config.aux_return_volume_0_dirty) {
        config.aux_return_volume_0_dirty.Assign(0);
        state.intermediate_mixer_volume[1] = config.aux_return_volume[0];
        LOG_TRACE(Audio_DSP, "mixers aux_return_volume[0] = {}", config.aux_return_volume[0]);
    }

    if (config.aux_return_volume_1_dirty) {
        config.aux_return_volume_1_dirty.Assign(0);
        state.intermediate_mixer_volume[2] = config.aux_return_volume[1];
        LOG_TRACE(Audio_DSP, "mixers aux_return_volume[1] = {}", config.aux_return_volume[1]);
    }

    if (config.output_format_dirty) {
        config.output_format_dirty.Assign(0);
        state.output_format = config.output_format;
        LOG_TRACE(Audio_DSP, "mixers output_format = {}",
                  static_cast<std::size_t>(config.output_format));
    }

    if (config.headphones_connected_dirty) {
        config.headphones_connected_dirty.Assign(0);
        // Do nothing. (Note: Whether headphones are connected does affect coefficients used for
        // surround sound.)
        LOG_TRACE(Audio_DSP, "mixers headphones_connected={}", config.headphones_connected);
    }

    if (config.dirty_raw) {
        LOG_DEBUG(Audio_DSP, "mixers remaining_dirty={:x}", config.dirty_raw);
    }

    config.dirty_raw = 0;
}

static s16 ClampToS16(s32 value) {
    return static_cast<s16>(std::clamp(value, -32768, 32767));
}

static std::array<s16, 2> AddAndClampToS16(const std::array<s16, 2>& a,
                                           const std::array<s16, 2>& b) {
    return {ClampToS16(static_cast<s32>(a[0]) + static_cast<s32>(b[0])),
            ClampToS16(static_cast<s32>(a[1]) + static_cast<s32>(b[1]))};
}

void Mixers::DownmixAndMixIntoCurrentFrame(float gain, const QuadFrame32& samples) {
    // TODO(merry): Limiter. (Currently we're performing final mixing assuming a disabled limiter.)

    switch (state.output_format) {
    case OutputFormat::Mono:
        std::transform(
            current_frame.begin(), current_frame.end(), samples.begin(), current_frame.begin(),
            [gain](const std::array<s16, 2>& accumulator,
                   const std::array<s32, 4>& sample) -> std::array<s16, 2> {
                // Downmix to mono
                s16 mono = ClampToS16(static_cast<s32>(
                    (gain * sample[0] + gain * sample[1] + gain * sample[2] + gain * sample[3]) /
                    2));
                // Mix into current frame
                return AddAndClampToS16(accumulator, {mono, mono});
            });
        return;

    case OutputFormat::Surround:
        // TODO(merry): Implement surround sound.
        // fallthrough

    case OutputFormat::Stereo:
        std::transform(
            current_frame.begin(), current_frame.end(), samples.begin(), current_frame.begin(),
            [gain](const std::array<s16, 2>& accumulator,
                   const std::array<s32, 4>& sample) -> std::array<s16, 2> {
                // Downmix to stereo
                s16 left = ClampToS16(static_cast<s32>(gain * sample[0] + gain * sample[2]));
                s16 right = ClampToS16(static_cast<s32>(gain * sample[1] + gain * sample[3]));
                // Mix into current frame
                return AddAndClampToS16(accumulator, {left, right});
            });
        return;
    }

    UNREACHABLE_MSG("Invalid output_format {}", static_cast<std::size_t>(state.output_format));
}

void Mixers::AuxReturn(const IntermediateMixSamples& read_samples) {
    // NOTE: read_samples.mix{1,2}.pcm32 annoyingly have their dimensions in reverse order to
    // QuadFrame32.

    if (state.aux_bus_enable[0]) {
        for (std::size_t sample = 0; sample < samples_per_frame; sample++) {
            for (std::size_t channel = 0; channel < 4; channel++) {
                state.intermediate_mix_buffer[1][sample][channel] =
                    read_samples.mix1.pcm32[channel][sample];
            }
        }
    }

    if (state.aux_bus_enable[1]) {
        for (std::size_t sample = 0; sample < samples_per_frame; sample++) {
            for (std::size_t channel = 0; channel < 4; channel++) {
                state.intermediate_mix_buffer[2][sample][channel] =
                    read_samples.mix2.pcm32[channel][sample];
            }
        }
    }
}

void Mixers::AuxSend(IntermediateMixSamples& write_samples,
                     const std::array<QuadFrame32, 3>& input) {
    // NOTE: read_samples.mix{1,2}.pcm32 annoyingly have their dimensions in reverse order to
    // QuadFrame32.

    state.intermediate_mix_buffer[0] = input[0];

    if (state.aux_bus_enable[0]) {
        for (std::size_t sample = 0; sample < samples_per_frame; sample++) {
            for (std::size_t channel = 0; channel < 4; channel++) {
                write_samples.mix1.pcm32[channel][sample] = input[1][sample][channel];
            }
        }
    } else {
        state.intermediate_mix_buffer[1] = input[1];
    }

    if (state.aux_bus_enable[1]) {
        for (std::size_t sample = 0; sample < samples_per_frame; sample++) {
            for (std::size_t channel = 0; channel < 4; channel++) {
                write_samples.mix2.pcm32[channel][sample] = input[2][sample][channel];
            }
        }
    } else {
        state.intermediate_mix_buffer[2] = input[2];
    }
}

void Mixers::MixCurrentFrame() {
    current_frame.fill({});

    // TODO(SachinV): This is probably not accurate, based on symbols from FE:Fates,
    // state.intermediate_mixer_volume[0] represents the master volume
    for (std::size_t mix = 0; mix < 3; mix++) {
        DownmixAndMixIntoCurrentFrame(state.intermediate_mixer_volume[mix],
                                      state.intermediate_mix_buffer[mix]);
    }

    // TODO(merry): Compressor. (We currently assume a disabled compressor.)
}

DspStatus Mixers::GetCurrentStatus() const {
    DspStatus status;
    status.unknown = 0;
    status.dropped_frames = 0;
    return status;
}

} // namespace AudioCore::HLE
