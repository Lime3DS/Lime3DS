// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "audio_core/hle/decoder.h"

namespace AudioCore::HLE {

using NeAACDecHandle = void*;

class AACDecoder final : public DecoderBase {
public:
    explicit AACDecoder(Memory::MemorySystem& memory);
    ~AACDecoder() override;
    BinaryMessage ProcessRequest(const BinaryMessage& request) override;

private:
    BinaryMessage Decode(const BinaryMessage& request);

    Memory::MemorySystem& memory;
    NeAACDecHandle decoder = nullptr;
};

} // namespace AudioCore::HLE
