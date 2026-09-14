// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>
#include <fmt/format.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "common/thread.h"
#include "core/core.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/guest_shutdown.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/apt.h"
#include "core/loader/loader.h"

namespace Core {

namespace {
using namespace std::chrono_literals;

/// How long to wait for the guest to read the notification before deciding it never will.
constexpr auto AcknowledgementGrace = 300ms;
constexpr auto PollSlice = 50ms;
/// A gap this long between savedata writes counts as the guest having finished writing.
constexpr auto SaveWriteQuiescence = 1s;
/// Cap on the search for an unused snapshot directory name.
constexpr int MaxSnapshotNameAttempts = 100;
/// Kept snapshots per title, so a title that keeps timing out cannot fill the disk.
constexpr std::size_t MaxKeptSnapshots = 5;
/// Upper bound on the wait for an in-flight SVC to give the HLE lock back.
constexpr int HleLockTimeoutMs = 250;

/// Under the user directory, not the cache: a cache is something the OS may delete.
std::string SnapshotDirectory() {
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) + "save_snapshots/";
}

/// Deletes the oldest snapshots of `program_id`, leaving room for one more. Names carry a
/// fixed-width timestamp, so they sort oldest first.
void PruneOldSnapshots(u64 program_id) {
    const std::string root = SnapshotDirectory();
    if (!FileUtil::IsDirectory(root)) {
        return;
    }

    FileUtil::FSTEntry listing{};
    FileUtil::ScanDirectoryTree(root, listing);

    const std::string prefix = fmt::format("{:016x}_", program_id);
    std::vector<std::string> mine;
    for (const auto& entry : listing.children) {
        if (entry.isDirectory && entry.virtualName.rfind(prefix, 0) == 0) {
            mine.push_back(entry.virtualName);
        }
    }
    if (mine.size() < MaxKeptSnapshots) {
        return;
    }

    std::sort(mine.begin(), mine.end());
    const std::size_t excess = mine.size() - (MaxKeptSnapshots - 1);
    for (std::size_t i = 0; i < excess; ++i) {
        LOG_INFO(Core, "Pruning old save data snapshot {}", mine[i]);
        FileUtil::DeleteDirRecursively(root + mine[i] + "/");
    }
}
} // Anonymous namespace

std::string_view DescribeGuestShutdown(GuestShutdownResult result) {
    switch (result) {
    case GuestShutdownResult::Clean:
        return "completed cleanly";
    case GuestShutdownResult::NotListening:
        return "was not acknowledged; the title does not handle it, stopping now";
    case GuestShutdownResult::NotDelivered:
        return "could not be requested at all; stopping without the guest knowing";
    case GuestShutdownResult::TimedOutIdle:
        return "timed out with no save in progress; stopping is safe";
    case GuestShutdownResult::TimedOutQuiet:
        return "timed out with an uncommitted save, but the guest had stopped writing";
    case GuestShutdownResult::TimedOutSaving:
        return "timed out while the guest was still saving; the save may be inconsistent";
    }
    return "finished in an unknown state";
}

GuestShutdownTimeouts GuestShutdownTimeouts::Minus(
    std::chrono::steady_clock::duration spent) const {
    const auto charge = [spent](std::chrono::milliseconds budget) {
        return std::max(budget - std::chrono::duration_cast<std::chrono::milliseconds>(spent),
                        std::chrono::milliseconds::zero());
    };
    return {charge(deadline), charge(mid_save_ceiling)};
}

GuestShutdownResult WaitForGuestShutdown(System& system, GuestShutdownTimeouts timeouts,
                                         const GuestShutdownWaitSlice& wait_slice) {
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + timeouts.deadline;
    const auto ceiling = start + std::max(timeouts.deadline, timeouts.mid_save_ceiling);
    // Clamped: the grace period is part of the freeze budget, not extra on top of it.
    const auto acknowledgement_deadline = std::min(start + AcknowledgementGrace, ceiling);

    while (true) {
        // Likewise the poll slice, so the last wait cannot overshoot the ceiling.
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            ceiling - std::chrono::steady_clock::now());
        const auto slice = std::clamp(remaining, std::chrono::milliseconds::zero(), PollSlice);
        if (wait_slice(slice)) {
            return GuestShutdownResult::Clean;
        }
        const auto now = std::chrono::steady_clock::now();

        const bool has_save = system.HasUncommittedGuestSave();
        // Before the acknowledgement: a title can autosave on its own timer without answering.
        if (has_save && now - system.LastGuestSaveWrite() < SaveWriteQuiescence) {
            if (now >= ceiling) {
                return GuestShutdownResult::TimedOutSaving;
            }
            continue;
        }

        // Only when nothing was written, so a title that saves without answering isn't cut off.
        if (!has_save && !system.WasGuestShutdownAcknowledged()) {
            if (now >= acknowledgement_deadline) {
                return GuestShutdownResult::NotListening;
            }
            continue;
        }

        if (now >= deadline) {
            return has_save ? GuestShutdownResult::TimedOutQuiet
                            : GuestShutdownResult::TimedOutIdle;
        }
    }
}

