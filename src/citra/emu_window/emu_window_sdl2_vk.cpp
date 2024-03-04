// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstdlib>
#include <memory>
#include <string>
#include <SDL.h>
#include <SDL_syswm.h>
#include <fmt/format.h>
#include "citra/emu_window/emu_window_sdl2_vk.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "core/frontend/emu_window.h"

class DummyContext : public Frontend::GraphicsContext {};

EmuWindow_SDL2_VK::EmuWindow_SDL2_VK(Core::System& system, bool fullscreen, bool is_secondary)
    : EmuWindow_SDL2{system, is_secondary} {
    const std::string window_title = fmt::format("Lime {} | {}-{}", Common::g_build_fullname,
                                                 Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         SDL_WINDOWPOS_UNDEFINED, // x position
                         SDL_WINDOWPOS_UNDEFINED, // y position
                         Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_SysWMinfo wm;
    SDL_VERSION(&wm.version);
    if (SDL_GetWindowWMInfo(render_window, &wm) == SDL_FALSE) {
        LOG_CRITICAL(Frontend, "Failed to get information from the window manager");
        std::exit(EXIT_FAILURE);
    }

    if (fullscreen) {
        Fullscreen();
        SDL_ShowCursor(false);
    }

    switch (wm.subsystem) {
#ifdef SDL_VIDEO_DRIVER_WINDOWS
    case SDL_SYSWM_TYPE::SDL_SYSWM_WINDOWS:
        window_info.type = Frontend::WindowSystemType::Windows;
        window_info.render_surface = reinterpret_cast<void*>(wm.info.win.window);
        break;
#endif
#ifdef SDL_VIDEO_DRIVER_X11
    case SDL_SYSWM_TYPE::SDL_SYSWM_X11:
        window_info.type = Frontend::WindowSystemType::X11;
        window_info.display_connection = wm.info.x11.display;
        window_info.render_surface = reinterpret_cast<void*>(wm.info.x11.window);
        break;
#endif
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    case SDL_SYSWM_TYPE::SDL_SYSWM_WAYLAND:
        window_info.type = Frontend::WindowSystemType::Wayland;
        window_info.display_connection = wm.info.wl.display;
        window_info.render_surface = wm.info.wl.surface;
        break;
#endif
#ifdef SDL_VIDEO_DRIVER_COCOA
    case SDL_SYSWM_TYPE::SDL_SYSWM_COCOA:
        window_info.type = Frontend::WindowSystemType::MacOS;
        window_info.render_surface = SDL_Metal_GetLayer(SDL_Metal_CreateView(render_window));
        break;
#endif
#ifdef SDL_VIDEO_DRIVER_ANDROID
    case SDL_SYSWM_TYPE::SDL_SYSWM_ANDROID:
        window_info.type = Frontend::WindowSystemType::Android;
        window_info.render_surface = reinterpret_cast<void*>(wm.info.android.window);
        break;
#endif
    default:
        LOG_CRITICAL(Frontend, "Window manager subsystem {} not implemented", wm.subsystem);
        std::exit(EXIT_FAILURE);
        break;
    }

    render_window_id = SDL_GetWindowID(render_window);

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
}

EmuWindow_SDL2_VK::~EmuWindow_SDL2_VK() = default;

std::unique_ptr<Frontend::GraphicsContext> EmuWindow_SDL2_VK::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}
