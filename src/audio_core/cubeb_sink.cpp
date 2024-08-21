// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdarg>
#include <mutex>
#include <vector>
#include <cubeb/cubeb.h>
#include "audio_core/audio_types.h"
#include "audio_core/cubeb_sink.h"
#include "common/logging/log.h"

namespace AudioCore {

struct CubebSink::Impl {
    cubeb* ctx = nullptr;
    cubeb_stream* stream = nullptr;

    std::function<void(s16*, std::size_t)> cb;

    static long DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                             void* output_buffer, long num_frames);
    static void StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state);
    static void LogCallback(char const* fmt, ...);
};

CubebSink::CubebSink(std::string_view target_device_name) : impl(std::make_unique<Impl>()) {
    if (cubeb_init(&impl->ctx, "Lime3DS Output", nullptr) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "cubeb_init failed");
        return;
    }

    if (cubeb_set_log_callback(CUBEB_LOG_NORMAL, &Impl::LogCallback) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "cubeb_set_log_callback failed");
        return;
    }

    cubeb_stream_params params = {
        .format = CUBEB_SAMPLE_S16LE,
        .rate = native_sample_rate,
        .channels = 2,
        .layout = CUBEB_LAYOUT_STEREO,
    };

    u32 minimum_latency = 100 * native_sample_rate / 1000; // Firefox default
    if (cubeb_get_min_latency(impl->ctx, &params, &minimum_latency) != CUBEB_OK) {
        LOG_WARNING(Audio_Sink,
                    "Error getting minimum output latency, falling back to default latency.");
    }

    cubeb_devid output_device = nullptr;
    if (target_device_name != auto_device_name && !target_device_name.empty()) {
        cubeb_device_collection collection;
        if (cubeb_enumerate_devices(impl->ctx, CUBEB_DEVICE_TYPE_OUTPUT, &collection) == CUBEB_OK) {
            const auto collection_end{collection.device + collection.count};
            const auto device{
                std::find_if(collection.device, collection_end, [&](const cubeb_device_info& info) {
                    return info.friendly_name != nullptr &&
                           target_device_name == info.friendly_name;
                })};
            if (device != collection_end) {
                output_device = device->devid;
            }
            cubeb_device_collection_destroy(impl->ctx, &collection);
        } else {
            LOG_WARNING(Audio_Sink,
                        "Audio output device enumeration not supported, using default device.");
        }
    }

    auto stream_err = cubeb_stream_init(impl->ctx, &impl->stream, "Lime3DSAudio", nullptr, nullptr,
                                        output_device, &params, std::max(512u, minimum_latency),
                                        &Impl::DataCallback, &Impl::StateCallback, impl.get());
    if (stream_err != CUBEB_OK) {
        switch (stream_err) {
        case CUBEB_ERROR:
        default:
            LOG_CRITICAL(Audio_Sink, "Error initializing cubeb stream ({})", stream_err);
            break;
        case CUBEB_ERROR_INVALID_FORMAT:
            LOG_CRITICAL(Audio_Sink, "Invalid format when initializing cubeb stream");
            break;
        case CUBEB_ERROR_DEVICE_UNAVAILABLE:
            LOG_CRITICAL(Audio_Sink, "Device unavailable when initializing cubeb stream");
            break;
        }
        return;
    }

    if (cubeb_stream_start(impl->stream) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "Error starting cubeb stream");
        return;
    }
}

CubebSink::~CubebSink() {
    if (impl->stream) {
        if (cubeb_stream_stop(impl->stream) != CUBEB_OK) {
            LOG_ERROR(Audio_Sink, "Error stopping cubeb stream.");
        }
        cubeb_stream_destroy(impl->stream);
    }

    if (impl->ctx) {
        cubeb_destroy(impl->ctx);
    }
}

unsigned int CubebSink::GetNativeSampleRate() const {
    return native_sample_rate;
}

void CubebSink::SetCallback(std::function<void(s16*, std::size_t)> cb) {
    impl->cb = cb;
}

long CubebSink::Impl::DataCallback(cubeb_stream* stream, void* user_data, const void* input_buffer,
                                   void* output_buffer, long num_frames) {
    auto* impl = static_cast<Impl*>(user_data);
    auto* buffer = static_cast<s16*>(output_buffer);

    if (!impl || !impl->cb) {
        LOG_DEBUG(Audio_Sink, "Missing internal data and/or audio callback, emitting zeroes.");
        std::memset(output_buffer, 0, num_frames * 2 * sizeof(s16));
    } else {
        impl->cb(buffer, num_frames);
    }

    return num_frames;
}

void CubebSink::Impl::StateCallback(cubeb_stream* stream, void* user_data, cubeb_state state) {
    switch (state) {
    case CUBEB_STATE_STARTED:
        LOG_INFO(Audio_Sink, "Cubeb Audio Stream Started");
        break;
    case CUBEB_STATE_STOPPED:
        LOG_INFO(Audio_Sink, "Cubeb Audio Stream Stopped");
        break;
    case CUBEB_STATE_DRAINED:
        LOG_INFO(Audio_Sink, "Cubeb Audio Stream Drained");
        break;
    case CUBEB_STATE_ERROR:
        LOG_CRITICAL(Audio_Sink, "Cubeb Audio Stream Errored");
        break;
    }
}

void CubebSink::Impl::LogCallback(char const* format, ...) {
    std::array<char, 512> buffer{};
    std::va_list args;
    va_start(args, format);
#ifdef _MSC_VER
    vsprintf_s(buffer.data(), buffer.size(), format, args);
#else
    vsnprintf(buffer.data(), buffer.size(), format, args);
#endif
    va_end(args);
    buffer.back() = '\0';
    LOG_DEBUG(Audio_Sink, "{}", buffer.data());
}

std::vector<std::string> ListCubebSinkDevices() {
    std::vector<std::string> device_list;
    cubeb* ctx;

    if (cubeb_init(&ctx, "Lime3DS Output Device Enumerator", nullptr) != CUBEB_OK) {
        LOG_CRITICAL(Audio_Sink, "cubeb_init failed");
        return {};
    }

    cubeb_device_collection collection;
    if (cubeb_enumerate_devices(ctx, CUBEB_DEVICE_TYPE_OUTPUT, &collection) == CUBEB_OK) {
        for (std::size_t i = 0; i < collection.count; i++) {
            const cubeb_device_info& device = collection.device[i];
            if (device.state == CUBEB_DEVICE_STATE_ENABLED && device.friendly_name) {
                device_list.emplace_back(device.friendly_name);
            }
        }
        cubeb_device_collection_destroy(ctx, &collection);
    } else {
        LOG_WARNING(Audio_Sink, "Audio output device enumeration not supported.");
    }

    cubeb_destroy(ctx);
    return device_list;
}

} // namespace AudioCore
