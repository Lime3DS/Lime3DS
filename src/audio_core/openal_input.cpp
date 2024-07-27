// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include <vector>
#include <AL/al.h>
#include <AL/alc.h>
#include "audio_core/input.h"
#include "audio_core/openal_input.h"
#include "audio_core/sink.h"
#include "common/logging/log.h"

namespace AudioCore {

struct OpenALInput::Impl {
    ALCdevice* device = nullptr;
    u8 sample_size_in_bytes = 0;
};

OpenALInput::OpenALInput(std::string device_id)
    : impl(std::make_unique<Impl>()), device_id(std::move(device_id)) {}

OpenALInput::~OpenALInput() {
    StopSampling();
}

void OpenALInput::StartSampling(const InputParameters& params) {
    if (IsSampling()) {
        return;
    }

    // OpenAL supports unsigned 8-bit and signed 16-bit PCM.
    // TODO: Re-sample the stream.
    if ((params.sample_size == 8 && params.sign == Signedness::Signed) ||
        (params.sample_size == 16 && params.sign == Signedness::Unsigned)) {
        LOG_WARNING(Audio, "Application requested unsupported unsigned PCM format. Falling back to "
                           "supported format.");
    }

    parameters = params;
    impl->sample_size_in_bytes = params.sample_size / 8;

    auto format = params.sample_size == 16 ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
    impl->device = alcCaptureOpenDevice(
        device_id != auto_device_name && !device_id.empty() ? device_id.c_str() : nullptr,
        params.sample_rate, format, static_cast<ALsizei>(params.buffer_size));
    auto open_error = alcGetError(impl->device);
    if (impl->device == nullptr || open_error != ALC_NO_ERROR) {
        LOG_CRITICAL(Audio, "alcCaptureOpenDevice failed: {}", open_error);
        StopSampling();
        return;
    }

    alcCaptureStart(impl->device);
    auto capture_error = alcGetError(impl->device);
    if (capture_error != ALC_NO_ERROR) {
        LOG_CRITICAL(Audio, "alcCaptureStart failed: {}", capture_error);
        StopSampling();
        return;
    }
}

void OpenALInput::StopSampling() {
    if (impl->device) {
        alcCaptureStop(impl->device);
        alcCaptureCloseDevice(impl->device);
        impl->device = nullptr;
    }
}

bool OpenALInput::IsSampling() {
    return impl->device != nullptr;
}

void OpenALInput::AdjustSampleRate(u32 sample_rate) {
    if (!IsSampling()) {
        return;
    }

    auto new_params = parameters;
    new_params.sample_rate = sample_rate;
    StopSampling();
    StartSampling(new_params);
}

Samples OpenALInput::Read() {
    if (!IsSampling()) {
        return {};
    }

    ALCint samples_captured = 0;
    alcGetIntegerv(impl->device, ALC_CAPTURE_SAMPLES, 1, &samples_captured);
    auto error = alcGetError(impl->device);
    if (error != ALC_NO_ERROR) {
        LOG_WARNING(Audio, "alcGetIntegerv(ALC_CAPTURE_SAMPLES) failed: {}", error);
        return {};
    }

    auto num_samples = std::min(samples_captured, static_cast<ALsizei>(parameters.buffer_size /
                                                                       impl->sample_size_in_bytes));
    Samples samples(num_samples * impl->sample_size_in_bytes);

    alcCaptureSamples(impl->device, samples.data(), num_samples);
    error = alcGetError(impl->device);
    if (error != ALC_NO_ERROR) {
        LOG_WARNING(Audio, "alcCaptureSamples failed: {}", error);
        return {};
    }

    return samples;
}

std::vector<std::string> ListOpenALInputDevices() {
    const char* devices_str;
    if (alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") != AL_FALSE) {
        devices_str = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
    } else {
        LOG_WARNING(
            Audio,
            "Missing OpenAL device enumeration extensions, cannot list audio capture devices.");
        return {};
    }

    if (!devices_str || *devices_str == '\0') {
        return {};
    }

    std::vector<std::string> device_list;
    while (*devices_str != '\0') {
        device_list.emplace_back(devices_str);
        devices_str += strlen(devices_str) + 1;
    }
    return device_list;
}

} // namespace AudioCore
