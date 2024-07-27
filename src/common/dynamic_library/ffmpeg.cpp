// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/dynamic_library/dynamic_library.h"
#include "common/dynamic_library/ffmpeg.h"
#include "common/logging/log.h"

namespace DynamicLibrary::FFmpeg {

// avutil
av_buffer_ref_func av_buffer_ref;
av_buffer_unref_func av_buffer_unref;
av_d2q_func av_d2q;
av_dict_count_func av_dict_count;
av_dict_get_func av_dict_get;
av_dict_get_string_func av_dict_get_string;
av_dict_set_func av_dict_set;
av_frame_alloc_func av_frame_alloc;
av_frame_free_func av_frame_free;
av_frame_unref_func av_frame_unref;
av_freep_func av_freep;
av_get_bytes_per_sample_func av_get_bytes_per_sample;
av_get_pix_fmt_func av_get_pix_fmt;
av_get_pix_fmt_name_func av_get_pix_fmt_name;
av_get_sample_fmt_name_func av_get_sample_fmt_name;
av_hwdevice_ctx_create_func av_hwdevice_ctx_create;
av_hwdevice_get_hwframe_constraints_func av_hwdevice_get_hwframe_constraints;
av_hwframe_constraints_free_func av_hwframe_constraints_free;
av_hwframe_ctx_alloc_func av_hwframe_ctx_alloc;
av_hwframe_ctx_init_func av_hwframe_ctx_init;
av_hwframe_get_buffer_func av_hwframe_get_buffer;
av_hwframe_transfer_data_func av_hwframe_transfer_data;
av_int_list_length_for_size_func av_int_list_length_for_size;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 53, 100) // lavu 56.53.100
av_opt_child_class_iterate_func av_opt_child_class_iterate;
#else
av_opt_child_class_next_func av_opt_child_class_next;
#endif
av_opt_next_func av_opt_next;
av_opt_set_bin_func av_opt_set_bin;
av_pix_fmt_desc_get_func av_pix_fmt_desc_get;
av_pix_fmt_desc_next_func av_pix_fmt_desc_next;
av_sample_fmt_is_planar_func av_sample_fmt_is_planar;
av_samples_alloc_array_and_samples_func av_samples_alloc_array_and_samples;
av_strdup_func av_strdup;
avutil_version_func avutil_version;

// avcodec
av_codec_is_encoder_func av_codec_is_encoder;
av_codec_iterate_func av_codec_iterate;
av_init_packet_func av_init_packet;
av_packet_alloc_func av_packet_alloc;
av_packet_free_func av_packet_free;
av_packet_rescale_ts_func av_packet_rescale_ts;
av_parser_close_func av_parser_close;
av_parser_init_func av_parser_init;
av_parser_parse2_func av_parser_parse2;
avcodec_alloc_context3_func avcodec_alloc_context3;
avcodec_descriptor_next_func avcodec_descriptor_next;
avcodec_find_decoder_func avcodec_find_decoder;
avcodec_find_encoder_by_name_func avcodec_find_encoder_by_name;
avcodec_free_context_func avcodec_free_context;
avcodec_get_class_func avcodec_get_class;
avcodec_get_hw_config_func avcodec_get_hw_config;
avcodec_open2_func avcodec_open2;
avcodec_parameters_from_context_func avcodec_parameters_from_context;
avcodec_receive_frame_func avcodec_receive_frame;
avcodec_receive_packet_func avcodec_receive_packet;
avcodec_send_frame_func avcodec_send_frame;
avcodec_send_packet_func avcodec_send_packet;
avcodec_version_func avcodec_version;

// avfilter
av_buffersink_get_frame_func av_buffersink_get_frame;
av_buffersrc_add_frame_func av_buffersrc_add_frame;
avfilter_get_by_name_func avfilter_get_by_name;
avfilter_graph_alloc_func avfilter_graph_alloc;
avfilter_graph_config_func avfilter_graph_config;
avfilter_graph_create_filter_func avfilter_graph_create_filter;
avfilter_graph_free_func avfilter_graph_free;
avfilter_graph_parse_ptr_func avfilter_graph_parse_ptr;
avfilter_inout_alloc_func avfilter_inout_alloc;
avfilter_inout_free_func avfilter_inout_free;
avfilter_version_func avfilter_version;

