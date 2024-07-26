// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <neaacdec.h>
#include "audio_core/hle/aac_decoder.h"

namespace AudioCore::HLE {

AACDecoder::AACDecoder(Memory::MemorySystem& memory) : memory(memory) {
    decoder = NeAACDecOpen();
    if (decoder == nullptr) {
        LOG_CRITICAL(Audio_DSP, "Could not open FAAD2 decoder.");
        return;
    }

    auto config = NeAACDecGetCurrentConfiguration(decoder);
    config->defObjectType = LC;
    config->outputFormat = FAAD_FMT_16BIT;
    if (!NeAACDecSetConfiguration(decoder, config)) {
        LOG_CRITICAL(Audio_DSP, "Could not configure FAAD2 decoder.");
        NeAACDecClose(decoder);
        decoder = nullptr;
        return;
    }

    LOG_INFO(Audio_DSP, "Created FAAD2 AAC decoder.");
}

AACDecoder::~AACDecoder() {
    if (decoder) {
        NeAACDecClose(decoder);
        decoder = nullptr;

        LOG_INFO(Audio_DSP, "Destroyed FAAD2 AAC decoder.");
    }
}

BinaryMessage AACDecoder::ProcessRequest(const BinaryMessage& request) {
    if (request.header.codec != DecoderCodec::DecodeAAC) {
        LOG_ERROR(Audio_DSP, "AAC decoder received unsupported codec: {}",
                  static_cast<u16>(request.header.codec));
        return {
            .header =
                {
                    .result = ResultStatus::Error,
                },
        };
    }

    switch (request.header.cmd) {
    case DecoderCommand::Init: {
        BinaryMessage response = request;
        response.header.result = ResultStatus::Success;
        return response;
    }
    case DecoderCommand::EncodeDecode: {
        return Decode(request);
    }
    case DecoderCommand::Shutdown:
    case DecoderCommand::SaveState:
    case DecoderCommand::LoadState: {
        LOG_WARNING(Audio_DSP, "Got unimplemented AAC binary request: {}",
                    static_cast<u16>(request.header.cmd));
        BinaryMessage response = request;
        response.header.result = ResultStatus::Success;
        return response;
    }
    default:
        LOG_ERROR(Audio_DSP, "Got unknown AAC binary request: {}",
                  static_cast<u16>(request.header.cmd));
        return {
            .header =
                {
                    .result = ResultStatus::Error,
                },
        };
    }
}

BinaryMessage AACDecoder::Decode(const BinaryMessage& request) {
    BinaryMessage response{};
    response.header.codec = request.header.codec;
    response.header.cmd = request.header.cmd;
    response.decode_aac_response.size = request.decode_aac_request.size;
    // This is a hack to continue games when a failure occurs.
    response.decode_aac_response.sample_rate = DecoderSampleRate::Rate48000;
    response.decode_aac_response.num_channels = 2;
    response.decode_aac_response.num_samples = 1024;

    if (decoder == nullptr) {
        return response;
    }

    if (request.decode_aac_request.src_addr < Memory::FCRAM_PADDR ||
        request.decode_aac_request.src_addr + request.decode_aac_request.size >
            Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
        LOG_ERROR(Audio_DSP, "Got out of bounds src_addr {:08x}",
                  request.decode_aac_request.src_addr);
        return response;
    }
    u8* data = memory.GetFCRAMPointer(request.decode_aac_request.src_addr - Memory::FCRAM_PADDR);
    u32 data_len = request.decode_aac_request.size;

    unsigned long sample_rate;
    u8 num_channels;
    auto init_result = NeAACDecInit(decoder, data, data_len, &sample_rate, &num_channels);
    if (init_result < 0) {
        LOG_ERROR(Audio_DSP, "Could not initialize FAAD2 AAC decoder for request: {}", init_result);
        return response;
    }

    // Advance past the frame header if needed.
    data += init_result;
    data_len -= init_result;

    std::array<std::vector<s16>, 2> out_streams;

    while (data_len > 0) {
        NeAACDecFrameInfo frame_info;
        auto curr_sample_buffer =
            static_cast<s16*>(NeAACDecDecode(decoder, &frame_info, data, data_len));
        if (curr_sample_buffer == nullptr || frame_info.error != 0) {
            LOG_ERROR(Audio_DSP, "Failed to decode AAC buffer using FAAD2: {}", frame_info.error);
            return response;
        }

        // Split the decode result into channels.
        u32 num_samples = frame_info.samples / frame_info.channels;
        for (u32 sample = 0; sample < num_samples; sample++) {
            for (u32 ch = 0; ch < frame_info.channels; ch++) {
                out_streams[ch].push_back(curr_sample_buffer[(sample * frame_info.channels) + ch]);
            }
        }

        data += frame_info.bytesconsumed;
        data_len -= frame_info.bytesconsumed;
    }

    // Transfer the decoded buffer from vector to the FCRAM.
    for (std::size_t ch = 0; ch < out_streams.size(); ch++) {
        if (out_streams[ch].empty()) {
            continue;
        }
        auto byte_size = out_streams[ch].size() * sizeof(s16);
        auto dst = ch == 0 ? request.decode_aac_request.dst_addr_ch0
                           : request.decode_aac_request.dst_addr_ch1;
        if (dst < Memory::FCRAM_PADDR ||
            dst + byte_size > Memory::FCRAM_PADDR + Memory::FCRAM_SIZE) {
            LOG_ERROR(Audio_DSP, "Got out of bounds dst_addr_ch{} {:08x}", ch, dst);
            return response;
        }
        std::memcpy(memory.GetFCRAMPointer(dst - Memory::FCRAM_PADDR), out_streams[ch].data(),
                    byte_size);
    }

    // Set the output frame info.
    response.decode_aac_response.sample_rate = GetSampleRateEnum(sample_rate);
    response.decode_aac_response.num_channels = num_channels;
    response.decode_aac_response.num_samples = static_cast<u32_le>(out_streams[0].size());

    return response;
}

} // namespace AudioCore::HLE
