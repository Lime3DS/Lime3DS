// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_base.h"
#include "video_core/renderer_software/sw_rasterizer.h"

namespace Core {
class System;
}

namespace SwRenderer {

struct ScreenInfo {
    u32 width;
    u32 height;
    std::vector<u8> pixels;
};

class RendererSoftware : public VideoCore::RendererBase {
public:
    explicit RendererSoftware(Core::System& system, Pica::PicaCore& pica,
                              Frontend::EmuWindow& window);
    ~RendererSoftware() override;

    [[nodiscard]] VideoCore::RasterizerInterface* Rasterizer() override {
        return &rasterizer;
    }

    [[nodiscard]] const ScreenInfo& Screen(VideoCore::ScreenId id) const noexcept {
        return screen_infos[static_cast<u32>(id)];
    }

    void SwapBuffers() override;
    void TryPresent(int timeout_ms, bool is_secondary) override {}
    void Sync() override {}

private:
    void PrepareRenderTarget();
    void LoadFBToScreenInfo(int i);

private:
    Memory::MemorySystem& memory;
    Pica::PicaCore& pica;
    RasterizerSoftware rasterizer;
    std::array<ScreenInfo, 3> screen_infos{};
};

} // namespace SwRenderer
