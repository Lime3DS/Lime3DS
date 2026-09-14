// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <string_view>

namespace Core {

class System;

enum class GuestShutdownResult {
    Clean,          ///< The guest saved and exited on its own.
    NotListening,   ///< The guest never read the notification, so it never wrote anything.
    NotDelivered,   ///< The guest was never told at all, so anything in flight was cut off.
    TimedOutIdle,   ///< Timed out, but with no save outstanding.
    TimedOutQuiet,  ///< Timed out with a save written but never committed, and writes long over.
    TimedOutSaving, ///< Timed out mid-save. The save may be inconsistent.
};

[[nodiscard]] constexpr bool MayHaveTornSave(GuestShutdownResult result) {
    // Not TimedOutQuiet: plenty of titles write their save and never commit it, so that flag stays
    // set for the rest of the session. Having gone quiet for SaveWriteQuiescence is the whole
    // basis for calling the write finished, so keeping a copy and warning about it anyway would
    // fire on every stop of those titles and mean nothing when it did.
    // NotDelivered is kept: we never learned what the guest was doing, and a spare copy is cheap.
    return result == GuestShutdownResult::TimedOutSaving ||
           result == GuestShutdownResult::NotDelivered;
}

/// Human-readable form of the result, for logging.
[[nodiscard]] std::string_view DescribeGuestShutdown(GuestShutdownResult result);

/// Blocks for up to the given duration. Returns true once emulation has ended.
using GuestShutdownWaitSlice = std::function<bool(std::chrono::milliseconds)>;

struct GuestShutdownTimeouts {
    std::chrono::milliseconds deadline;
    /// Replaces the deadline while the guest is mid-save. Frontends that block a UI thread have
    /// to keep this short enough that the OS doesn't kill them for going unresponsive.
    std::chrono::milliseconds mid_save_ceiling;

    /// Charges time already spent against both, clamped at zero: the budget is freeze time.
    [[nodiscard]] GuestShutdownTimeouts Minus(std::chrono::steady_clock::duration spent) const;
};

/**
 * Waits for the guest to exit after being sent an APT shutdown notification. Extends the wait
 * while it is actively writing its save, and gives up early if it never reads the notification.
 * "Actively" means recent writes: not every archive sends a commit, so a lull has to count as
 * finished. Call System::ResetGuestShutdownProgress() just before sending the notification.
 *
 * Never blocks past the larger of the two budgets, the acknowledgement grace period included: all
 * of it is time a frontend's UI thread spends frozen.
 */
GuestShutdownResult WaitForGuestShutdown(System& system, GuestShutdownTimeouts timeouts,
                                         const GuestShutdownWaitSlice& wait_slice);

/// Copy of the running title's save data, taken while still intact and kept only if the stop
/// turned out to interrupt a save. Stands in for the atomicity Azahar doesn't emulate.
class SaveDataSnapshot {
public:
    SaveDataSnapshot() = default;
    ~SaveDataSnapshot();

    SaveDataSnapshot(const SaveDataSnapshot&) = delete;
    SaveDataSnapshot& operator=(const SaveDataSnapshot&) = delete;

    /**
     * Copies the save aside, but only when the guest has an uncommitted write to lose. An intact
     * save needs no copy, and checking first is what keeps an ordinary stop from paying for a full
     * copy and delete of the title's save directory for nothing.
     * Call with System::GetSessionLock() held: it reads the app loader.
     * @param budget Wall-clock cap on the copy, which is otherwise unbounded and runs on a
     *               frontend's UI thread. A copy that runs out of budget is abandoned.
     */
    void Take(System& system, std::chrono::milliseconds budget);

    /// Keeps the copy if the save may have been torn, otherwise deletes it.
    void Finish(GuestShutdownResult result);

private:
    void Discard();

    std::string path;              ///< Empty if no snapshot was taken.
    bool taken_mid_write = false;  ///< The guest wrote to the save while it was being copied.
    bool covers_everything = true; ///< No at-risk archive fell outside what was copied.
};

/**
 * Asks the running title to save and exit, then waits for it, taking a save data snapshot
 * beforehand and resolving it afterwards. Only `wait_slice` differs between frontends.
 *
 * Call with neither System::GetSessionLock() nor the HLE lock held: this takes both, and drops
 * them before waiting, since System::Shutdown() runs on the emu thread and needs them.
 *
 * @param on_session_confirmed Run under the session lock once the session is known to still be
 *                             live, before anything else touches it. Frontends resume emulation
 *                             here: a paused guest never sees the notification, and doing it any
 *                             earlier would leave a session that has already gone away resumed.
 *                             Must not block.
 */
GuestShutdownResult PerformGuestShutdown(System& system, GuestShutdownTimeouts timeouts,
                                         const GuestShutdownWaitSlice& wait_slice,
                                         const std::function<void()>& on_session_confirmed = {});

} // namespace Core
