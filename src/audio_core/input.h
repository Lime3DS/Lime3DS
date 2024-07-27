// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

namespace AudioCore {

enum class Signedness : u8 {
    Signed,
    Unsigned,
};

using Samples = std::vector<u8>;

struct InputParameters {
    Signedness sign;
    u8 sample_size;
    bool buffer_loop;
    u32 sample_rate;
    u32 buffer_offset;
    u32 buffer_size;
};

class Input {
public:
    Input() = default;

    virtual ~Input() = default;

    /// Starts the microphone. Called by Core
    virtual void StartSampling(const InputParameters& params) = 0;

    /// Stops the microphone. Called by Core
    virtual void StopSampling() = 0;

    /// Checks whether the microphone is currently sampling.
    virtual bool IsSampling() = 0;

    /**
     * Adjusts the Parameters. Implementations should update the parameters field in addition to
     * changing the mic to sample according to the new parameters. Called by Core
     */
    virtual void AdjustSampleRate(u32 sample_rate) = 0;

    /**
     * Called from the actual event timing at a constant period under a given sample rate.
     * When sampling is enabled this function is expected to return a buffer of 16 samples in ideal
     * conditions, but can be lax if the data is coming in from another source like a real mic.
     */
    virtual Samples Read() = 0;

protected:
    InputParameters parameters;
};

} // namespace AudioCore
