// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include "audio_core/dsp_interface.h"

namespace Core {
class Timing;
}

namespace Memory {
class MemorySystem;
}

namespace AudioCore {

class DspLle final : public DspInterface {
public:
    explicit DspLle(Core::System& system, bool multithread);
    explicit DspLle(Core::System& system, Memory::MemorySystem& memory, Core::Timing& timing,
                    bool multithread);
    ~DspLle() override;

    u16 RecvData(u32 register_number) override;
    bool RecvDataIsReady(u32 register_number) const override;
    void SetSemaphore(u16 semaphore_value) override;
    std::vector<u8> PipeRead(DspPipe pipe_number, std::size_t length) override;
    std::size_t GetPipeReadableSize(DspPipe pipe_number) const override;
    void PipeWrite(DspPipe pipe_number, std::span<const u8> buffer) override;

    std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory() override;

    void SetInterruptHandler(
        std::function<void(Service::DSP::InterruptType type, DspPipe pipe)> handler) override;

    void LoadComponent(const std::span<const u8> buffer) override;
    void UnloadComponent() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace AudioCore
