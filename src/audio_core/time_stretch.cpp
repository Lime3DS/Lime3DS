// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>
#include <SoundTouch.h>

#include "audio_core/audio_types.h"
#include "audio_core/time_stretch.h"
#include "common/assert.h"
#include "common/logging/log.h"

namespace AudioCore {

TimeStretcher::TimeStretcher() : sound_touch(std::make_unique<soundtouch::SoundTouch>()) {
    sound_touch->setChannels(2);
    sound_touch->setSampleRate(native_sample_rate);
    sound_touch->setPitch(1.0);
    sound_touch->setTempo(1.0);
}

TimeStretcher::~TimeStretcher() = default;

void TimeStretcher::SetOutputSampleRate(unsigned int sample_rate) {
    sound_touch->setSampleRate(sample_rate);
}

std::size_t TimeStretcher::Process(const s16* in, std::size_t num_in, s16* out,
                                   std::size_t num_out) {
    const double time_delta = static_cast<double>(num_out) / native_sample_rate; // seconds
    double current_ratio = static_cast<double>(num_in) / static_cast<double>(num_out);

    const double max_latency = 0.25; // seconds
    const double max_backlog = native_sample_rate * max_latency;
    const double backlog_fullness = sound_touch->numSamples() / max_backlog;
    if (backlog_fullness > 4.0) {
        // Too many samples in backlog: Don't push anymore on
        num_in = 0;
    }

    // We ideally want the backlog to be about 50% full.
    // This gives some headroom both ways to prevent underflow and overflow.
    // We tweak current_ratio to encourage this.
    constexpr double tweak_time_scale = 0.050; // seconds
    const double tweak_correction = (backlog_fullness - 0.5) * (time_delta / tweak_time_scale);
    current_ratio *= std::pow(1.0 + 2.0 * tweak_correction, tweak_correction < 0 ? 3.0 : 1.0);

    // This low-pass filter smoothes out variance in the calculated stretch ratio.
    // The time-scale determines how responsive this filter is.
    constexpr double lpf_time_scale = 0.712; // seconds
    const double lpf_gain = 1.0 - std::exp(-time_delta / lpf_time_scale);
    stretch_ratio += lpf_gain * (current_ratio - stretch_ratio);

    // Place a lower limit of 5% speed. When a game boots up, there will be
    // many silence samples. These do not need to be timestretched.
    stretch_ratio = std::max(stretch_ratio, 0.05);
    sound_touch->setTempo(stretch_ratio);

    LOG_TRACE(Audio, "{:5}/{:5} ratio:{:0.6f} backlog:{:0.6f}", num_in, num_out, stretch_ratio,
              backlog_fullness);

    if constexpr (std::is_floating_point<soundtouch::SAMPLETYPE>()) {
        // The SoundTouch library on most systems expects float samples
        // use this vector to store input if soundtouch::SAMPLETYPE is a float
        std::vector<soundtouch::SAMPLETYPE> float_in(2 * num_in);
        std::vector<soundtouch::SAMPLETYPE> float_out(2 * num_out);

        for (std::size_t i = 0; i < (2 * num_in); i++) {
            // Conventional integer PCM uses a range of -32768 to 32767,
            // but float samples use -1 to 1
            // As a result we need to scale sample values during conversion
            const float temp = static_cast<float>(in[i]) / std::numeric_limits<s16>::max();
            float_in[i] = static_cast<soundtouch::SAMPLETYPE>(temp);
        }

        sound_touch->putSamples(float_in.data(), static_cast<u32>(num_in));

        const std::size_t samples_received =
            sound_touch->receiveSamples(float_out.data(), static_cast<u32>(num_out));

        // Converting output samples back to shorts so we can use them
        for (std::size_t i = 0; i < (2 * num_out); i++) {
            const s16 temp = static_cast<s16>(float_out[i] * std::numeric_limits<s16>::max());
            out[i] = temp;
        }

        return samples_received;
    } else if (std::is_same<soundtouch::SAMPLETYPE, s16>()) {
        // Use reinterpret_cast to workaround compile error when SAMPLETYPE is float.
        sound_touch->putSamples(reinterpret_cast<const soundtouch::SAMPLETYPE*>(in),
                                static_cast<u32>(num_in));
        return sound_touch->receiveSamples(reinterpret_cast<soundtouch::SAMPLETYPE*>(out),
                                           static_cast<u32>(num_out));
    } else {
        static_assert(std::is_floating_point<soundtouch::SAMPLETYPE>() ||
                      std::is_same<soundtouch::SAMPLETYPE, s16>());
        UNREACHABLE_MSG("Invalid SAMPLETYPE {}", typeid(soundtouch::SAMPLETYPE).name());
        return 0;
    }
}

void TimeStretcher::Clear() {
    sound_touch->clear();
}

void TimeStretcher::Flush() {
    sound_touch->flush();
}

} // namespace AudioCore