// avformat
av_guess_format_func av_guess_format;
av_interleaved_write_frame_func av_interleaved_write_frame;
av_muxer_iterate_func av_muxer_iterate;
av_write_trailer_func av_write_trailer;
avformat_alloc_output_context2_func avformat_alloc_output_context2;
avformat_free_context_func avformat_free_context;
avformat_get_class_func avformat_get_class;
avformat_network_init_func avformat_network_init;
avformat_new_stream_func avformat_new_stream;
avformat_query_codec_func avformat_query_codec;
avformat_write_header_func avformat_write_header;
avformat_version_func avformat_version;
avio_closep_func avio_closep;
avio_open_func avio_open;

// swresample
#if LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 100)
swr_alloc_set_opts2_func swr_alloc_set_opts2;
#else
swr_alloc_set_opts_func swr_alloc_set_opts;
#endif
swr_convert_func swr_convert;
swr_free_func swr_free;
swr_init_func swr_init;
swresample_version_func swresample_version;

static std::unique_ptr<Common::DynamicLibrary> avutil;
static std::unique_ptr<Common::DynamicLibrary> avcodec;
static std::unique_ptr<Common::DynamicLibrary> avfilter;
static std::unique_ptr<Common::DynamicLibrary> avformat;
static std::unique_ptr<Common::DynamicLibrary> swresample;

#define LOAD_SYMBOL(library, name)                                                                 \
    any_failed = any_failed || (name = library->GetSymbol<name##_func>(#name)) == nullptr

static bool LoadAVUtil() {
    if (avutil) {
        return true;
    }

    avutil = std::make_unique<Common::DynamicLibrary>("avutil", LIBAVUTIL_VERSION_MAJOR);
    if (!avutil->IsLoaded()) {
        LOG_WARNING(Common, "Could not dynamically load libavutil: {}", avutil->GetLoadError());
        avutil.reset();
        return false;
    }

    auto any_failed = false;

    LOAD_SYMBOL(avutil, avutil_version);

    auto major_version = AV_VERSION_MAJOR(avutil_version());
    if (major_version != LIBAVUTIL_VERSION_MAJOR) {
        LOG_WARNING(Common, "libavutil version {} does not match supported version {}.",
                    major_version, LIBAVUTIL_VERSION_MAJOR);
        avutil.reset();
        return false;
    }

    LOAD_SYMBOL(avutil, av_buffer_ref);
    LOAD_SYMBOL(avutil, av_buffer_unref);
    LOAD_SYMBOL(avutil, av_d2q);
    LOAD_SYMBOL(avutil, av_dict_count);
    LOAD_SYMBOL(avutil, av_dict_get);
    LOAD_SYMBOL(avutil, av_dict_get_string);
    LOAD_SYMBOL(avutil, av_dict_set);
    LOAD_SYMBOL(avutil, av_frame_alloc);
    LOAD_SYMBOL(avutil, av_frame_free);
    LOAD_SYMBOL(avutil, av_frame_unref);
    LOAD_SYMBOL(avutil, av_freep);
    LOAD_SYMBOL(avutil, av_get_bytes_per_sample);
    LOAD_SYMBOL(avutil, av_get_pix_fmt);
    LOAD_SYMBOL(avutil, av_get_pix_fmt_name);
    LOAD_SYMBOL(avutil, av_get_sample_fmt_name);
    LOAD_SYMBOL(avutil, av_hwdevice_ctx_create);
    LOAD_SYMBOL(avutil, av_hwdevice_get_hwframe_constraints);
    LOAD_SYMBOL(avutil, av_hwframe_constraints_free);
    LOAD_SYMBOL(avutil, av_hwframe_ctx_alloc);
    LOAD_SYMBOL(avutil, av_hwframe_ctx_init);
    LOAD_SYMBOL(avutil, av_hwframe_get_buffer);
    LOAD_SYMBOL(avutil, av_hwframe_transfer_data);
    LOAD_SYMBOL(avutil, av_int_list_length_for_size);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 53, 100) // lavu 56.53.100
    LOAD_SYMBOL(avutil, av_opt_child_class_iterate);
#else
    LOAD_SYMBOL(avutil, av_opt_child_class_next);
#endif
    LOAD_SYMBOL(avutil, av_opt_next);
    LOAD_SYMBOL(avutil, av_opt_set_bin);
    LOAD_SYMBOL(avutil, av_pix_fmt_desc_get);
    LOAD_SYMBOL(avutil, av_pix_fmt_desc_next);
    LOAD_SYMBOL(avutil, av_sample_fmt_is_planar);
    LOAD_SYMBOL(avutil, av_samples_alloc_array_and_samples);
    LOAD_SYMBOL(avutil, av_strdup);

    if (any_failed) {
        LOG_WARNING(Common, "Could not find all required functions in libavutil.");
        avutil.reset();
        return false;
    }

    LOG_INFO(Common, "Successfully loaded libavutil.");
    return true;
}

