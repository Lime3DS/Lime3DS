// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/core.h"

namespace AudioCore::HLE {

enum class DecoderCommand : u16 {
    /// Initializes the decoder.
    Init = 0,
    /// Decodes/encodes a data frame.
    EncodeDecode = 1,
    /// Shuts down the decoder.
    Shutdown = 2,
    /// Loads the saved decoder state. Used for DSP wake.
    LoadState = 3,
    /// Saves the decoder state. Used for DSP sleep.
    SaveState = 4,
};

enum class DecoderCodec : u16 {
    None = 0,
    DecodeAAC = 1,
    EncodeAAC = 2,
};

enum class ResultStatus : u32 {
    Success = 0,
    Error = 1,
};

enum class DecoderSampleRate : u32 {
    Rate48000 = 0,
    Rate44100 = 1,
    Rate32000 = 2,
    Rate24000 = 3,
    Rate22050 = 4,
    Rate16000 = 5,
    Rate12000 = 6,
    Rate11025 = 7,
    Rate8000 = 8
};

// The DSP replies with the same contents as the response too.
struct DecodeAACInitRequest {
    u32_le unknown1 = 0; // observed 1 here
    u32_le unknown2 = 0; // observed -1 here
    u32_le unknown3 = 0; // observed 1 here
    u32_le unknown4 = 0; // observed 0 here
    u32_le unknown5 = 0; // unused? observed 1 here
    u32_le unknown6 = 0; // unused? observed 0x20 here
};

struct DecodeAACRequest {
    u32_le src_addr = 0;
    u32_le size = 0;
    u32_le dst_addr_ch0 = 0;
    u32_le dst_addr_ch1 = 0;
    u32_le unknown1 = 0; // unused?
    u32_le unknown2 = 0; // unused?
};

struct DecodeAACResponse {
    enum_le<DecoderSampleRate> sample_rate;
    u32_le num_channels = 0; // this is a guess, so far I only observed 2 here
    u32_le size = 0;
    u32_le unknown1 = 0;
    u32_le unknown2 = 0;
    u32_le num_samples = 0; // this is a guess, so far I only observed 1024 here
};

// The DSP replies with the same contents as the response too.
struct EncodeAACInitRequest {
    u32_le unknown1 = 0; // Num channels? 1 or 2. observed 1 here
    enum_le<DecoderSampleRate> sample_rate =
        DecoderSampleRate::Rate16000; // the rate the 3DS Sound app uses
    u32_le unknown3 = 0;              // less than 3 according to the 3DS Sound app. observed 2 here
    u32_le unknown4 =
        0; // 0:raw 1:ADTS? less than 2 according to the 3DS Sound app. observed 0 here
    u32_le unknown5 = 0; // unused?
    u32_le unknown6 = 0; // unused?
};

struct EncodeAACRequest {
    u32_le src_addr_ch0 = 0;
    u32_le src_addr_ch1 = 0;
    u32_le dst_addr = 0;
    u32_le unknown1 = 0; // the 3DS Sound app explicitly moves 0x003B'4A08, possibly an address
    u32_le unknown2 = 0; // unused?
    u32_le unknown3 = 0; // unused?
};

struct EncodeAACResponse {
    u32_le unknown1 = 0;
    u32_le unknown2 = 0;
    u32_le unknown3 = 0;
    u32_le unknown4 = 0;
    u32_le unknown5 = 0; // unused?
    u32_le unknown6 = 0; // unused?
};

struct BinaryMessage {
    struct {
        enum_le<DecoderCodec> codec =
            DecoderCodec::None; // this is a guess. until now only 0x1 was observed here
        enum_le<DecoderCommand> cmd = DecoderCommand::Init;
        // This is a guess, when tested with Init EncodeAAC, the DSP replies 0x0 for apparently
        // valid values and 0x1 (regardless of what was passed in the request) for invalid values in
        // other fields
        enum_le<ResultStatus> result = ResultStatus::Error;
    } header;
    union {
        std::array<u8, 24> data{};

        DecodeAACInitRequest decode_aac_init;
        DecodeAACRequest decode_aac_request;
        DecodeAACResponse decode_aac_response;

        EncodeAACInitRequest encode_aac_init;
        EncodeAACRequest encode_aac_request;
        EncodeAACResponse encode_aac_response;
    };
};
static_assert(sizeof(BinaryMessage) == 32, "Unexpected struct size for BinaryMessage");

enum_le<DecoderSampleRate> GetSampleRateEnum(u32 sample_rate);

class DecoderBase {
public:
    virtual ~DecoderBase() = default;
    virtual BinaryMessage ProcessRequest(const BinaryMessage& request) = 0;
};

} // namespace AudioCore::HLE
