// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <boost/serialization/array.hpp>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NDM {

class NDM_U final : public ServiceFramework<NDM_U> {
public:
    NDM_U();

private:
    /**
     *  NDM::EnterExclusiveState service function
     *  Inputs:
     *      0 : Header code [0x00010042]
     *      1 : Exclusive State
     *      2 : 0x20
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void EnterExclusiveState(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::LeaveExclusiveState service function
     *  Inputs:
     *      0 : Header code [0x00020002]
     *      1 : 0x20
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void LeaveExclusiveState(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::QueryExclusiveMode service function
     *  Inputs:
     *      0 : Header code [0x00030000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     *      2 : Current Exclusive State
     */
    void QueryExclusiveMode(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::LockState service function
     *  Inputs:
     *      0 : Header code [0x00040002]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void LockState(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::UnlockState service function
     *  Inputs:
     *      0 : Header code [0x00050002]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void UnlockState(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::SuspendDaemons service function
     *  Inputs:
     *      0 : Header code [0x00060040]
     *      1 : Daemon bit mask
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void SuspendDaemons(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::ResumeDaemons service function
     *  Inputs:
     *      0 : Header code [0x00070040]
     *      1 : Daemon bit mask
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void ResumeDaemons(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::SuspendScheduler service function
     *  Inputs:
     *      0 : Header code [0x00080040]
     *      1 : (u8/bool) 0 = Wait for completion, 1 = Perform in background
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void SuspendScheduler(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::ResumeScheduler service function
     *  Inputs:
     *      0 : Header code [0x00090000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void ResumeScheduler(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::QueryStatus service function
     *  Inputs:
     *      0 : Header code [0x000D0040]
     *      1 : Daemon
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     *      2 : Daemon status
     */
    void QueryStatus(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::GetDaemonDisableCount service function
     *  Inputs:
     *      0 : Header code [0x000E0040]
     *      1 : Daemon
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     *      2 : Current process disable count
     *      3 : Total disable count
     */
    void GetDaemonDisableCount(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::GetSchedulerDisableCount service function
     *  Inputs:
     *      0 : Header code [0x000F0000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     *      2 : Current process disable count
     *      3 : Total disable count
     */
    void GetSchedulerDisableCount(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::SetScanInterval service function
     *  Inputs:
     *      0 : Header code [0x00100040]
     *      1 : Interval (default = 30)
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void SetScanInterval(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::GetScanInterval service function
     *  Inputs:
     *      0 : Header code [0x00110000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     *      2 : Interval (default = 30)
     */
    void GetScanInterval(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::SetRetryInterval service function
     *  Inputs:
     *      0 : Header code [0x00120040]
     *      1 : Interval (default = 10)
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void SetRetryInterval(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::GetRetryInterval service function
     *  Inputs:
     *      0 : Header code [0x00130000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     *      2 : Interval (default = 10)
     */
    void GetRetryInterval(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::OverrideDefaultDaemons service function
     *  Inputs:
     *      0 : Header code [0x00140040]
     *      1 : Daemon bit mask
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void OverrideDefaultDaemons(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::ResetDefaultDaemons service function
     *  Inputs:
     *      0 : Header code [0x00150000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void ResetDefaultDaemons(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::GetDefaultDaemons service function
     *  Inputs:
     *      0 : Header code [0x00160000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     *      2 : Daemon bit mask
     *  Note:
     *      Gets the current default daemon bit mask. The default value is (DAEMONMASK_CEC |
     * DAEMONMASK_FRIENDS)
     */
    void GetDefaultDaemons(Kernel::HLERequestContext& ctx);

    /**
     *  NDM::ClearHalfAwakeMacFilter service function
     *  Inputs:
     *      0 : Header code [0x00170000]
     *  Outputs:
     *      1 : Result, 0 on success, otherwise error code
     */
    void ClearHalfAwakeMacFilter(Kernel::HLERequestContext& ctx);

    enum class Daemon : u32 {
        Cec = 0,
        Boss = 1,
        Nim = 2,
        Friend = 3,
    };

    enum class DaemonMask : u32 {
        None = 0,
        Cec = (1 << static_cast<u32>(Daemon::Cec)),
        Boss = (1 << static_cast<u32>(Daemon::Boss)),
        Nim = (1 << static_cast<u32>(Daemon::Nim)),
        Friend = (1 << static_cast<u32>(Daemon::Friend)),
        Default = Cec | Friend,
        All = Cec | Boss | Nim | Friend,
    };

    enum class DaemonStatus : u32 {
        Busy = 0,
        Idle = 1,
        Suspending = 2,
        Suspended = 3,
    };

    enum class ExclusiveState : u32 {
        None = 0,
        Infrastructure = 1,
        LocalCommunications = 2,
        Streetpass = 3,
        StreetpassData = 4,
    };

    enum : u32 {
        DEFAULT_RETRY_INTERVAL = 10,
        DEFAULT_SCAN_INTERVAL = 30,
    };

    DaemonMask daemon_bit_mask = DaemonMask::Default;
    DaemonMask default_daemon_bit_mask = DaemonMask::Default;
    std::array<DaemonStatus, 4> daemon_status = {
        DaemonStatus::Idle,
        DaemonStatus::Idle,
        DaemonStatus::Idle,
        DaemonStatus::Idle,
    };
    ExclusiveState exclusive_state = ExclusiveState::None;
    u32 scan_interval = DEFAULT_SCAN_INTERVAL;
    u32 retry_interval = DEFAULT_RETRY_INTERVAL;
    bool daemon_lock_enabled = false;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar & daemon_bit_mask;
        ar & default_daemon_bit_mask;
        ar & daemon_status;
        ar & exclusive_state;
        ar & scan_interval;
        ar & retry_interval;
        ar & daemon_lock_enabled;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::NDM

BOOST_CLASS_EXPORT_KEY(Service::NDM::NDM_U)
