// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "audio_core/input.h"
#include "common/swap.h"
#include "common/threadsafe_queue.h"

namespace AudioCore {

class StaticInput final : public Input {
public:
    StaticInput();
    ~StaticInput() = default;

    void StartSampling(const InputParameters& params) {
        parameters = params;
        is_sampling = true;
    }

    void StopSampling() {
        is_sampling = false;
    }

    bool IsSampling() {
        return is_sampling;
    }

    void AdjustSampleRate(u32 sample_rate) {}

    Samples Read() {
        return (parameters.sample_size == 8) ? CACHE_8_BIT : CACHE_16_BIT;
    }

private:
    bool is_sampling = false;
    std::vector<u8> CACHE_8_BIT;
    std::vector<u8> CACHE_16_BIT;
};

} // namespace AudioCore
