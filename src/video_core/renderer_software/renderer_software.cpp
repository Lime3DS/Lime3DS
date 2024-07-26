// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/color.h"
#include "core/core.h"
#include "video_core/gpu.h"
#include "video_core/pica/pica_core.h"
#include "video_core/renderer_software/renderer_software.h"

namespace SwRenderer {

RendererSoftware::RendererSoftware(Core::System& system, Pica::PicaCore& pica_,
                                   Frontend::EmuWindow& window)
    : VideoCore::RendererBase{system, window, nullptr}, memory{system.Memory()}, pica{pica_},
      rasterizer{memory, pica} {}

RendererSoftware::~RendererSoftware() = default;

void RendererSoftware::SwapBuffers() {
    PrepareRenderTarget();
    EndFrame();
}

void RendererSoftware::PrepareRenderTarget() {
    const auto& regs_lcd = pica.regs_lcd;
    for (u32 i = 0; i < 3; i++) {
        const u32 fb_id = i == 2 ? 1 : 0;

        const auto color_fill = fb_id == 0 ? regs_lcd.color_fill_top : regs_lcd.color_fill_bottom;
        if (!color_fill.is_enabled) {
            LoadFBToScreenInfo(i);
        }
    }
}

void RendererSoftware::LoadFBToScreenInfo(int i) {
    const u32 fb_id = i == 2 ? 1 : 0;
    const auto& framebuffer = pica.regs.framebuffer_config[fb_id];
    auto& info = screen_infos[i];

    const PAddr framebuffer_addr =
        framebuffer.active_fb == 0 ? framebuffer.address_left1 : framebuffer.address_left2;
    const s32 bpp = Pica::BytesPerPixel(framebuffer.color_format);
    const u8* framebuffer_data = memory.GetPhysicalPointer(framebuffer_addr);

    const s32 pixel_stride = framebuffer.stride / bpp;
    info.height = framebuffer.height;
    info.width = pixel_stride;
    info.pixels.resize(info.width * info.height * 4);

    for (u32 y = 0; y < info.height; y++) {
        for (u32 x = 0; x < info.width; x++) {
            const u8* pixel = framebuffer_data + (y * pixel_stride + pixel_stride - x) * bpp;
            const Common::Vec4 color = [&] {
                switch (framebuffer.color_format) {
                case Pica::PixelFormat::RGBA8:
                    return Common::Color::DecodeRGBA8(pixel);
                case Pica::PixelFormat::RGB8:
                    return Common::Color::DecodeRGB8(pixel);
                case Pica::PixelFormat::RGB565:
                    return Common::Color::DecodeRGB565(pixel);
                case Pica::PixelFormat::RGB5A1:
                    return Common::Color::DecodeRGB5A1(pixel);
                case Pica::PixelFormat::RGBA4:
                    return Common::Color::DecodeRGBA4(pixel);
                }
                UNREACHABLE();
            }();
            const u32 output_offset = (x * info.height + y) * 4;
            u8* dest = info.pixels.data() + output_offset;
            std::memcpy(dest, color.AsArray(), sizeof(color));
        }
    }
}

} // namespace SwRenderer
