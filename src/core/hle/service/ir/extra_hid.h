// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <span>
#include <boost/serialization/array.hpp>
#include "common/bit_field.h"
#include "common/swap.h"
#include "core/frontend/input.h"
#include "core/hle/service/ir/ir_user.h"

namespace Core {
struct TimingEventType;
class Timing;
class Movie;
} // namespace Core

namespace Service::HID {
class ArticBaseController;
};

namespace Service::IR {

struct ExtraHIDResponse {
    union {
        BitField<0, 8, u32> header;
        BitField<8, 12, u32> c_stick_x;
        BitField<20, 12, u32> c_stick_y;
    } c_stick;
    union {
        BitField<0, 5, u8> battery_level;
        BitField<5, 1, u8> zl_not_held;
        BitField<6, 1, u8> zr_not_held;
        BitField<7, 1, u8> r_not_held;
    } buttons;
    u8 unknown;
};
static_assert(sizeof(ExtraHIDResponse) == 6, "HID status response has wrong size!");

/**
 * An IRDevice emulating Circle Pad Pro or New 3DS additional HID hardware.
 * This device sends periodic udates at a rate configured by the 3DS, and sends calibration data if
 * requested.
 */
class ExtraHID final : public IRDevice {
public:
    explicit ExtraHID(SendFunc send_func, Core::Timing& timing, Core::Movie& movie);
    ~ExtraHID();

    void OnConnect() override;
    void OnDisconnect() override;
    void OnReceive(std::span<const u8> data) override;

    /// Requests input devices reload from current settings. Called when the input settings change.
    void RequestInputDevicesReload();

    void UseArticController(const std::shared_ptr<Service::HID::ArticBaseController>& ac) {
        artic_controller = ac;
    }

private:
    void SendHIDStatus();
    void HandleConfigureHIDPollingRequest(std::span<const u8> request);
    void HandleReadCalibrationDataRequest(std::span<const u8> request);
    void LoadInputDevices();

    Core::Timing& timing;
    Core::Movie& movie;
    u8 hid_period;
    Core::TimingEventType* hid_polling_callback_id;
    std::array<u8, 0x40> calibration_data;
    std::unique_ptr<Input::ButtonDevice> zl;
    std::unique_ptr<Input::ButtonDevice> zr;
    std::unique_ptr<Input::AnalogDevice> c_stick;
    std::atomic<bool> is_device_reload_pending;

    std::shared_ptr<Service::HID::ArticBaseController> artic_controller = nullptr;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & hid_period;
        ar & calibration_data; // This isn't writeable for now, but might be in future
        if (Archive::is_loading::value) {
            LoadInputDevices(); // zl, zr, c_stick are loaded here
        }
    }
    friend class boost::serialization::access;
};

} // namespace Service::IR

BOOST_CLASS_EXPORT_KEY(Service::IR::ExtraHID)
