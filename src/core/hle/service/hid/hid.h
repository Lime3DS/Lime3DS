// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>
#include <boost/serialization/version.hpp>
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/input.h"
#include "core/hle/service/service.h"
#include "network/artic_base/artic_base_client.h"

namespace Kernel {
class Event;
class SharedMemory;
} // namespace Kernel

namespace Core {
struct TimingEventType;
};

namespace Service::HID {

/**
 * Structure of a Pad controller state.
 */
struct PadState {
    union {
        u32 hex{};

        BitField<0, 1, u32> a;
        BitField<1, 1, u32> b;
        BitField<2, 1, u32> select;
        BitField<3, 1, u32> start;
        BitField<4, 1, u32> right;
        BitField<5, 1, u32> left;
        BitField<6, 1, u32> up;
        BitField<7, 1, u32> down;
        BitField<8, 1, u32> r;
        BitField<9, 1, u32> l;
        BitField<10, 1, u32> x;
        BitField<11, 1, u32> y;
        BitField<12, 1, u32> debug;
        BitField<13, 1, u32> gpio14;

        BitField<28, 1, u32> circle_right;
        BitField<29, 1, u32> circle_left;
        BitField<30, 1, u32> circle_up;
        BitField<31, 1, u32> circle_down;
    };
};

/**
 * Structure of a single entry of Pad state history within HID shared memory
 */
struct PadDataEntry {
    PadState current_state;
    PadState delta_additions;
    PadState delta_removals;

    s16 circle_pad_x;
    s16 circle_pad_y;
};

/**
 * Structure of a single entry of touch state history within HID shared memory
 */
struct TouchDataEntry {
    u16 x;                     ///< Y-coordinate of a touchpad press on the lower screen
    u16 y;                     ///< X-coordinate of a touchpad press on the lower screen
    BitField<0, 7, u32> valid; ///< Set to 1 when this entry contains actual X/Y data, otherwise 0
};

/**
 * Structure of a single entry of accelerometer state history within HID shared memory
 */
struct AccelerometerDataEntry {
    s16 x;
    s16 y;
    s16 z;
};

/**
 * Structure of a single entry of gyroscope state history within HID shared memory
 */
struct GyroscopeDataEntry {
    s16 x;
    s16 y;
    s16 z;
};

/**
 * Structure of data stored in HID shared memory
 */
struct SharedMem {
    /// Pad data, this is used for buttons and the circle pad
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated pad state entry

        INSERT_PADDING_WORDS(0x1);

        f32 sliderstate_3d;

        PadState current_state; ///< Current state of the pad buttons

        // TODO(bunnei): Implement `raw_circle_pad_data` field
        u32 raw_circle_pad_data; ///< Raw (analog) circle pad data, before being converted

        INSERT_PADDING_WORDS(0x1);

        std::array<PadDataEntry, 8> entries; ///< Last 8 pad entries
    } pad;

    /// Touchpad data, this is used for touchpad input
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated touch entry

        INSERT_PADDING_WORDS(0x1);

        // TODO(bunnei): Implement `raw_entry` field
        TouchDataEntry raw_entry; ///< Raw (analog) touch data, before being converted

        std::array<TouchDataEntry, 8> entries; ///< Last 8 touch entries, in pixel coordinates
    } touch;

    /// Accelerometer data
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated accelerometer entry

        INSERT_PADDING_WORDS(0x1);

        AccelerometerDataEntry raw_entry;
        INSERT_PADDING_BYTES(2);

        std::array<AccelerometerDataEntry, 8> entries;
    } accelerometer;

    /// Gyroscope data
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated accelerometer entry

        INSERT_PADDING_WORDS(0x1);

        GyroscopeDataEntry raw_entry;
        INSERT_PADDING_BYTES(2);

        std::array<GyroscopeDataEntry, 32> entries;
    } gyroscope;
};

/**
 * Structure of calibrate params that GetGyroscopeLowCalibrateParam returns
 */
struct GyroscopeCalibrateParam {
    struct {
        // TODO (wwylele): figure out the exact meaning of these params
        s16 zero_point;
        s16 positive_unit_point;
        s16 negative_unit_point;
    } x, y, z;
};

#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(SharedMem, field_name) == position * 4,                                 \
                  "Field " #field_name " has invalid position")

ASSERT_REG_POSITION(pad.index_reset_ticks, 0x0);
ASSERT_REG_POSITION(touch.index_reset_ticks, 0x2A);

#undef ASSERT_REG_POSITION

struct DirectionState {
    bool up;
    bool down;
    bool left;
    bool right;
};

/// Translates analog stick axes to directions. This is exposed for ir_rst module to use.
DirectionState GetStickDirectionState(s16 circle_pad_x, s16 circle_pad_y);

class ArticBaseController {
public:
    struct ControllerData {
        u32 index{};
        u32 pad{};
        s16 c_pad_x{};
        s16 c_pad_y{};
        u16 touch_x{};
        u16 touch_y{};
        s16 c_stick_x{};
        s16 c_stick_y{};
        s16 accel_x{};
        s16 accel_y{};
        s16 accel_z{};
        s16 gyro_x{};
        s16 gyro_y{};
        s16 gyro_z{};
    };
    static_assert(sizeof(ControllerData) == 0x20, "Incorrect ControllerData size");

    ArticBaseController(const std::shared_ptr<Network::ArticBase::Client>& client);

    bool IsCreated() {
        return udp_stream.get();
    }

    bool IsReady() {
        return udp_stream.get() ? udp_stream->IsReady() : false;
    }

