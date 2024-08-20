// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstdlib>
#include <string>
#include <android/native_window_jni.h>
#include "common/logging/log.h"
#include "common/settings.h"
#include "input_common/main.h"
#include "jni/emu_window/emu_window.h"
#include "jni/id_cache.h"
#include "jni/input_manager.h"
#include "network/network.h"
#include "video_core/renderer_base.h"

static bool IsPortraitMode() {
    return JNI_FALSE != IDCache::GetEnvForThread()->CallStaticBooleanMethod(
                            IDCache::GetNativeLibraryClass(), IDCache::GetIsPortraitMode());
}

bool EmuWindow_Android::OnSurfaceChanged(ANativeWindow* surface) {
    if (render_window == surface) {
        return false;
    }

    render_window = surface;

    window_info.type = Frontend::WindowSystemType::Android;
    window_info.render_surface = surface;

    StopPresenting();
    OnFramebufferSizeChanged();

    return true;
}

bool EmuWindow_Android::OnTouchEvent(int x, int y, bool pressed) {
    if (pressed) {
        return TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    }

    TouchReleased();
    return true;
}

void EmuWindow_Android::OnTouchMoved(int x, int y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
}

void EmuWindow_Android::OnFramebufferSizeChanged() {
    const bool is_portrait_mode{IsPortraitMode()};

    const int bigger{window_width > window_height ? window_width : window_height};
    const int smaller{window_width < window_height ? window_width : window_height};
    if (is_portrait_mode) {
        UpdateCurrentFramebufferLayout(smaller, bigger, is_portrait_mode);
    } else {
        UpdateCurrentFramebufferLayout(bigger, smaller, is_portrait_mode);
    }
}

EmuWindow_Android::EmuWindow_Android(ANativeWindow* surface) : host_window{surface} {
    LOG_DEBUG(Frontend, "Initializing EmuWindow_Android");

    if (!surface) {
        LOG_CRITICAL(Frontend, "surface is nullptr");
        return;
    }

    window_width = ANativeWindow_getWidth(surface);
    window_height = ANativeWindow_getHeight(surface);

    Network::Init();
}

EmuWindow_Android::~EmuWindow_Android() {
    DestroyWindowSurface();
    DestroyContext();
}

void EmuWindow_Android::MakeCurrent() {
    core_context->MakeCurrent();
}

void EmuWindow_Android::DoneCurrent() {
    core_context->DoneCurrent();
}
