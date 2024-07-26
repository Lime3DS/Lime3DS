// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include "common/common_types.h"
#include "common/dynamic_library/ffmpeg.h"
#include "common/thread.h"
#include "common/threadsafe_queue.h"
#include "core/dumping/backend.h"

namespace VideoCore {
class RendererBase;
}

namespace VideoDumper {

using VariableAudioFrame = std::vector<s16>;

class FFmpegMuxer;

/**
 * Wrapper around FFmpeg AVCodecContext + AVStream.
 * Rescales/Resamples, encodes and writes a frame.
 */
class FFmpegStream {
public:
    bool Init(FFmpegMuxer& muxer);
    void Free();
    void Flush();

protected:
    ~FFmpegStream();

    void WritePacket(AVPacket* packet);
    void SendFrame(AVFrame* frame);

    struct AVCodecContextDeleter {
        void operator()(AVCodecContext* codec_context) const {
            DynamicLibrary::FFmpeg::avcodec_free_context(&codec_context);
        }
    };

    struct AVFrameDeleter {
        void operator()(AVFrame* frame) const {
            DynamicLibrary::FFmpeg::av_frame_free(&frame);
        }
    };

    struct AVPacketDeleter {
        void operator()(AVPacket* packet) const {
            av_packet_free(&packet);
        }
    };

    AVFormatContext* format_context{};
    std::mutex* format_context_mutex{};
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codec_context{};
    AVStream* stream{};
};

/**
 * A FFmpegStream used for video data.
 * Filters (scales), encodes and writes a frame.
 */
class FFmpegVideoStream : public FFmpegStream {
public:
    ~FFmpegVideoStream();

    bool Init(FFmpegMuxer& muxer, const Layout::FramebufferLayout& layout);
    void Free();
    void ProcessFrame(VideoFrame& frame);

private:
    bool InitHWContext(const AVCodec* codec);
    bool InitFilters();

    u64 frame_count{};

    std::unique_ptr<AVFrame, AVFrameDeleter> current_frame{};
    std::unique_ptr<AVFrame, AVFrameDeleter> filtered_frame{};
    std::unique_ptr<AVFrame, AVFrameDeleter> hw_frame{};
    Layout::FramebufferLayout layout;

    /// The pixel format the input frames are stored in
    static constexpr AVPixelFormat pixel_format = AVPixelFormat::AV_PIX_FMT_BGRA;

    // Software pixel format. For normal encoders, this is the format they accept. For HW-acceled
    // encoders, this is the format the HW frames context accepts.
    AVPixelFormat sw_pixel_format = AV_PIX_FMT_NONE;

    /// Whether the encoder we are using requires HW frames to be supplied.
    bool requires_hw_frames = false;

    // Filter related
    struct AVFilterGraphDeleter {
        void operator()(AVFilterGraph* filter_graph) const {
            DynamicLibrary::FFmpeg::avfilter_graph_free(&filter_graph);
        }
    };
    std::unique_ptr<AVFilterGraph, AVFilterGraphDeleter> filter_graph{};
    // These don't need to be freed apparently
    AVFilterContext* source_context;
    AVFilterContext* sink_context;

    /// The filter graph to use. This graph means 'change FPS to 60, convert format if needed'
    static constexpr std::string_view filter_graph_desc = "fps=60";
};

/**
 * A FFmpegStream used for audio data.
 * Resamples (converts), encodes and writes a frame.
 * This also temporarily stores resampled audio data before there are enough to form a frame.
 */
class FFmpegAudioStream : public FFmpegStream {
public:
    ~FFmpegAudioStream();

    bool Init(FFmpegMuxer& muxer);
    void Free();
    void ProcessFrame(const VariableAudioFrame& channel0, const VariableAudioFrame& channel1);
    void Flush();

private:
    struct SwrContextDeleter {
        void operator()(SwrContext* swr_context) const {
            DynamicLibrary::FFmpeg::swr_free(&swr_context);
        }
    };

    int frame_size{};
    u64 frame_count{};

    std::unique_ptr<AVFrame, AVFrameDeleter> audio_frame{};
    std::unique_ptr<SwrContext, SwrContextDeleter> swr_context{};

    u8** resampled_data{};
    int offset{}; // Number of output samples that are currently in resampled_data.
};

/**
 * Wrapper around FFmpeg AVFormatContext.
 * Manages the video and audio streams, and accepts video and audio data.
 */
class FFmpegMuxer {
public:
    ~FFmpegMuxer();

    bool Init(const std::string& path, const Layout::FramebufferLayout& layout);
    void Free();
    void ProcessVideoFrame(VideoFrame& frame);
    void ProcessAudioFrame(const VariableAudioFrame& channel0, const VariableAudioFrame& channel1);
    void FlushVideo();
    void FlushAudio();
    void WriteTrailer();

private:
    struct AVFormatContextDeleter {
        void operator()(AVFormatContext* format_context) const {
            DynamicLibrary::FFmpeg::avio_closep(&format_context->pb);
            DynamicLibrary::FFmpeg::avformat_free_context(format_context);
        }
    };

    FFmpegAudioStream audio_stream{};
    FFmpegVideoStream video_stream{};
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> format_context{};
    std::mutex format_context_mutex;

    friend class FFmpegStream;
};

/**
 * FFmpeg video dumping backend.
 * This class implements a double buffer.
 */
class FFmpegBackend : public Backend {
public:
    FFmpegBackend(VideoCore::RendererBase& renderer);
    ~FFmpegBackend() override;
    bool StartDumping(const std::string& path, const Layout::FramebufferLayout& layout) override;
    void AddVideoFrame(VideoFrame frame) override;
    void AddAudioFrame(AudioCore::StereoFrame16 frame) override;
    void AddAudioSample(const std::array<s16, 2>& sample) override;
    void StopDumping() override;
    bool IsDumping() const override;
    Layout::FramebufferLayout GetLayout() const override;

private:
    void EndDumping();

    VideoCore::RendererBase& renderer;
    std::atomic_bool is_dumping = false; ///< Whether the backend is currently dumping

    FFmpegMuxer ffmpeg{};

    Layout::FramebufferLayout video_layout;
    std::array<VideoFrame, 2> video_frame_buffers;
    u32 current_buffer = 0, next_buffer = 1;
    Common::Event event1, event2;
    std::thread video_processing_thread;

    std::array<Common::SPSCQueue<VariableAudioFrame>, 2> audio_frame_queues;
    std::thread audio_processing_thread;

    Common::Event processing_ended;
};

/// Struct describing encoder/muxer options
struct OptionInfo {
    std::string name;
    std::string description;
    AVOptionType type;
    std::string default_value;
    struct NamedConstant {
        std::string name;
        std::string description;
        s64 value;
    };
    std::vector<NamedConstant> named_constants;

    // If this is a scalar type
    double min;
    double max;
};

/// Struct describing an encoder
struct EncoderInfo {
    std::string name;
    std::string long_name;
    AVCodecID codec;
    std::vector<OptionInfo> options;
};

/// Struct describing a format
struct FormatInfo {
    std::string name;
    std::string long_name;
    std::vector<std::string> extensions;
    std::set<AVCodecID> supported_video_codecs;
    std::set<AVCodecID> supported_audio_codecs;
    std::vector<OptionInfo> options;
};

std::vector<EncoderInfo> ListEncoders(AVMediaType type);
std::vector<OptionInfo> GetEncoderGenericOptions();
std::vector<FormatInfo> ListFormats();
std::vector<OptionInfo> GetFormatGenericOptions();
std::vector<std::string> GetPixelFormats();
std::vector<std::string> GetSampleFormats();

} // namespace VideoDumper
