// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include "audio_core/audio_types.h"
#include "audio_core/sink.h"

namespace AudioCore {

class NullSink final : public Sink {
public:
    explicit NullSink(std::string_view) {}
    ~NullSink() override = default;

    unsigned int GetNativeSampleRate() const override {
        return native_sample_rate;
    }

    void SetCallback(std::function<void(s16*, std::size_t)>) override {}
};

} // namespace AudioCore
