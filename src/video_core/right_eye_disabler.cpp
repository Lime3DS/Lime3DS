// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "right_eye_disabler.h"
#include "video_core/gpu.h"
#include "video_core/gpu_impl.h"

namespace VideoCore {
bool RightEyeDisabler::ShouldAllowCmdQueueTrigger(PAddr addr, u32 size) {
    if (!enabled || !enable_for_frame)
        return true;

    constexpr u32 top_screen_size = 0x00469000;

    if (report_end_frame_pending) {
        ReportEndFrame();
        report_end_frame_pending = false;
    }
    cmd_queue_trigger_happened = true;

    auto guess = gpu.impl->pica.GuessCmdRenderProperties(addr, size);
    if (guess.has_draw && guess.vp_height == top_screen_size && !top_screen_blocked) {
        if (top_screen_buf == 0) {
            top_screen_buf = guess.paddr;
        }
        top_screen_drawn = true;
        if (top_screen_transfered) {
            cmd_trigger_blocked = true;
            return false;
        }
    }

    cmd_trigger_blocked = false;
    return true;
}
bool RightEyeDisabler::ShouldAllowDisplayTransfer(PAddr src_address, size_t size) {
    if (!enabled || !enable_for_frame)
        return true;

    if (size >= 400 && !top_screen_blocked) {
        if (top_screen_drawn && src_address == top_screen_buf) {
            top_screen_transfered = true;
        }

        if (src_address == top_screen_buf && cmd_trigger_blocked) {
            top_screen_blocked = true;
            return false;
        }
    }

    if (cmd_queue_trigger_happened)
        display_tranfer_happened = true;
    return true;
}
void RightEyeDisabler::ReportEndFrame() {
    if (!enabled)
        return;

    enable_for_frame = Settings::values.disable_right_eye_render.GetValue();

    if (display_tranfer_happened) {
        top_screen_drawn = false;
        top_screen_transfered = false;
        top_screen_blocked = false;
        cmd_queue_trigger_happened = false;
        cmd_trigger_blocked = false;
        display_tranfer_happened = false;
        top_screen_buf = 0;
    } else {
        report_end_frame_pending = true;
    }
}
} // namespace VideoCore
