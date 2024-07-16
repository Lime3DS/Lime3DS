// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "common/archives.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/movie.h"

SERIALIZE_EXPORT_IMPL(Service::IR::IR_RST)
SERVICE_CONSTRUCT_IMPL(Service::IR::IR_RST)

namespace Service::IR {

template <class Archive>
void IR_RST::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    ar & update_event;
    ar & shared_memory;
    ar & next_pad_index;
    ar & raw_c_stick;
    ar & update_period;
    // update_callback_id and input devices are set separately
    ReloadInputDevices();
}

struct PadDataEntry {
    PadState current_state;
    PadState delta_additions;
    PadState delta_removals;

    s16_le c_stick_x;
    s16_le c_stick_y;
};

struct SharedMem {
    u64_le index_reset_ticks;          ///< CPU tick count for when HID module updated entry index 0
    u64_le index_reset_ticks_previous; ///< Previous `index_reset_ticks`
    u32_le index;
    INSERT_PADDING_WORDS(1);
    std::array<PadDataEntry, 8> entries; ///< Last 8 pad entries
};

static_assert(sizeof(SharedMem) == 0x98, "SharedMem has wrong size!");

void IR_RST::LoadInputDevices() {
    zl_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.current_input_profile.buttons[Settings::NativeButton::ZL]);
    zr_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.current_input_profile.buttons[Settings::NativeButton::ZR]);
    c_stick = Input::CreateDevice<Input::AnalogDevice>(
        Settings::values.current_input_profile.analogs[Settings::NativeAnalog::CStick]);
}

void IR_RST::UnloadInputDevices() {
    zl_button = nullptr;
    zr_button = nullptr;
    c_stick = nullptr;
}

void IR_RST::UpdateCallback(std::uintptr_t user_data, s64 cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_memory->GetPointer());

    if (is_device_reload_pending.exchange(false))
        LoadInputDevices();

    constexpr u32 VALID_EXTRAHID_KEYS = 0xF00C000;

    PadState state;
    s16 c_stick_x, c_stick_y;

    if (artic_controller.get() && artic_controller->IsReady()) {
        Service::HID::ArticBaseController::ControllerData data =
            artic_controller->GetControllerData();

        state.hex = data.pad & VALID_EXTRAHID_KEYS;

        c_stick_x = data.c_stick_x;
        c_stick_y = data.c_stick_y;

        system.Movie().HandleIrRst(state, c_stick_x, c_stick_y);
    } else {
        state.zl.Assign(zl_button->GetStatus());
        state.zr.Assign(zr_button->GetStatus());

        // Get current c-stick position and update c-stick direction
        float c_stick_x_f, c_stick_y_f;
        std::tie(c_stick_x_f, c_stick_y_f) = c_stick->GetStatus();
        constexpr int MAX_CSTICK_RADIUS = 0x9C; // Max value for a c-stick radius
        c_stick_x = static_cast<s16>(c_stick_x_f * MAX_CSTICK_RADIUS);
        c_stick_y = static_cast<s16>(c_stick_y_f * MAX_CSTICK_RADIUS);

        system.Movie().HandleIrRst(state, c_stick_x, c_stick_y);

        if (!raw_c_stick) {
            const HID::DirectionState direction = HID::GetStickDirectionState(c_stick_x, c_stick_y);
            state.c_stick_up.Assign(direction.up);
            state.c_stick_down.Assign(direction.down);
            state.c_stick_left.Assign(direction.left);
            state.c_stick_right.Assign(direction.right);
        }
    }

    // TODO (wwylele): implement raw C-stick data for raw_c_stick = true

    const u32 last_entry_index = mem->index;
    mem->index = next_pad_index;
    next_pad_index = (next_pad_index + 1) % mem->entries.size();

    // Get the previous Pad state
    PadState old_state{mem->entries[last_entry_index].current_state};

    // Compute bitmask with 1s for bits different from the old state
    PadState changed = {state.hex ^ old_state.hex};

    // Get the current Pad entry
    PadDataEntry& pad_entry = mem->entries[mem->index];

    // Update entry properties
    pad_entry.current_state.hex = state.hex;
    pad_entry.delta_additions.hex = changed.hex & state.hex;
    pad_entry.delta_removals.hex = changed.hex & old_state.hex;
    pad_entry.c_stick_x = c_stick_x;
    pad_entry.c_stick_y = c_stick_y;

    // If we just updated index 0, provide a new timestamp
    if (mem->index == 0) {
        mem->index_reset_ticks_previous = mem->index_reset_ticks;
        mem->index_reset_ticks = system.CoreTiming().GetTicks();
    }

    update_event->Signal();

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(msToCycles(update_period) - cycles_late, update_callback_id);
}

void IR_RST::GetHandles(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 3);
    rb.Push(ResultSuccess);
    rb.PushMoveObjects(shared_memory, update_event);
}

void IR_RST::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    update_period = static_cast<int>(rp.Pop<u32>());
    raw_c_stick = rp.Pop<bool>();

    if (raw_c_stick)
        LOG_ERROR(Service_IR, "raw C-stick data is not implemented!");

    next_pad_index = 0;
    is_device_reload_pending.store(true);
    system.CoreTiming().ScheduleEvent(msToCycles(update_period), update_callback_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_DEBUG(Service_IR, "called. update_period={}, raw_c_stick={}", update_period, raw_c_stick);
}

void IR_RST::Shutdown(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    system.CoreTiming().UnscheduleEvent(update_callback_id, 0);
    UnloadInputDevices();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_DEBUG(Service_IR, "called");
}

IR_RST::IR_RST(Core::System& system) : ServiceFramework("ir:rst", 2), system(system) {
    using namespace Kernel;
    // Note: these two kernel objects are even available before Initialize service function is
    // called.
    shared_memory =
        system.Kernel()
            .CreateSharedMemory(nullptr, 0x1000, MemoryPermission::ReadWrite,
                                MemoryPermission::Read, 0, MemoryRegion::BASE, "IRRST:SharedMemory")
            .Unwrap();
    update_event = system.Kernel().CreateEvent(ResetType::OneShot, "IRRST:UpdateEvent");

    update_callback_id = system.CoreTiming().RegisterEvent(
        "IRRST:UpdateCallBack", [this](std::uintptr_t user_data, s64 cycles_late) {
            UpdateCallback(user_data, cycles_late);
        });

    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &IR_RST::GetHandles, "GetHandles"},
        {0x0002, &IR_RST::Initialize, "Initialize"},
        {0x0003, &IR_RST::Shutdown, "Shutdown"},
        {0x0009, nullptr, "WriteToTwoFields"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

IR_RST::~IR_RST() = default;

void IR_RST::ReloadInputDevices() {
    is_device_reload_pending.store(true);
}

} // namespace Service::IR
