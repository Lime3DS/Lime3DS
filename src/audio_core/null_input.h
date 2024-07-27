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

class NullInput final : public Input {
public:
    void StartSampling(const InputParameters& params) override {
        parameters = params;
        is_sampling = true;
    }

    void StopSampling() override {
        is_sampling = false;
    }

    bool IsSampling() override {
        return is_sampling;
    }

    void AdjustSampleRate(u32 sample_rate) override {}

    Samples Read() override {
        return {};
    }

private:
    bool is_sampling = false;
};

} // namespace AudioCore
