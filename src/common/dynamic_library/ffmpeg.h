// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/ffversion.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswresample/swresample.h>
}

namespace DynamicLibrary::FFmpeg {

// avutil
typedef AVBufferRef* (*av_buffer_ref_func)(const AVBufferRef*);
typedef void (*av_buffer_unref_func)(AVBufferRef**);
typedef AVRational (*av_d2q_func)(double d, int max);
typedef int (*av_dict_count_func)(const AVDictionary*);
typedef AVDictionaryEntry* (*av_dict_get_func)(const AVDictionary*, const char*,
                                               const AVDictionaryEntry*, int);
typedef int (*av_dict_get_string_func)(const AVDictionary*, char**, const char, const char);
typedef int (*av_dict_set_func)(AVDictionary**, const char*, const char*, int);
typedef AVFrame* (*av_frame_alloc_func)();
typedef void (*av_frame_free_func)(AVFrame**);
typedef void (*av_frame_unref_func)(AVFrame*);
typedef void (*av_freep_func)(void*);
typedef int (*av_get_bytes_per_sample_func)(AVSampleFormat);
typedef AVPixelFormat (*av_get_pix_fmt_func)(const char*);
typedef const char* (*av_get_pix_fmt_name_func)(AVPixelFormat);
typedef const char* (*av_get_sample_fmt_name_func)(AVSampleFormat);
typedef int (*av_hwdevice_ctx_create_func)(AVBufferRef**, AVHWDeviceType, const char*,
                                           AVDictionary*, int);
typedef AVHWFramesConstraints* (*av_hwdevice_get_hwframe_constraints_func)(AVBufferRef*,
                                                                           const void*);
typedef void (*av_hwframe_constraints_free_func)(AVHWFramesConstraints**);
typedef AVBufferRef* (*av_hwframe_ctx_alloc_func)(AVBufferRef*);
typedef int (*av_hwframe_ctx_init_func)(AVBufferRef*);
typedef int (*av_hwframe_get_buffer_func)(AVBufferRef*, AVFrame*, int);
typedef int (*av_hwframe_transfer_data_func)(AVFrame*, const AVFrame*, int);
typedef unsigned (*av_int_list_length_for_size_func)(unsigned, const void*, uint64_t);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 53, 100) // lavu 56.53.100
typedef const AVClass* (*av_opt_child_class_iterate_func)(const AVClass*, void**);
#else
typedef const AVClass* (*av_opt_child_class_next_func)(const AVClass*, const AVClass*);
#endif
typedef const AVOption* (*av_opt_next_func)(const void*, const AVOption*);
typedef int (*av_opt_set_bin_func)(void*, const char*, const uint8_t*, int, int);
typedef const AVPixFmtDescriptor* (*av_pix_fmt_desc_get_func)(AVPixelFormat);
typedef const AVPixFmtDescriptor* (*av_pix_fmt_desc_next_func)(const AVPixFmtDescriptor*);
typedef int (*av_sample_fmt_is_planar_func)(AVSampleFormat);
typedef int (*av_samples_alloc_array_and_samples_func)(uint8_t***, int*, int, int, AVSampleFormat,
                                                       int);
typedef char* (*av_strdup_func)(const char*);
typedef unsigned (*avutil_version_func)();

extern av_buffer_ref_func av_buffer_ref;
extern av_buffer_unref_func av_buffer_unref;
extern av_d2q_func av_d2q;
extern av_dict_count_func av_dict_count;
extern av_dict_get_func av_dict_get;
extern av_dict_get_string_func av_dict_get_string;
extern av_dict_set_func av_dict_set;
extern av_frame_alloc_func av_frame_alloc;
extern av_frame_free_func av_frame_free;
extern av_frame_unref_func av_frame_unref;
extern av_freep_func av_freep;
extern av_get_bytes_per_sample_func av_get_bytes_per_sample;
extern av_get_pix_fmt_func av_get_pix_fmt;
extern av_get_pix_fmt_name_func av_get_pix_fmt_name;
extern av_get_sample_fmt_name_func av_get_sample_fmt_name;
extern av_hwdevice_ctx_create_func av_hwdevice_ctx_create;
extern av_hwdevice_get_hwframe_constraints_func av_hwdevice_get_hwframe_constraints;
extern av_hwframe_constraints_free_func av_hwframe_constraints_free;
extern av_hwframe_ctx_alloc_func av_hwframe_ctx_alloc;
extern av_hwframe_ctx_init_func av_hwframe_ctx_init;
extern av_hwframe_get_buffer_func av_hwframe_get_buffer;
extern av_hwframe_transfer_data_func av_hwframe_transfer_data;
extern av_int_list_length_for_size_func av_int_list_length_for_size;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 53, 100) // lavu 56.53.100
extern av_opt_child_class_iterate_func av_opt_child_class_iterate;
#else
extern av_opt_child_class_next_func av_opt_child_class_next;
#endif
extern av_opt_next_func av_opt_next;
extern av_opt_set_bin_func av_opt_set_bin;
extern av_pix_fmt_desc_get_func av_pix_fmt_desc_get;
extern av_pix_fmt_desc_next_func av_pix_fmt_desc_next;
extern av_sample_fmt_is_planar_func av_sample_fmt_is_planar;
extern av_samples_alloc_array_and_samples_func av_samples_alloc_array_and_samples;
extern av_strdup_func av_strdup;
extern avutil_version_func avutil_version;

