// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "citra/emu_window/emu_window_sdl2.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "core/core.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "network/network.h"

void EmuWindow_SDL2::OnMouseMotion(s32 x, s32 y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    InputCommon::GetMotionEmu()->Tilt(x, y);
}

void EmuWindow_SDL2::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    if (button == SDL_BUTTON_LEFT) {
        if (state == SDL_PRESSED) {
            TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
        } else {
            TouchReleased();
        }
    } else if (button == SDL_BUTTON_RIGHT) {
        if (state == SDL_PRESSED) {
            InputCommon::GetMotionEmu()->BeginTilt(x, y);
        } else {
            InputCommon::GetMotionEmu()->EndTilt();
        }
    }
}

std::pair<unsigned, unsigned> EmuWindow_SDL2::TouchToPixelPos(float touch_x, float touch_y) const {
    int w, h;
    SDL_GetWindowSize(render_window, &w, &h);

    touch_x *= w;
    touch_y *= h;

    return {static_cast<unsigned>(std::max(std::round(touch_x), 0.0f)),
            static_cast<unsigned>(std::max(std::round(touch_y), 0.0f))};
}

void EmuWindow_SDL2::OnFingerDown(float x, float y) {
    // TODO(NeatNit): keep track of multitouch using the fingerID and a dictionary of some kind
    // This isn't critical because the best we can do when we have that is to average them, like the
    // 3DS does

    const auto [px, py] = TouchToPixelPos(x, y);
    TouchPressed(px, py);
}

void EmuWindow_SDL2::OnFingerMotion(float x, float y) {
    const auto [px, py] = TouchToPixelPos(x, y);
    TouchMoved(px, py);
}

void EmuWindow_SDL2::OnFingerUp() {
    TouchReleased();
}

void EmuWindow_SDL2::OnKeyEvent(int key, u8 state) {
    if (state == SDL_PRESSED) {
        InputCommon::GetKeyboard()->PressKey(key);
    } else if (state == SDL_RELEASED) {
        InputCommon::GetKeyboard()->ReleaseKey(key);
    }
}

bool EmuWindow_SDL2::IsOpen() const {
    return is_open;
}

void EmuWindow_SDL2::RequestClose() {
    is_open = false;
}

void EmuWindow_SDL2::OnResize() {
    int width, height;
    SDL_GL_GetDrawableSize(render_window, &width, &height);
    UpdateCurrentFramebufferLayout(width, height);
}

void EmuWindow_SDL2::Fullscreen() {
    if (SDL_SetWindowFullscreen(render_window, SDL_WINDOW_FULLSCREEN) == 0) {
        return;
    }

    LOG_ERROR(Frontend, "Fullscreening failed: {}", SDL_GetError());

    // Try a different fullscreening method
    LOG_INFO(Frontend, "Attempting to use borderless fullscreen...");
    if (SDL_SetWindowFullscreen(render_window, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
        return;
    }

    LOG_ERROR(Frontend, "Borderless fullscreening failed: {}", SDL_GetError());

    // Fallback algorithm: Maximise window.
    // Works on all systems (unless something is seriously wrong), so no fallback for this one.
    LOG_INFO(Frontend, "Falling back on a maximised window...");
    SDL_MaximizeWindow(render_window);
}

EmuWindow_SDL2::EmuWindow_SDL2(Core::System& system_, bool is_secondary)
    : EmuWindow(is_secondary), system(system_) {}

EmuWindow_SDL2::~EmuWindow_SDL2() {
    SDL_Quit();
}

void EmuWindow_SDL2::InitializeSDL2() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL2: {}! Exiting...", SDL_GetError());
        exit(1);
    }

    InputCommon::Init();
    Network::Init();

    SDL_SetMainReady();
}

u32 EmuWindow_SDL2::GetEventWindowId(const SDL_Event& event) const {
    switch (event.type) {
    case SDL_WINDOWEVENT:
        return event.window.windowID;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        return event.key.windowID;
    case SDL_MOUSEMOTION:
        return event.motion.windowID;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        return event.button.windowID;
    case SDL_MOUSEWHEEL:
        return event.wheel.windowID;
    case SDL_FINGERDOWN:
    case SDL_FINGERMOTION:
    case SDL_FINGERUP:
        return event.tfinger.windowID;
    case SDL_TEXTEDITING:
        return event.edit.windowID;
    case SDL_TEXTEDITING_EXT:
        return event.editExt.windowID;
    case SDL_TEXTINPUT:
        return event.text.windowID;
    case SDL_DROPBEGIN:
    case SDL_DROPFILE:
    case SDL_DROPTEXT:
    case SDL_DROPCOMPLETE:
        return event.drop.windowID;
    case SDL_USEREVENT:
        return event.user.windowID;
    default:
        // Event is not for any particular window, so we can just pretend it's for this one.
        return render_window_id;
    }
}

void EmuWindow_SDL2::PollEvents() {
    SDL_Event event;
    std::vector<SDL_Event> other_window_events;

    // SDL_PollEvent returns 0 when there are no more events in the event queue
    while (SDL_PollEvent(&event)) {
        if (GetEventWindowId(event) != render_window_id) {
            other_window_events.push_back(event);
            continue;
        }

        switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MINIMIZED:
                OnResize();
                break;
            case SDL_WINDOWEVENT_CLOSE:
                RequestClose();
                break;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            OnKeyEvent(static_cast<int>(event.key.keysym.scancode), event.key.state);
            break;
        case SDL_MOUSEMOTION:
            // ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID)
                OnMouseMotion(event.motion.x, event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            // ignore if it came from touch
            if (event.button.which != SDL_TOUCH_MOUSEID) {
                OnMouseButton(event.button.button, event.button.state, event.button.x,
                              event.button.y);
            }
            break;
        case SDL_FINGERDOWN:
            OnFingerDown(event.tfinger.x, event.tfinger.y);
            break;
        case SDL_FINGERMOTION:
            OnFingerMotion(event.tfinger.x, event.tfinger.y);
            break;
        case SDL_FINGERUP:
            OnFingerUp();
            break;
        case SDL_QUIT:
            RequestClose();
            break;
        default:
            break;
        }
    }
    for (auto& e : other_window_events) {
        // This is a somewhat hacky workaround to re-emit window events meant for another window
        // since SDL_PollEvent() is global but we poll events per window.
        SDL_PushEvent(&e);
    }
    if (!is_secondary) {
        UpdateFramerateCounter();
    }
}

void EmuWindow_SDL2::OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) {
    SDL_SetWindowMinimumSize(render_window, minimal_size.first, minimal_size.second);
}

void EmuWindow_SDL2::UpdateFramerateCounter() {
    const u32 current_time = SDL_GetTicks();
    if (current_time > last_time + 2000) {
        const auto results = system.GetAndResetPerfStats();
        const auto title =
            fmt::format("Lime {} | {}-{} | FPS: {:.0f} ({:.0f}%)", Common::g_build_fullname,
                        Common::g_scm_branch, Common::g_scm_desc, results.game_fps,
                        results.emulation_speed * 100.0f);
        SDL_SetWindowTitle(render_window, title.c_str());
        last_time = current_time;
    }
}
