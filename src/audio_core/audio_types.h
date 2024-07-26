// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include "common/common_types.h"

namespace AudioCore {

/// Samples per second which the 3DS's audio hardware natively outputs at
constexpr int native_sample_rate = 32728; // Hz

/// Samples per audio frame at native sample rate
constexpr int samples_per_frame = 160;

/// The final output to the speakers is stereo. Preprocessing output in Source is also stereo.
using StereoFrame16 = std::array<std::array<s16, 2>, samples_per_frame>;

/// The DSP is quadraphonic internally.
using QuadFrame32 = std::array<std::array<s32, 4>, samples_per_frame>;

/// A variable length buffer of signed PCM16 stereo samples.
using StereoBuffer16 = std::deque<std::array<s16, 2>>;

constexpr std::size_t num_dsp_pipe = 8;
enum class DspPipe {
    Debug = 0,
    Dma = 1,
    Audio = 2,
    Binary = 3,
};

enum class DspState {
    Off,
    On,
    Sleeping,
};

} // namespace AudioCore