static bool LoadAVCodec() {
    if (avcodec) {
        return true;
    }

    avcodec = std::make_unique<Common::DynamicLibrary>("avcodec", LIBAVCODEC_VERSION_MAJOR);
    if (!avcodec->IsLoaded()) {
        LOG_WARNING(Common, "Could not dynamically load libavcodec: {}", avcodec->GetLoadError());
        avcodec.reset();
        return false;
    }

    auto any_failed = false;

    LOAD_SYMBOL(avcodec, avcodec_version);

    auto major_version = AV_VERSION_MAJOR(avcodec_version());
    if (major_version != LIBAVCODEC_VERSION_MAJOR) {
        LOG_WARNING(Common, "libavcodec version {} does not match supported version {}.",
                    major_version, LIBAVCODEC_VERSION_MAJOR);
        avcodec.reset();
        return false;
    }

    LOAD_SYMBOL(avcodec, av_codec_is_encoder);
    LOAD_SYMBOL(avcodec, av_codec_iterate);
    LOAD_SYMBOL(avcodec, av_init_packet);
    LOAD_SYMBOL(avcodec, av_packet_alloc);
    LOAD_SYMBOL(avcodec, av_packet_free);
    LOAD_SYMBOL(avcodec, av_packet_rescale_ts);
    LOAD_SYMBOL(avcodec, av_parser_close);
    LOAD_SYMBOL(avcodec, av_parser_init);
    LOAD_SYMBOL(avcodec, av_parser_parse2);
    LOAD_SYMBOL(avcodec, avcodec_alloc_context3);
    LOAD_SYMBOL(avcodec, avcodec_descriptor_next);
    LOAD_SYMBOL(avcodec, avcodec_find_decoder);
    LOAD_SYMBOL(avcodec, avcodec_find_encoder_by_name);
    LOAD_SYMBOL(avcodec, avcodec_free_context);
    LOAD_SYMBOL(avcodec, avcodec_get_class);
    LOAD_SYMBOL(avcodec, avcodec_get_hw_config);
    LOAD_SYMBOL(avcodec, avcodec_open2);
    LOAD_SYMBOL(avcodec, avcodec_parameters_from_context);
    LOAD_SYMBOL(avcodec, avcodec_receive_frame);
    LOAD_SYMBOL(avcodec, avcodec_receive_packet);
    LOAD_SYMBOL(avcodec, avcodec_send_frame);
    LOAD_SYMBOL(avcodec, avcodec_send_packet);

    if (any_failed) {
        LOG_WARNING(Common, "Could not find all required functions in libavcodec.");
        avcodec.reset();
        return false;
    }

    LOG_INFO(Common, "Successfully loaded libavcodec.");
    return true;
}

static bool LoadAVFilter() {
    if (avfilter) {
        return true;
    }

    avfilter = std::make_unique<Common::DynamicLibrary>("avfilter", LIBAVFILTER_VERSION_MAJOR);
    if (!avfilter->IsLoaded()) {
        LOG_WARNING(Common, "Could not dynamically load libavfilter: {}", avfilter->GetLoadError());
        avfilter.reset();
        return false;
    }

    auto any_failed = false;

    LOAD_SYMBOL(avfilter, avfilter_version);

    auto major_version = AV_VERSION_MAJOR(avfilter_version());
    if (major_version != LIBAVFILTER_VERSION_MAJOR) {
        LOG_WARNING(Common, "libavfilter version {} does not match supported version {}.",
                    major_version, LIBAVFILTER_VERSION_MAJOR);
        avfilter.reset();
        return false;
    }

    LOAD_SYMBOL(avfilter, av_buffersink_get_frame);
    LOAD_SYMBOL(avfilter, av_buffersrc_add_frame);
    LOAD_SYMBOL(avfilter, avfilter_get_by_name);
    LOAD_SYMBOL(avfilter, avfilter_graph_alloc);
    LOAD_SYMBOL(avfilter, avfilter_graph_config);
    LOAD_SYMBOL(avfilter, avfilter_graph_create_filter);
    LOAD_SYMBOL(avfilter, avfilter_graph_free);
    LOAD_SYMBOL(avfilter, avfilter_graph_parse_ptr);
    LOAD_SYMBOL(avfilter, avfilter_inout_alloc);
    LOAD_SYMBOL(avfilter, avfilter_inout_free);

    if (any_failed) {
        LOG_WARNING(Common, "Could not find all required functions in libavfilter.");
        avfilter.reset();
        return false;
    }

    LOG_INFO(Common, "Successfully loaded libavfilter.");
    return true;
}

