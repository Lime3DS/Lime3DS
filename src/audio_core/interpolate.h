// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include "audio_core/audio_types.h"
#include "common/common_types.h"

namespace AudioCore::AudioInterp {

/// A variable length buffer of signed PCM16 stereo samples.
using StereoBuffer16 = std::deque<std::array<s16, 2>>;

struct State {
    /// Two historical samples.
    std::array<s16, 2> xn1 = {}; ///< x[n-1]
    std::array<s16, 2> xn2 = {}; ///< x[n-2]
    /// Current fractional position.
    u64 fposition = 0;
};

/**
 * No interpolation. This is equivalent to a zero-order hold. There is a two-sample predelay.
 * @param state Interpolation state.
 * @param input Input buffer.
 * @param rate Stretch factor. Must be a positive non-zero value.
 *             rate > 1.0 performs decimation and rate < 1.0 performs upsampling.
 * @param output The resampled audio buffer.
 * @param outputi The index of output to start writing to.
 */
void None(State& state, StereoBuffer16& input, float rate, StereoFrame16& output,
          std::size_t& outputi);

/**
 * Linear interpolation. This is equivalent to a first-order hold. There is a two-sample predelay.
 * @param state Interpolation state.
 * @param input Input buffer.
 * @param rate Stretch factor. Must be a positive non-zero value.
 *             rate > 1.0 performs decimation and rate < 1.0 performs upsampling.
 * @param output The resampled audio buffer.
 * @param outputi The index of output to start writing to.
 */
void Linear(State& state, StereoBuffer16& input, float rate, StereoFrame16& output,
            std::size_t& outputi);

} // namespace AudioCore::AudioInterp
