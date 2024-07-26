// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "audio_core/audio_types.h"
#include "common/common_types.h"

namespace AudioCore::Codec {

/// See: Codec::DecodeADPCM
struct ADPCMState {
    // Two historical samples from previous processed buffer,
    // required for ADPCM decoding
    s16 yn1; ///< y[n-1]
    s16 yn2; ///< y[n-2]
};

/**
 * @param data Pointer to buffer that contains ADPCM data to decode
 * @param sample_count Length of buffer in terms of number of samples
 * @param adpcm_coeff ADPCM coefficients
 * @param state ADPCM state, this is updated with new state
 * @return Decoded stereo signed PCM16 data, sample_count in length
 */
StereoBuffer16 DecodeADPCM(const u8* data, const std::size_t sample_count,
                           const std::array<s16, 16>& adpcm_coeff, ADPCMState& state);

/**
 * @param num_channels Number of channels
 * @param data Pointer to buffer that contains PCM8 data to decode
 * @param sample_count Length of buffer in terms of number of samples
 * @return Decoded stereo signed PCM16 data, sample_count in length
 */
StereoBuffer16 DecodePCM8(const unsigned num_channels, const u8* const data,
                          const std::size_t sample_count);

/**
 * @param num_channels Number of channels
 * @param data Pointer to buffer that contains PCM16 data to decode
 * @param sample_count Length of buffer in terms of number of samples
 * @return Decoded stereo signed PCM16 data, sample_count in length
 */
StereoBuffer16 DecodePCM16(const unsigned num_channels, const u8* const data,
                           const std::size_t sample_count);
} // namespace AudioCore::Codec