// avcodec
typedef int (*av_codec_is_encoder_func)(const AVCodec*);
typedef const AVCodec* (*av_codec_iterate_func)(void**);
typedef void (*av_init_packet_func)(AVPacket*);
typedef AVPacket* (*av_packet_alloc_func)();
typedef void (*av_packet_free_func)(AVPacket**);
typedef void (*av_packet_rescale_ts_func)(AVPacket*, AVRational, AVRational);
typedef void (*av_parser_close_func)(AVCodecParserContext*);
typedef AVCodecParserContext* (*av_parser_init_func)(int);
typedef int (*av_parser_parse2_func)(AVCodecParserContext*, AVCodecContext*, uint8_t**, int*,
                                     const uint8_t*, int, int64_t, int64_t, int64_t);
typedef AVCodecContext* (*avcodec_alloc_context3_func)(const AVCodec*);
typedef const AVCodecDescriptor* (*avcodec_descriptor_next_func)(const AVCodecDescriptor*);
typedef AVCodec* (*avcodec_find_decoder_func)(AVCodecID);
typedef const AVCodec* (*avcodec_find_encoder_by_name_func)(const char*);
typedef void (*avcodec_free_context_func)(AVCodecContext**);
typedef const AVClass* (*avcodec_get_class_func)();
typedef const AVCodecHWConfig* (*avcodec_get_hw_config_func)(const AVCodec*, int);
typedef int (*avcodec_open2_func)(AVCodecContext*, const AVCodec*, AVDictionary**);
typedef int (*avcodec_parameters_from_context_func)(AVCodecParameters* par, const AVCodecContext*);
typedef int (*avcodec_receive_frame_func)(AVCodecContext*, AVFrame*);
typedef int (*avcodec_receive_packet_func)(AVCodecContext*, AVPacket*);
typedef int (*avcodec_send_frame_func)(AVCodecContext*, const AVFrame*);
typedef int (*avcodec_send_packet_func)(AVCodecContext*, const AVPacket*);
typedef unsigned (*avcodec_version_func)();

extern av_codec_is_encoder_func av_codec_is_encoder;
extern av_codec_iterate_func av_codec_iterate;
extern av_init_packet_func av_init_packet;
extern av_packet_alloc_func av_packet_alloc;
extern av_packet_free_func av_packet_free;
extern av_packet_rescale_ts_func av_packet_rescale_ts;
extern av_parser_close_func av_parser_close;
extern av_parser_init_func av_parser_init;
extern av_parser_parse2_func av_parser_parse2;
extern avcodec_alloc_context3_func avcodec_alloc_context3;
extern avcodec_descriptor_next_func avcodec_descriptor_next;
extern avcodec_find_decoder_func avcodec_find_decoder;
extern avcodec_find_encoder_by_name_func avcodec_find_encoder_by_name;
extern avcodec_free_context_func avcodec_free_context;
extern avcodec_get_class_func avcodec_get_class;
extern avcodec_get_hw_config_func avcodec_get_hw_config;
extern avcodec_open2_func avcodec_open2;
extern avcodec_parameters_from_context_func avcodec_parameters_from_context;
extern avcodec_receive_frame_func avcodec_receive_frame;
extern avcodec_receive_packet_func avcodec_receive_packet;
extern avcodec_send_frame_func avcodec_send_frame;
extern avcodec_send_packet_func avcodec_send_packet;
extern avcodec_version_func avcodec_version;

