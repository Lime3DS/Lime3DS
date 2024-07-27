// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/settings.h"
#include "video_core/gpu.h"
#ifdef ENABLE_OPENGL
#include "video_core/renderer_opengl/renderer_opengl.h"
#endif
#ifdef ENABLE_SOFTWARE_RENDERER
#include "video_core/renderer_software/renderer_software.h"
#endif
#ifdef ENABLE_VULKAN
#include "video_core/renderer_vulkan/renderer_vulkan.h"
#endif
#include "video_core/video_core.h"

namespace VideoCore {

std::unique_ptr<RendererBase> CreateRenderer(Frontend::EmuWindow& emu_window,
                                             Frontend::EmuWindow* secondary_window,
                                             Pica::PicaCore& pica, Core::System& system) {
    const Settings::GraphicsAPI graphics_api = Settings::values.graphics_api.GetValue();
    switch (graphics_api) {
#ifdef ENABLE_SOFTWARE_RENDERER
    case Settings::GraphicsAPI::Software:
        return std::make_unique<SwRenderer::RendererSoftware>(system, pica, emu_window);
#endif
#ifdef ENABLE_VULKAN
    case Settings::GraphicsAPI::Vulkan:
        return std::make_unique<Vulkan::RendererVulkan>(system, pica, emu_window, secondary_window);
#endif
#ifdef ENABLE_OPENGL
    case Settings::GraphicsAPI::OpenGL:
        return std::make_unique<OpenGL::RendererOpenGL>(system, pica, emu_window, secondary_window);
#endif
    default:
        LOG_CRITICAL(Render,
                     "Unknown or unsupported graphics API {}, falling back to available default",
                     graphics_api);
#ifdef ENABLE_OPENGL
        return std::make_unique<OpenGL::RendererOpenGL>(system, pica, emu_window, secondary_window);
#elif ENABLE_VULKAN
        return std::make_unique<Vulkan::RendererVulkan>(system, pica, emu_window, secondary_window);
#elif ENABLE_SOFTWARE_RENDERER
        return std::make_unique<SwRenderer::RendererSoftware>(system, pica, emu_window);
#else
// TODO: Add a null renderer backend for this, perhaps.
#error "At least one renderer must be enabled."
#endif
    }
}

} // namespace VideoCore