void SaveDataSnapshot::Take(System& system, std::chrono::milliseconds budget) {
    if (!system.IsPoweredOn()) {
        return;
    }

    // Nothing written since the last commit, so a stop has nothing to tear. Checked first: this is
    // the common case, and copying regardless makes every stop pay for a copy it then deletes.
    if (!system.HasUncommittedGuestSave()) {
        return;
    }

    u64 program_id{};
    if (system.GetAppLoader().ReadProgramId(program_id) != Loader::ResultStatus::Success) {
        return;
    }

    const std::string save_path = FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(
        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), program_id);
    if (!FileUtil::IsDirectory(save_path)) {
        // Nothing on the SD card to lose. Titles that save elsewhere aren't covered.
        return;
    }

    // Extra and system save data are tracked as at risk but live outside this directory, so
    // Finish() has to stop short of promising a copy that covers them.
    covers_everything = !system.HasUncommittedSaveOutsideSdSaveData();

    // The guest keeps running, so this may copy a save mid-write. Kept anyway: a torn copy still
    // beats the nothing the user is left with if the stop tears the real save.
    const auto copy_started = std::chrono::steady_clock::now();
    taken_mid_write = copy_started - system.LastGuestSaveWrite() < SaveWriteQuiescence;

    PruneOldSnapshots(program_id);

    // Timestamped so an earlier kept copy isn't clobbered; CopyDir merges into an existing dir.
    const std::string prefix = fmt::format("{}{:016x}_{}", SnapshotDirectory(), program_id,
                                           static_cast<u64>(std::time(nullptr)));
    std::string destination = prefix + "/";
    for (int attempt = 1; FileUtil::Exists(destination); ++attempt) {
        if (attempt > MaxSnapshotNameAttempts) {
            LOG_WARNING(Core, "Not snapshotting save data: no unused name left under {}", prefix);
            return;
        }
        destination = fmt::format("{}_{}/", prefix, attempt);
    }

    // CopyDir creates the destination first, so only its return value proves the copy happened.
    const auto copy_deadline = copy_started + budget;
    const bool copied = FileUtil::CopyDir(save_path, destination, [copy_deadline] {
        return std::chrono::steady_clock::now() < copy_deadline;
    });
    if (!copied) {
        LOG_WARNING(Core, "Could not snapshot save data to {} within {}ms", destination,
                    budget.count());
        // A partial copy would be offered to the user as a recoverable save.
        FileUtil::DeleteDirRecursively(destination);
        return;
    }
    path = destination;

    // It may also have started writing partway through a copy that began while it was quiet.
    taken_mid_write = taken_mid_write || system.LastGuestSaveWrite() >= copy_started;
    LOG_DEBUG(Core, "Snapshotted save data to {}", path);
}

SaveDataSnapshot::~SaveDataSnapshot() {
    Discard();
}

void SaveDataSnapshot::Finish(GuestShutdownResult result) {
    if (!MayHaveTornSave(result)) {
        Discard();
        return;
    }

    if (path.empty()) {
        // Nothing was at risk when the copy would have been taken, or it could not be made. Say
        // so: silence here reads as "the save is fine".
        LOG_WARNING(Core,
                    "Emulation was stopped and the guest may not have finished saving: it {}. No "
                    "copy of its save data was available to keep",
                    DescribeGuestShutdown(result));
        return;
    }

    std::string caveats;
    if (taken_mid_write) {
        caveats +=
            " The copy was made while the guest was writing, so it may be incomplete itself.";
    }
    if (!covers_everything) {
        caveats += " It covers only the title's SD card save data; extra or system save data was "
                   "also written and is not included.";
    }
    LOG_WARNING(Core,
                "Emulation was stopped and the guest may not have finished saving: it {}. Its "
                "save data as it was before the shutdown has been kept at {}.{}",
                DescribeGuestShutdown(result), path, caveats);
    path.clear(); // Don't delete it in the destructor.
}

void SaveDataSnapshot::Discard() {
    if (path.empty()) {
        return;
    }
    FileUtil::DeleteDirRecursively(path);
    path.clear();
}

GuestShutdownResult PerformGuestShutdown(System& system, GuestShutdownTimeouts timeouts,
                                         const GuestShutdownWaitSlice& wait_slice,
                                         const std::function<void()>& on_session_confirmed) {
    // Off by default: a title that reads the notification and then ignores it still costs the
    // whole budget, and most do. Worth turning on per game for the ones that buffer their save.
    if (!Settings::values.notify_guest_on_shutdown.GetValue()) {
        return GuestShutdownResult::NotDelivered;
    }

    const auto started = std::chrono::steady_clock::now();

    SaveDataSnapshot snapshot;
    {
        // Held across the IsPoweredOn() check. Both locks and the reference go before the wait:
        // System::Shutdown() runs on the emu thread and frees what they guard.
        std::scoped_lock session_guard{system.GetSessionLock()};
        if (!system.IsPoweredOn()) {
            return GuestShutdownResult::NotDelivered;
        }
        if (on_session_confirmed) {
            on_session_confirmed();
        }

        // Taken while the save is intact; under the session lock because it reads the app loader.
        snapshot.Take(system, timeouts.deadline);

        // An applet dialog holds the HLE lock while blocking on us, so never wait on it forever.
        std::unique_lock hle_guard{system.Kernel().GetHLELock(), std::defer_lock};
        if (!Common::TryLockFor(hle_guard, HleLockTimeoutMs)) {
            LOG_WARNING(Core, "HLE lock still held, stopping without notifying the guest");
            // Usually a long-running FS call holds it, so a save may be in flight. Keep the copy.
            snapshot.Finish(GuestShutdownResult::NotDelivered);
            return GuestShutdownResult::NotDelivered;
        }

        auto apt = Service::APT::GetModule(system);
        if (!apt) {
            snapshot.Finish(GuestShutdownResult::NotDelivered);
            return GuestShutdownResult::NotDelivered;
        }
        system.ResetGuestShutdownProgress();
        apt->GetAppletManager()->SendNotificationToAll(Service::APT::Notification::Shutdown);
    }

    const auto result = WaitForGuestShutdown(
        system, timeouts.Minus(std::chrono::steady_clock::now() - started), wait_slice);
    snapshot.Finish(result);
    LOG_INFO(Core, "Guest shutdown {}", DescribeGuestShutdown(result));
    return result;
}

} // namespace Core
