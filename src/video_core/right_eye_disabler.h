// Copyright 2024-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace VideoCore {
class GPU;

class RightEyeDisabler {
public:
    RightEyeDisabler(GPU& gpu) : gpu{gpu} {}

    bool ShouldAllowCmdQueueTrigger(PAddr addr, u32 size);
    bool ShouldAllowDisplayTransfer(PAddr src_address, size_t size);

    void ReportEndFrame();

    void SetEnabled(bool enable) {
        enabled = enable;
    }

private:
    bool enabled = true;
    // Enabled by ReportEndFrame() once the user setting has been read.
    bool enable_for_frame = false;

    bool top_screen_drawn = false;
    bool top_screen_transfered = false;
    bool top_screen_blocked = false;
    bool cmd_trigger_blocked = false;
    PAddr top_screen_buf = 0;

    bool cmd_queue_trigger_happened = false;
    bool display_tranfer_happened = false;
    bool report_end_frame_pending = false;

    GPU& gpu;
};
} // namespace VideoCore