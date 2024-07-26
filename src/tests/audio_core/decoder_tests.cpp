// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
#if defined(HAVE_MF) || defined(HAVE_FFMPEG)

#include <catch2/catch_test_macros.hpp>
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/shared_page.h"
#include "core/memory.h"

#include "audio_core/hle/decoder.h"
#ifdef HAVE_MF
#include "audio_core/hle/wmf_decoder.h"
#elif HAVE_FFMPEG
#include "audio_core/hle/ffmpeg_decoder.h"
#endif
#include "audio_fixures.h"

TEST_CASE("DSP HLE Audio Decoder", "[audio_core]") {
    Core::System system;
    Memory::MemorySystem memory{system};
    SECTION("decoder should produce correct samples") {
        auto decoder =
#ifdef HAVE_MF
            std::make_unique<AudioCore::HLE::WMFDecoder>(memory);
#elif HAVE_FFMPEG
            std::make_unique<AudioCore::HLE::FFMPEGDecoder>(memory);
#endif
        AudioCore::HLE::BinaryMessage request{};

        request.header.codec = AudioCore::HLE::DecoderCodec::DecodeAAC;
        request.header.cmd = AudioCore::HLE::DecoderCommand::Init;
        // initialize decoder
        std::optional<AudioCore::HLE::BinaryMessage> response = decoder->ProcessRequest(request);

        request.header.cmd = AudioCore::HLE::DecoderCommand::EncodeDecode;
        u8* fcram = memory.GetFCRAMPointer(0);

        std::memcpy(fcram, fixure_buffer, fixure_buffer_size);
        request.decode_aac_request.src_addr = Memory::FCRAM_PADDR;
        request.decode_aac_request.dst_addr_ch0 = Memory::FCRAM_PADDR + 1024;
        request.decode_aac_request.dst_addr_ch1 = Memory::FCRAM_PADDR + 1048576; // 1 MB
        request.decode_aac_request.size = fixure_buffer_size;

        response = decoder->ProcessRequest(request);
        response = decoder->ProcessRequest(request);
        // remove this line
        request.decode_aac_request.src_addr = Memory::FCRAM_PADDR;
    }
}

#endif