    ControllerData GetControllerData();

private:
    std::shared_ptr<Network::ArticBase::Client::UDPStream> udp_stream;
    u32 last_packet_id{};
    ControllerData last_controller_data{};
};

class Module final {
public:
    explicit Module(Core::System& system);

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> hid, const char* name, u32 max_session);

        std::shared_ptr<Module> GetModule() const;

    protected:
        /**
         * HID::GetIPCHandles service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : IPC Command Structure translate-header
         *      3 : Handle to HID shared memory
         *      4 : Event signaled by HID
         *      5 : Event signaled by HID
         *      6 : Event signaled by HID
         *      7 : Gyroscope event
         *      8 : Event signaled by HID
         */
        void GetIPCHandles(Kernel::HLERequestContext& ctx);

        /**
         * HID::EnableAccelerometer service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void EnableAccelerometer(Kernel::HLERequestContext& ctx);

        /**
         * HID::DisableAccelerometer service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DisableAccelerometer(Kernel::HLERequestContext& ctx);

        /**
         * HID::EnableGyroscopeLow service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void EnableGyroscopeLow(Kernel::HLERequestContext& ctx);

        /**
         * HID::DisableGyroscopeLow service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DisableGyroscopeLow(Kernel::HLERequestContext& ctx);

        /**
         * HID::GetSoundVolume service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 output value
         */
        void GetSoundVolume(Kernel::HLERequestContext& ctx);

        /**
         * HID::GetGyroscopeLowRawToDpsCoefficient service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : float output value
         */
        void GetGyroscopeLowRawToDpsCoefficient(Kernel::HLERequestContext& ctx);

        /**
         * HID::GetGyroscopeLowCalibrateParam service function
         *  Inputs:
         *      None
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2~6 (18 bytes) : struct GyroscopeCalibrateParam
         */
        void GetGyroscopeLowCalibrateParam(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> hid;
    };

    void UseArticClient(const std::shared_ptr<Network::ArticBase::Client>& client);

    void ReloadInputDevices();

    const PadState& GetState() const;

    // Updating period for each HID device. These empirical values are measured from a 11.2 3DS.
    static constexpr u64 pad_update_ticks = BASE_CLOCK_RATE_ARM11 / 234;
    static constexpr u64 accelerometer_update_ticks = BASE_CLOCK_RATE_ARM11 / 104;
    static constexpr u64 gyroscope_update_ticks = BASE_CLOCK_RATE_ARM11 / 101;

private:
    void LoadInputDevices();
    void UpdatePadCallback(std::uintptr_t user_data, s64 cycles_late);
    void UpdateAccelerometerCallback(std::uintptr_t user_data, s64 cycles_late);
    void UpdateGyroscopeCallback(std::uintptr_t user_data, s64 cycles_late);

    Core::System& system;

    // Handle to shared memory region designated to HID_User service
    std::shared_ptr<Kernel::SharedMemory> shared_mem;

    // Event handles
    std::shared_ptr<Kernel::Event> event_pad_or_touch_1;
    std::shared_ptr<Kernel::Event> event_pad_or_touch_2;
    std::shared_ptr<Kernel::Event> event_accelerometer;
    std::shared_ptr<Kernel::Event> event_gyroscope;
    std::shared_ptr<Kernel::Event> event_debug_pad;

    // The HID module of a 3DS does not store the PadState.
    // Storing this here was necessary for emulation specific tasks like cheats or scripting.
    PadState state;

    // xperia64: These are used to averate the previous N raw circle pad inputs with the current raw
    // input to simulate the sluggishness of a real 3DS circle pad
    // The Theatrhythm games rely on the circle pad being fairly slow to move, and from empircal
    // testing, need a minimum of 3 averaging to not drop inputs
    static constexpr s16 CIRCLE_PAD_AVERAGING = 3;
    std::vector<s16> circle_pad_old_x = std::vector<s16>(CIRCLE_PAD_AVERAGING - 1, 0);
    std::vector<s16> circle_pad_old_y = std::vector<s16>(CIRCLE_PAD_AVERAGING - 1, 0);

    u32 next_pad_index = 0;
    u32 next_touch_index = 0;
    u32 next_accelerometer_index = 0;
    u32 next_gyroscope_index = 0;

    int enable_accelerometer_count = 0; // positive means enabled
    int enable_gyroscope_count = 0;     // positive means enabled

    Core::TimingEventType* pad_update_event;
    Core::TimingEventType* accelerometer_update_event;
    Core::TimingEventType* gyroscope_update_event;

    std::atomic<bool> is_device_reload_pending{true};
    std::array<std::unique_ptr<Input::ButtonDevice>, Settings::NativeButton::NUM_BUTTONS_HID>
        buttons;
    std::unique_ptr<Input::AnalogDevice> circle_pad;
    std::unique_ptr<Input::MotionDevice> motion_device;
    std::unique_ptr<Input::TouchDevice> touch_device;
    std::unique_ptr<Input::TouchDevice> touch_btn_device;

    std::shared_ptr<ArticBaseController> artic_controller;
    std::shared_ptr<Network::ArticBase::Client> artic_client;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

std::shared_ptr<Module> GetModule(Core::System& system);

void InstallInterfaces(Core::System& system);
} // namespace Service::HID

SERVICE_CONSTRUCT(Service::HID::Module)
BOOST_CLASS_EXPORT_KEY(Service::HID::Module)
BOOST_CLASS_VERSION(Service::HID::Module, 1)
