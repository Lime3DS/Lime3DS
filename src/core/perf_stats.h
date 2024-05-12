// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/thread.h"

namespace Core {

/**
 * Class to manage and query performance/timing statistics. All public functions of this class are
 * thread-safe unless stated otherwise.
 */
class PerfStats {
public:
    explicit PerfStats(u64 title_id);
    ~PerfStats();

    using Clock = std::chrono::high_resolution_clock;

    enum class PerfArticEventBits {
        NONE = 0,
        ARTIC_SAVE_DATA = (1 << 0),
        ARTIC_EXT_DATA = (1 << 1),
        ARTIC_BOSS_EXT_DATA = (1 << 2),
        ARTIC_SHARED_EXT_DATA = (1 << 3),
    };
    union PerfArticEvents {
        u32 raw{};
        BitField<0, 1, u32> artic_save_data;
        BitField<1, 1, u32> artic_ext_data;
        BitField<2, 1, u32> artic_boss_ext_data;
        BitField<3, 1, u32> artic_shared_ext_data;

        void Set(PerfArticEventBits event, bool set) {
            raw = (raw & ~static_cast<u32>(event)) | (set ? static_cast<u32>(event) : 0);
        }
        bool Get(PerfArticEventBits event) {
            return (raw & static_cast<u32>(event)) != 0;
        }
    };

    struct Results {
        /// System FPS (LCD VBlanks) in Hz
        double system_fps;
        /// Game FPS (GSP frame submissions) in Hz
        double game_fps;
        /// Walltime per system frame, in seconds, excluding any waits
        double frametime;
        /// Ratio of walltime / emulated time elapsed
        double emulation_speed;
        /// Artic base bytes per second
        double artic_transmitted = 0;
        /// Artic base events
        PerfArticEvents artic_events{};
    };

    void BeginSystemFrame();
    void EndSystemFrame();
    void EndGameFrame();

    Results GetAndResetStats(std::chrono::microseconds current_system_time_us);

    Results GetLastStats();

    /**
     * Returns the arithmetic mean of all frametime values stored in the performance history.
     */
    double GetMeanFrametime() const;

    /**
     * Gets the ratio between walltime and the emulated time of the previous system frame. This is
     * useful for scaling inputs or outputs moving between the two time domains.
     */
    double GetLastFrameTimeScale() const;

    void AddArticBaseTraffic(u32 bytes) {
        artic_transmitted += bytes;
    }

    void ReportPerfArticEvent(PerfArticEventBits event, bool set) {
        if (set) {
            artic_events.Set(event, set);
            prev_artic_event.Set(event, set);
        } else {
            artic_events.Set(event, set);
        }
    }

private:
    mutable std::mutex object_mutex;

    /// Title ID for the game that is running. 0 if there is no game running yet
    u64 title_id{0};
    /// Current index for writing to the perf_history array
    std::size_t current_index{0};
    /// Stores an hour of historical frametime data useful for processing and tracking performance
    /// regressions with code changes.
    std::array<double, 216000> perf_history{};

    /// Point when the cumulative counters were reset
    Clock::time_point reset_point = Clock::now();
    /// System time when the cumulative counters were reset
    std::chrono::microseconds reset_point_system_us{0};

    /// Cumulative duration (excluding v-sync/frame-limiting) of frames since last reset
    Clock::duration accumulated_frametime = Clock::duration::zero();
    /// Cumulative number of system frames (LCD VBlanks) presented since last reset
    u32 system_frames = 0;
    /// Cumulative number of game frames (GSP frame submissions) since last reset
    u32 game_frames = 0;
    /// Cumulative number of transmitted artic base traffic
    std::atomic<u32> artic_transmitted = 0;
    // System events that affect performance
    PerfArticEvents artic_events;

    PerfArticEvents prev_artic_event;

    /// Point when the previous system frame ended
    Clock::time_point previous_frame_end = reset_point;
    /// Point when the current system frame began
    Clock::time_point frame_begin = reset_point;
    /// Total visible duration (including frame-limiting, etc.) of the previous system frame
    Clock::duration previous_frame_length = Clock::duration::zero();

    /// Last recorded performance statistics.
    Results last_stats;
};

class FrameLimiter {
public:
    using Clock = std::chrono::high_resolution_clock;

    void DoFrameLimiting(std::chrono::microseconds current_system_time_us);

    bool IsFrameAdvancing() const;
    /**
     * Sets whether frame advancing is enabled or not.
     * Note: The frontend must cancel frame advancing before shutting down in order
     *       to resume the emu_thread.
     */
    void SetFrameAdvancing(bool value);
    void AdvanceFrame();
    void WaitOnce();

private:
    /// Emulated system time (in microseconds) at the last limiter invocation
    std::chrono::microseconds previous_system_time_us{0};
    /// Walltime at the last limiter invocation
    Clock::time_point previous_walltime = Clock::now();

    /// Accumulated difference between walltime and emulated time
    std::chrono::microseconds frame_limiting_delta_err{0};

    /// Whether to use frame advancing (i.e. frame by frame)
    std::atomic_bool frame_advancing_enabled;

    /// Event to advance the frame when frame advancing is enabled
    Common::Event frame_advance_event;
};

} // namespace Core