static bool LoadAVFormat() {
    if (avformat) {
        return true;
    }

    avformat = std::make_unique<Common::DynamicLibrary>("avformat", LIBAVFORMAT_VERSION_MAJOR);
    if (!avformat->IsLoaded()) {
        LOG_WARNING(Common, "Could not dynamically load libavformat: {}", avformat->GetLoadError());
        avformat.reset();
        return false;
    }

    auto any_failed = false;

    LOAD_SYMBOL(avformat, avformat_version);

    auto major_version = AV_VERSION_MAJOR(avformat_version());
    if (major_version != LIBAVFORMAT_VERSION_MAJOR) {
        LOG_WARNING(Common, "libavformat version {} does not match supported version {}.",
                    major_version, LIBAVFORMAT_VERSION_MAJOR);
        avformat.reset();
        return false;
    }

    LOAD_SYMBOL(avformat, av_guess_format);
    LOAD_SYMBOL(avformat, av_interleaved_write_frame);
    LOAD_SYMBOL(avformat, av_muxer_iterate);
    LOAD_SYMBOL(avformat, av_write_trailer);
    LOAD_SYMBOL(avformat, avformat_alloc_output_context2);
    LOAD_SYMBOL(avformat, avformat_free_context);
    LOAD_SYMBOL(avformat, avformat_get_class);
    LOAD_SYMBOL(avformat, avformat_network_init);
    LOAD_SYMBOL(avformat, avformat_new_stream);
    LOAD_SYMBOL(avformat, avformat_query_codec);
    LOAD_SYMBOL(avformat, avformat_write_header);
    LOAD_SYMBOL(avformat, avio_closep);
    LOAD_SYMBOL(avformat, avio_open);

    if (any_failed) {
        LOG_WARNING(Common, "Could not find all required functions in libavformat.");
        avformat.reset();
        return false;
    }

    LOG_INFO(Common, "Successfully loaded libavformat.");
    return true;
}

static bool LoadSWResample() {
    if (swresample) {
        return true;
    }

    swresample =
        std::make_unique<Common::DynamicLibrary>("swresample", LIBSWRESAMPLE_VERSION_MAJOR);
    if (!swresample->IsLoaded()) {
        LOG_WARNING(Common, "Could not dynamically load libswresample: {}",
                    swresample->GetLoadError());
        swresample.reset();
        return false;
    }

    auto any_failed = false;

    LOAD_SYMBOL(swresample, swresample_version);

    auto major_version = AV_VERSION_MAJOR(swresample_version());
    if (major_version != LIBSWRESAMPLE_VERSION_MAJOR) {
        LOG_WARNING(Common, "libswresample version {} does not match supported version {}.",
                    major_version, LIBSWRESAMPLE_VERSION_MAJOR);
        swresample.reset();
        return false;
    }

#if LIBSWRESAMPLE_VERSION_INT >= AV_VERSION_INT(4, 5, 100)
    LOAD_SYMBOL(swresample, swr_alloc_set_opts2);
#else
    LOAD_SYMBOL(swresample, swr_alloc_set_opts);
#endif
    LOAD_SYMBOL(swresample, swr_convert);
    LOAD_SYMBOL(swresample, swr_free);
    LOAD_SYMBOL(swresample, swr_init);

    if (any_failed) {
        LOG_WARNING(Common, "Could not find all required functions in libswresample.");
        swresample.reset();
        return false;
    }

    LOG_INFO(Common, "Successfully loaded libswresample.");
    return true;
}

bool LoadFFmpeg() {
    return LoadAVUtil() && LoadAVCodec() && LoadAVFilter() && LoadAVFormat() && LoadSWResample();
}

} // namespace DynamicLibrary::FFmpeg
