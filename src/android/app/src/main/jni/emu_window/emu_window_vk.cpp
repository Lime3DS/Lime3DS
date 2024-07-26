// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdlib>
#include <android/native_window_jni.h>
#include "common/logging/log.h"
#include "common/settings.h"
#include "jni/emu_window/emu_window_vk.h"

class GraphicsContext_Android final : public Frontend::GraphicsContext {
public:
    explicit GraphicsContext_Android(std::shared_ptr<Common::DynamicLibrary> driver_library_)
        : driver_library{driver_library_} {}

    ~GraphicsContext_Android() = default;

    std::shared_ptr<Common::DynamicLibrary> GetDriverLibrary() override {
        return driver_library;
    }

private:
    std::shared_ptr<Common::DynamicLibrary> driver_library;
};

EmuWindow_Android_Vulkan::EmuWindow_Android_Vulkan(
    ANativeWindow* surface, std::shared_ptr<Common::DynamicLibrary> driver_library_)
    : EmuWindow_Android{surface}, driver_library{driver_library_} {
    CreateWindowSurface();

    if (core_context = CreateSharedContext(); !core_context) {
        LOG_CRITICAL(Frontend, "CreateSharedContext() failed");
        return;
    }

    OnFramebufferSizeChanged();
}

bool EmuWindow_Android_Vulkan::CreateWindowSurface() {
    if (!host_window) {
        return true;
    }

    window_info.type = Frontend::WindowSystemType::Android;
    window_info.render_surface = host_window;

    return true;
}

std::unique_ptr<Frontend::GraphicsContext> EmuWindow_Android_Vulkan::CreateSharedContext() const {
    return std::make_unique<GraphicsContext_Android>(driver_library);
}

std::shared_ptr<Common::DynamicLibrary> EmuWindow_Android_Vulkan::GetDriverLibrary() {
    return driver_library;
}
