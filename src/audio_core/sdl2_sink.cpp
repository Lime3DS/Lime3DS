// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <vector>
#include <SDL.h>
#include "audio_core/audio_types.h"
#include "audio_core/sdl2_sink.h"
#include "common/assert.h"
#include "common/logging/log.h"

namespace AudioCore {

struct SDL2Sink::Impl {
    unsigned int sample_rate = 0;

    SDL_AudioDeviceID audio_device_id = 0;

    std::function<void(s16*, std::size_t)> cb;

    static void Callback(void* impl_, u8* buffer, int buffer_size_in_bytes);
};

SDL2Sink::SDL2Sink(std::string device_name) : impl(std::make_unique<Impl>()) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        LOG_CRITICAL(Audio_Sink, "SDL_Init(SDL_INIT_AUDIO) failed with: {}", SDL_GetError());
        impl->audio_device_id = 0;
        return;
    }

    SDL_AudioSpec desired_audiospec;
    SDL_zero(desired_audiospec);
    desired_audiospec.format = AUDIO_S16;
    desired_audiospec.channels = 2;
    desired_audiospec.freq = native_sample_rate;
    desired_audiospec.samples = 512;
    desired_audiospec.userdata = impl.get();
    desired_audiospec.callback = &Impl::Callback;

    SDL_AudioSpec obtained_audiospec;
    SDL_zero(obtained_audiospec);

    const char* device = nullptr;
    if (device_name != auto_device_name && !device_name.empty()) {
        device = device_name.c_str();
    }

    impl->audio_device_id =
        SDL_OpenAudioDevice(device, false, &desired_audiospec, &obtained_audiospec, 0);
    if (impl->audio_device_id <= 0) {
        LOG_CRITICAL(Audio_Sink, "SDL_OpenAudioDevice failed with code {} for device \"{}\"",
                     impl->audio_device_id, device_name);
        return;
    }

    impl->sample_rate = obtained_audiospec.freq;

    // SDL2 audio devices start out paused, unpause it:
    SDL_PauseAudioDevice(impl->audio_device_id, 0);
}

SDL2Sink::~SDL2Sink() {
    if (impl->audio_device_id <= 0)
        return;

    SDL_CloseAudioDevice(impl->audio_device_id);
}

unsigned int SDL2Sink::GetNativeSampleRate() const {
    if (impl->audio_device_id <= 0)
        return native_sample_rate;

    return impl->sample_rate;
}

void SDL2Sink::SetCallback(std::function<void(s16*, std::size_t)> cb) {
    impl->cb = cb;
}

void SDL2Sink::Impl::Callback(void* impl_, u8* buffer, int buffer_size_in_bytes) {
    Impl* impl = reinterpret_cast<Impl*>(impl_);
    if (!impl || !impl->cb)
        return;

    const std::size_t num_frames = buffer_size_in_bytes / (2 * sizeof(s16));

    impl->cb(reinterpret_cast<s16*>(buffer), num_frames);
}

std::vector<std::string> ListSDL2SinkDevices() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        LOG_CRITICAL(Audio_Sink, "SDL_InitSubSystem failed with: {}", SDL_GetError());
        return {};
    }

    std::vector<std::string> device_list;
    const int device_count = SDL_GetNumAudioDevices(0);
    for (int i = 0; i < device_count; ++i) {
        device_list.push_back(SDL_GetAudioDeviceName(i, 0));
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    return device_list;
}

} // namespace AudioCore
