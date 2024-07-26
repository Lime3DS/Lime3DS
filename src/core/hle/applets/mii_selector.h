// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/mii.h"
#include "core/hle/result.h"
#include "core/hle/service/apt/apt.h"

namespace Frontend {
class MiiSelector;
struct MiiSelectorConfig;
} // namespace Frontend

namespace HLE::Applets {

struct MiiConfig {
    u8 enable_cancel_button;
    u8 enable_guest_mii;
    u8 show_on_top_screen;
    INSERT_PADDING_BYTES(5);
    std::array<u16_le, 0x40> title;
    INSERT_PADDING_BYTES(4);
    u8 show_guest_miis;
    INSERT_PADDING_BYTES(3);
    u32 initially_selected_mii_index;
    std::array<u8, 0x6> guest_mii_whitelist;
    std::array<u8, 0x64> user_mii_whitelist;
    INSERT_PADDING_BYTES(2);
    u32 magic_value;
};
static_assert(sizeof(MiiConfig) == 0x104, "MiiConfig structure has incorrect size");
#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(MiiConfig, field_name) == position,                                     \
                  "Field " #field_name " has invalid position")
ASSERT_REG_POSITION(title, 0x08);
ASSERT_REG_POSITION(show_guest_miis, 0x8C);
ASSERT_REG_POSITION(initially_selected_mii_index, 0x90);
ASSERT_REG_POSITION(guest_mii_whitelist, 0x94);
#undef ASSERT_REG_POSITION

struct MiiResult {
    u32_be return_code;
    u32_be is_guest_mii_selected;
    u32_be selected_guest_mii_index;
    Mii::ChecksummedMiiData selected_mii_data;
    std::array<u16_le, 0xC> guest_mii_name;
};
static_assert(sizeof(MiiResult) == 0x84, "MiiResult structure has incorrect size");
#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(MiiResult, field_name) == position,                                     \
                  "Field " #field_name " has invalid position")
ASSERT_REG_POSITION(selected_mii_data, 0x0C);
ASSERT_REG_POSITION(guest_mii_name, 0x6C);
#undef ASSERT_REG_POSITION

class MiiSelector final : public Applet {
public:
    MiiSelector(Core::System& system, Service::APT::AppletId id, Service::APT::AppletId parent,
                bool preload, std::weak_ptr<Service::APT::AppletManager> manager)
        : Applet(system, id, parent, preload, std::move(manager)) {}

    Result ReceiveParameterImpl(const Service::APT::MessageParameter& parameter) override;
    Result Start(const Service::APT::MessageParameter& parameter) override;
    Result Finalize() override;
    void Update() override;

    static MiiResult GetStandardMiiResult();

private:
    Frontend::MiiSelectorConfig ToFrontendConfig(const MiiConfig& config) const;

    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with
    /// GSPGPU::ImportDisplayCaptureInfo
    std::shared_ptr<Kernel::SharedMemory> framebuffer_memory;

    MiiConfig config;

    MiiResult result{};

    std::shared_ptr<Frontend::MiiSelector> frontend_applet;
};
} // namespace HLE::Applets
