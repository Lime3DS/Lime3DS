// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_rect.h>
#include "citra/emu_window/emu_window_sdl2_sw.h"
#include "common/scm_rev.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "video_core/gpu.h"
#include "video_core/renderer_software/renderer_software.h"

class DummyContext : public Frontend::GraphicsContext {};

EmuWindow_SDL2_SW::EmuWindow_SDL2_SW(Core::System& system_, bool fullscreen, bool is_secondary)
    : EmuWindow_SDL2{system_, is_secondary}, system{system_} {
    std::string window_title = fmt::format("Lime {} | {}-{}", Common::g_build_fullname,
                                           Common::g_scm_branch, Common::g_scm_desc);
    render_window =
        SDL_CreateWindow(window_title.c_str(),
                         SDL_WINDOWPOS_UNDEFINED, // x position
                         SDL_WINDOWPOS_UNDEFINED, // y position
                         Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight,
                         SDL_WINDOW_SHOWN);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window: {}", SDL_GetError());
        exit(1);
    }

    window_surface = SDL_GetWindowSurface(render_window);
    renderer = SDL_CreateSoftwareRenderer(window_surface);

    if (renderer == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 software renderer: {}", SDL_GetError());
        exit(1);
    }

    if (fullscreen) {
        Fullscreen();
    }

    render_window_id = SDL_GetWindowID(render_window);

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
}

EmuWindow_SDL2_SW::~EmuWindow_SDL2_SW() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(render_window);
}

std::unique_ptr<Frontend::GraphicsContext> EmuWindow_SDL2_SW::CreateSharedContext() const {
    return std::make_unique<DummyContext>();
}

void EmuWindow_SDL2_SW::Present() {
    const auto layout{Layout::DefaultFrameLayout(
        Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight, false, false)};

    using VideoCore::ScreenId;

    while (IsOpen()) {
        SDL_SetRenderDrawColor(renderer,
                               static_cast<Uint8>(Settings::values.bg_red.GetValue() * 255),
                               static_cast<Uint8>(Settings::values.bg_green.GetValue() * 255),
                               static_cast<Uint8>(Settings::values.bg_blue.GetValue() * 255), 0xFF);
        SDL_RenderClear(renderer);

        const auto draw_screen = [&](ScreenId screen_id) {
            const auto dst_rect =
                screen_id == ScreenId::TopLeft ? layout.top_screen : layout.bottom_screen;
            SDL_Rect sdl_rect{static_cast<int>(dst_rect.left), static_cast<int>(dst_rect.top),
                              static_cast<int>(dst_rect.GetWidth()),
                              static_cast<int>(dst_rect.GetHeight())};
            SDL_Surface* screen = LoadFramebuffer(screen_id);
            SDL_BlitSurface(screen, nullptr, window_surface, &sdl_rect);
            SDL_FreeSurface(screen);
        };

        draw_screen(ScreenId::TopLeft);
        draw_screen(ScreenId::Bottom);

        SDL_RenderPresent(renderer);
        SDL_UpdateWindowSurface(render_window);
    }
}

SDL_Surface* EmuWindow_SDL2_SW::LoadFramebuffer(VideoCore::ScreenId screen_id) {
    const auto& renderer = static_cast<SwRenderer::RendererSoftware&>(system.GPU().Renderer());
    const auto& info = renderer.Screen(screen_id);
    const int width = static_cast<int>(info.width);
    const int height = static_cast<int>(info.height);
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, width, height, 0, SDL_PIXELFORMAT_ABGR8888);
    SDL_LockSurface(surface);
    std::memcpy(surface->pixels, info.pixels.data(), info.pixels.size());
    SDL_UnlockSurface(surface);
    return surface;
}
