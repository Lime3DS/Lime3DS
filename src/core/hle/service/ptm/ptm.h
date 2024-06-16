// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PTM {

/// Id of the SharedExtData archive used by the PTM process
constexpr std::array<u8, 12> ptm_shared_extdata_id = {0, 0, 0, 0, 0x0B, 0, 0, 0xF0, 0, 0, 0, 0};

/// Charge levels used by PTM functions
enum class ChargeLevels : u32 {
    CriticalBattery = 1,
    LowBattery = 2,
    HalfFull = 3,
    MostlyFull = 4,
    CompletelyFull = 5,
};

/**
 * Represents the gamecoin file structure in the SharedExtData archive
 * More information in 3dbrew
 * (http://www.3dbrew.org/wiki/Extdata#Shared_Extdata_0xf000000b_gamecoin.dat)
 */
struct GameCoin {
    u32 magic;               ///< Magic number: 0x4F00
    u16 total_coins;         ///< Total Play Coins
    u16 total_coins_on_date; ///< Total Play Coins obtained on the date stored below.
    u32 step_count;          ///< Total step count at the time a new Play Coin was obtained.
    u32 last_step_count;     ///< Step count for the day the last Play Coin was obtained
    u16 year;
    u8 month;
    u8 day;
};

void CheckNew3DS(IPC::RequestBuilder& rb);

class Module final {
public:
    explicit Module(Core::System& system_);
    ~Module() = default;

    static u16 GetPlayCoins();
    static void SetPlayCoins(u16 play_coins);

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> ptm, const char* name, u32 max_session);

    protected:
        /**
         * It is unknown if GetAdapterState is the same as GetBatteryChargeState,
         * it is likely to just be a duplicate function of GetBatteryChargeState
         * that controls another part of the HW.
         * PTM::GetAdapterState service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output of function, 0 = not charging, 1 = charging.
         */
        void GetAdapterState(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetShellState service function.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Whether the 3DS's physical shell casing is open (1) or closed (0)
         */
        void GetShellState(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetBatteryLevel service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Battery level, 5 = completely full battery, 4 = mostly full battery,
         *          3 = half full battery, 2 =  low battery, 1 = critical battery.
         */
        void GetBatteryLevel(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetBatteryChargeState service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output of function, 0 = not charging, 1 = charging.
         */
        void GetBatteryChargeState(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetPedometerState service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output of function, 0 = not counting steps, 1 = counting steps.
         */
        void GetPedometerState(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetStepHistory service function
         *  Inputs:
         *      1 : Number of hours
         *    2-3 : Start time
         *      4 : Buffer mapping descriptor
         *      5 : (short*) Buffer for step counts
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetStepHistory(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetTotalStepCount service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output of function, * = total step count
         */
        void GetTotalStepCount(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetSoftwareClosedFlag service function
         *  Outputs:
         *      1: Result code, 0 on success, otherwise error code
         *      2: Whether or not the "software closed" dialog was requested by the last FIRM
         *         and should be displayed.
         */
        void GetSoftwareClosedFlag(Kernel::HLERequestContext& ctx);

        /**
         * PTM::CheckNew3DS service function
         *  Outputs:
         *      1: Result code, 0 on success, otherwise error code
         *      2: u8 output: 0 = Old3DS, 1 = New3DS.
         */
        void CheckNew3DS(Kernel::HLERequestContext& ctx);

        /**
         * PTM::GetSystemTime service function
         *  Outputs:
         *      1: Result code, 0 on success, otherwise error code
         *      2-3: Time since 01/01/2020.
         */
        void GetSystemTime(Kernel::HLERequestContext& ctx);

	/**
	 * PTM::ConfigureNew3DSCPU service function
	 *
	 */
	void ConfigureNew3DSCPU(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> ptm;
    };

private:
    Core::System& system;

    bool shell_open = true;
    bool battery_is_charging = true;
    bool pedometer_is_counting = false;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::PTM

BOOST_CLASS_EXPORT_KEY(Service::PTM::Module)
SERVICE_CONSTRUCT(Service::PTM::Module)