// avfilter
typedef int (*av_buffersink_get_frame_func)(AVFilterContext*, AVFrame*);
typedef int (*av_buffersrc_add_frame_func)(AVFilterContext*, AVFrame*);
typedef const AVFilter* (*avfilter_get_by_name_func)(const char*);
typedef AVFilterGraph* (*avfilter_graph_alloc_func)();
typedef int (*avfilter_graph_config_func)(AVFilterGraph*, void*);
typedef int (*avfilter_graph_create_filter_func)(AVFilterContext**, const AVFilter*, const char*,
                                                 const char*, void*, AVFilterGraph*);
typedef void (*avfilter_graph_free_func)(AVFilterGraph** graph);
typedef int (*avfilter_graph_parse_ptr_func)(AVFilterGraph*, const char*, AVFilterInOut**,
                                             AVFilterInOut**, void*);
typedef AVFilterInOut* (*avfilter_inout_alloc_func)();
typedef void (*avfilter_inout_free_func)(AVFilterInOut**);
typedef unsigned (*avfilter_version_func)();

extern av_buffersink_get_frame_func av_buffersink_get_frame;
extern av_buffersrc_add_frame_func av_buffersrc_add_frame;
extern avfilter_get_by_name_func avfilter_get_by_name;
extern avfilter_graph_alloc_func avfilter_graph_alloc;
extern avfilter_graph_config_func avfilter_graph_config;
extern avfilter_graph_create_filter_func avfilter_graph_create_filter;
extern avfilter_graph_free_func avfilter_graph_free;
extern avfilter_graph_parse_ptr_func avfilter_graph_parse_ptr;
extern avfilter_inout_alloc_func avfilter_inout_alloc;
extern avfilter_inout_free_func avfilter_inout_free;
extern avfilter_version_func avfilter_version;

// avformat
typedef const AVOutputFormat* (*av_guess_format_func)(const char*, const char*, const char*);
typedef int (*av_interleaved_write_frame_func)(AVFormatContext*, AVPacket*);
typedef const AVOutputFormat* (*av_muxer_iterate_func)(void**);
typedef int (*av_write_trailer_func)(AVFormatContext*);
typedef int (*avformat_alloc_output_context2_func)(AVFormatContext**, const AVOutputFormat*,
                                                   const char*, const char*);
typedef void (*avformat_free_context_func)(AVFormatContext*);
typedef const AVClass* (*avformat_get_class_func)();
typedef int (*avformat_network_init_func)();
typedef AVStream* (*avformat_new_stream_func)(AVFormatContext*, const AVCodec*);
typedef int (*avformat_query_codec_func)(const AVOutputFormat*, AVCodecID, int);
typedef int (*avformat_write_header_func)(AVFormatContext*, AVDictionary**);
typedef unsigned (*avformat_version_func)();
typedef int (*avio_closep_func)(AVIOContext**);
typedef int (*avio_open_func)(AVIOContext**, const char*, int);

extern av_guess_format_func av_guess_format;
extern av_interleaved_write_frame_func av_interleaved_write_frame;
extern av_muxer_iterate_func av_muxer_iterate;
extern av_write_trailer_func av_write_trailer;
extern avformat_alloc_output_context2_func avformat_alloc_output_context2;
extern avformat_free_context_func avformat_free_context;
extern avformat_get_class_func avformat_get_class;
extern avformat_network_init_func avformat_network_init;
extern avformat_new_stream_func avformat_new_stream;
extern avformat_query_codec_func avformat_query_codec;
extern avformat_write_header_func avformat_write_header;
extern avformat_version_func avformat_version;
extern avio_closep_func avio_closep;
extern avio_open_func avio_open;

// swresample
#if LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 100)
typedef SwrContext* (*swr_alloc_set_opts2_func)(SwrContext**, AVChannelLayout*, AVSampleFormat, int,
                                                AVChannelLayout*, AVSampleFormat, int, int, void*);
#else
typedef SwrContext* (*swr_alloc_set_opts_func)(SwrContext*, int64_t, AVSampleFormat, int, int64_t,
                                               AVSampleFormat, int, int, void*);
#endif
typedef int (*swr_convert_func)(SwrContext*, uint8_t**, int, const uint8_t**, int);
typedef void (*swr_free_func)(SwrContext**);
typedef int (*swr_init_func)(SwrContext*);
typedef unsigned (*swresample_version_func)();

#if LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 100)
extern swr_alloc_set_opts2_func swr_alloc_set_opts2;
#else
extern swr_alloc_set_opts_func swr_alloc_set_opts;
#endif
extern swr_convert_func swr_convert;
extern swr_free_func swr_free;
extern swr_init_func swr_init;
extern swresample_version_func swresample_version;

bool LoadFFmpeg();

} // namespace DynamicLibrary::FFmpeg
