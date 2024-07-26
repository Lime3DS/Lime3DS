// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Pica {
struct DisplayTransferConfig;
struct MemoryFillConfig;
} // namespace Pica

namespace Memory {
class MemorySystem;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace SwRenderer {

class SwBlitter {
public:
    explicit SwBlitter(Memory::MemorySystem& memory, VideoCore::RasterizerInterface* rasterizer);
    ~SwBlitter();

    void TextureCopy(const Pica::DisplayTransferConfig& config);

    void DisplayTransfer(const Pica::DisplayTransferConfig& config);

    void MemoryFill(const Pica::MemoryFillConfig& config);

private:
    Memory::MemorySystem& memory;
    VideoCore::RasterizerInterface* rasterizer;
};

} // namespace SwRenderer
