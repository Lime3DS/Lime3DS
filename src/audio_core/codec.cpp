// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include "audio_core/audio_types.h"
#include "audio_core/codec.h"
#include "common/assert.h"
#include "common/common_types.h"

namespace AudioCore::Codec {

StereoBuffer16 DecodeADPCM(const u8* const data, const std::size_t sample_count,
                           const std::array<s16, 16>& adpcm_coeff, ADPCMState& state) {
    // GC-ADPCM with scale factor and variable coefficients.
    // Frames are 8 bytes long containing 14 samples each.
    // Samples are 4 bits (one nibble) long.

    constexpr std::size_t FRAME_LEN = 8;
    constexpr std::size_t SAMPLES_PER_FRAME = 14;
    static constexpr std::array<int, 16> SIGNED_NIBBLES{
        0, 1, 2, 3, 4, 5, 6, 7, -8, -7, -6, -5, -4, -3, -2, -1,
    };

    const std::size_t ret_size =
        sample_count % 2 == 0 ? sample_count : sample_count + 1; // Ensure multiple of two.
    StereoBuffer16 ret(ret_size);

    int yn1 = state.yn1, yn2 = state.yn2;

    const std::size_t NUM_FRAMES =
        (sample_count + (SAMPLES_PER_FRAME - 1)) / SAMPLES_PER_FRAME; // Round up.
    for (std::size_t framei = 0; framei < NUM_FRAMES; framei++) {
        const int frame_header = data[framei * FRAME_LEN];
        const int scale = 1 << (frame_header & 0xF);
        const int idx = (frame_header >> 4) & 0x7;

        // Coefficients are fixed point with 11 bits fractional part.
        const int coef1 = adpcm_coeff[idx * 2 + 0];
        const int coef2 = adpcm_coeff[idx * 2 + 1];

        // Decodes an audio sample. One nibble produces one sample.
        const auto decode_sample = [&](const int nibble) -> s16 {
            const int xn = nibble * scale;
            // We first transform everything into 11 bit fixed point, perform the second order
            // digital filter, then transform back.
            // 0x400 == 0.5 in 11 bit fixed point.
            // Filter: y[n] = x[n] + 0.5 + c1 * y[n-1] + c2 * y[n-2]
            int val = ((xn << 11) + 0x400 + coef1 * yn1 + coef2 * yn2) >> 11;
            // Clamp to output range.
            val = std::clamp(val, -32768, 32767);
            // Advance output feedback.
            yn2 = yn1;
            yn1 = val;
            return (s16)val;
        };

        std::size_t outputi = framei * SAMPLES_PER_FRAME;
        std::size_t datai = framei * FRAME_LEN + 1;
        for (std::size_t i = 0; i < SAMPLES_PER_FRAME && outputi < sample_count; i += 2) {
            const s16 sample1 = decode_sample(SIGNED_NIBBLES[data[datai] >> 4]);
            ret[outputi].fill(sample1);
            outputi++;

            const s16 sample2 = decode_sample(SIGNED_NIBBLES[data[datai] & 0xF]);
            ret[outputi].fill(sample2);
            outputi++;

            datai++;
        }
    }

    state.yn1 = static_cast<s16>(yn1);
    state.yn2 = static_cast<s16>(yn2);

    return ret;
}

StereoBuffer16 DecodePCM8(const unsigned num_channels, const u8* const data,
                          const std::size_t sample_count) {
    ASSERT(num_channels == 1 || num_channels == 2);

    const auto decode_sample = [](u8 sample) {
        return static_cast<s16>(static_cast<u16>(sample) << 8);
    };

    StereoBuffer16 ret(sample_count);

    if (num_channels == 1) {
        for (std::size_t i = 0; i < sample_count; i++) {
            ret[i].fill(decode_sample(data[i]));
        }
    } else {
        for (std::size_t i = 0; i < sample_count; i++) {
            ret[i][0] = decode_sample(data[i * 2 + 0]);
            ret[i][1] = decode_sample(data[i * 2 + 1]);
        }
    }

    return ret;
}

StereoBuffer16 DecodePCM16(const unsigned num_channels, const u8* const data,
                           const std::size_t sample_count) {
    ASSERT(num_channels == 1 || num_channels == 2);

    StereoBuffer16 ret(sample_count);

    if (num_channels == 1) {
        for (std::size_t i = 0; i < sample_count; i++) {
            s16 sample;
            std::memcpy(&sample, data + i * sizeof(s16), sizeof(s16));
            ret[i].fill(sample);
        }
    } else {
        for (std::size_t i = 0; i < sample_count; ++i) {
            std::memcpy(&ret[i], data + i * sizeof(s16) * 2, 2 * sizeof(s16));
        }
    }

    return ret;
}
} // namespace AudioCore::Codec
