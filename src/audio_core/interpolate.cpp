// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "audio_core/interpolate.h"
#include "common/assert.h"

namespace AudioCore::AudioInterp {

// Calculations are done in fixed point with 24 fractional bits.
// (This is not verified. This was chosen for minimal error.)
constexpr u64 scale_factor = 1 << 24;
constexpr u64 scale_mask = scale_factor - 1;

/// Here we step over the input in steps of rate, until we consume all of the input.
/// Three adjacent samples are passed to fn each step.
template <typename Function>
static void StepOverSamples(State& state, StereoBuffer16& input, float rate, StereoFrame16& output,
                            std::size_t& outputi, Function fn) {
    ASSERT(rate > 0);

    if (input.empty())
        return;

    input.insert(input.begin(), {state.xn2, state.xn1});

    const u64 step_size = static_cast<u64>(rate * scale_factor);
    u64 fposition = state.fposition;
    std::size_t inputi = 0;

    while (outputi < output.size()) {
        inputi = static_cast<std::size_t>(fposition / scale_factor);

        if (inputi + 2 >= input.size()) {
            inputi = input.size() - 2;
            break;
        }

        u64 fraction = fposition & scale_mask;
        output[outputi++] = fn(fraction, input[inputi], input[inputi + 1], input[inputi + 2]);

        fposition += step_size;
    }

    state.xn2 = input[inputi];
    state.xn1 = input[inputi + 1];
    state.fposition = fposition - inputi * scale_factor;

    input.erase(input.begin(), std::next(input.begin(), inputi + 2));
}

void None(State& state, StereoBuffer16& input, float rate, StereoFrame16& output,
          std::size_t& outputi) {
    StepOverSamples(
        state, input, rate, output, outputi,
        [](u64 fraction, const auto& x0, const auto& x1, const auto& x2) { return x0; });
}

void Linear(State& state, StereoBuffer16& input, float rate, StereoFrame16& output,
            std::size_t& outputi) {
    // Note on accuracy: Some values that this produces are +/- 1 from the actual firmware.
    StepOverSamples(state, input, rate, output, outputi,
                    [](u64 fraction, const auto& x0, const auto& x1, const auto& x2) {
                        // This is a saturated subtraction. (Verified by black-box fuzzing.)
                        s64 delta0 = std::clamp<s64>(x1[0] - x0[0], -32768, 32767);
                        s64 delta1 = std::clamp<s64>(x1[1] - x0[1], -32768, 32767);

                        return std::array<s16, 2>{
                            static_cast<s16>(x0[0] + fraction * delta0 / scale_factor),
                            static_cast<s16>(x0[1] + fraction * delta1 / scale_factor),
                        };
                    });
}

} // namespace AudioCore::AudioInterp
