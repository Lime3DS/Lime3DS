// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

/**
 * The shared page stores various runtime configuration settings. This memory page is
 * read-only for user processes (there is a bit in the header that grants the process
 * write access, according to 3dbrew; this is not emulated)
 */

#include <chrono>
#include <ctime>
#include <memory>
#include <boost/serialization/export.hpp>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/memory_ref.h"
#include "common/swap.h"
#include "core/memory.h"

namespace Core {
struct TimingEventType;
class Timing;
} // namespace Core

namespace SharedPage {

// See http://3dbrew.org/wiki/Configuration_Memory#Shared_Memory_Page_For_ARM11_Processes

struct DateTime {
    u64_le date_time;                  // 0
    u64_le update_tick;                // 8
    u64_le tick_to_second_coefficient; // 10
    u64_le tick_offset;                // 18
};
static_assert(sizeof(DateTime) == 0x20, "Datetime size is wrong");

union BatteryState {
    u8 raw;
    BitField<0, 1, u8> is_adapter_connected;
    BitField<1, 1, u8> is_charging;
    BitField<2, 3, u8> charge_level;
};

using MacAddress = std::array<u8, 6>;

// Default MAC address in the Nintendo 3DS range
constexpr MacAddress DefaultMac = {0x40, 0xF4, 0x07, 0x00, 0x00, 0x00};

enum class WifiLinkLevel : u8 {
    Off = 0,
    Poor = 1,
    Good = 2,
    Best = 3,
};

enum class WifiState : u8 {
    Invalid = 0,
    Enabled = 1,
    Internet = 2,
    Local1 = 3,
    Local2 = 4,
    Local3 = 6,
    Disabled = 7,
};

struct SharedPageDef {
    // Most of these names are taken from the 3dbrew page linked above.
    u32_le date_time_counter; // 0
    u8 running_hw;            // 4
    /// "Microcontroller hardware info"
    u8 mcu_hw_info;                      // 5
    INSERT_PADDING_BYTES(0x20 - 0x6);    // 6
    DateTime date_time_0;                // 20
    DateTime date_time_1;                // 40
    u8 wifi_macaddr[6];                  // 60
    u8 wifi_link_level;                  // 66
    u8 wifi_state;                       // 67
    INSERT_PADDING_BYTES(0x80 - 0x68);   // 68
    float_le sliderstate_3d;             // 80
    u8 ledstate_3d;                      // 84
    BatteryState battery_state;          // 85
    u8 unknown_value;                    // 86
    INSERT_PADDING_BYTES(0xA0 - 0x87);   // 87
    u64_le menu_title_id;                // A0
    u64_le active_menu_title_id;         // A8
    INSERT_PADDING_BYTES(0x1000 - 0xB0); // B0
};
static_assert(sizeof(SharedPageDef) == Memory::SHARED_PAGE_SIZE,
              "Shared page structure size is wrong");

class Handler : public BackingMem {
public:
    Handler(Core::Timing& timing, u64 override_init_time);

    void SetMacAddress(const MacAddress&);

    MacAddress GetMacAddress();

    void SetWifiLinkLevel(WifiLinkLevel);

    WifiLinkLevel GetWifiLinkLevel();

    void SetWifiState(WifiState);

    void Set3DLed(u8);

    void Set3DSlider(float);

    SharedPageDef& GetSharedPage();

    u8* GetPtr() override {
        return reinterpret_cast<u8*>(&shared_page);
    }

    const u8* GetPtr() const override {
        return reinterpret_cast<const u8*>(&shared_page);
    }

    std::size_t GetSize() const override {
        return sizeof(shared_page);
    }

    /// Gets the system time in milliseconds since the year 2000.
    u64 GetSystemTimeSince2000() const;

    /// Gets the system time in milliseconds since the year 1900.
    u64 GetSystemTimeSince1900() const;

private:
    void UpdateTimeCallback(std::uintptr_t user_data, int cycles_late);
    Core::Timing& timing;
    Core::TimingEventType* update_time_event;
    std::chrono::seconds init_time;

    SharedPageDef shared_page;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace SharedPage

namespace boost::serialization {

template <class Archive>
void load_construct_data(Archive& ar, SharedPage::Handler* t, const unsigned int);

} // namespace boost::serialization

BOOST_CLASS_EXPORT_KEY(SharedPage::Handler)
