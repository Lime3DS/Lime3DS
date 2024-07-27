// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/decoder.h"

namespace AudioCore::HLE {

DecoderSampleRate GetSampleRateEnum(u32 sample_rate) {
    switch (sample_rate) {
    case 48000:
        return DecoderSampleRate::Rate48000;
    case 44100:
        return DecoderSampleRate::Rate44100;
    case 32000:
        return DecoderSampleRate::Rate32000;
    case 24000:
        return DecoderSampleRate::Rate24000;
    case 22050:
        return DecoderSampleRate::Rate22050;
    case 16000:
        return DecoderSampleRate::Rate16000;
    case 12000:
        return DecoderSampleRate::Rate12000;
    case 11025:
        return DecoderSampleRate::Rate11025;
    case 8000:
        return DecoderSampleRate::Rate8000;
    default:
        LOG_WARNING(Audio_DSP, "Unknown decoder sample rate: {}", sample_rate);
        return DecoderSampleRate::Rate48000;
    }
}

} // namespace AudioCore::HLE
