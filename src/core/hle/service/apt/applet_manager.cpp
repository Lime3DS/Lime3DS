// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/file_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/frontend/input.h"
#include "core/hle/applets/applet.h"
#include "core/hle/applets/erreula.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/applets/mint.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/errors.h"
#include "core/hle/service/apt/ns.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "video_core/utils.h"

SERVICE_CONSTRUCT_IMPL(Service::APT::AppletManager)

namespace Service::APT {

/// The interval at which the home button update callback will be called, 16.6ms
static constexpr u64 button_update_interval_us = 16666;
/// The interval at which the HLE Applet update callback will be called, 16.6ms.
static constexpr u64 hle_applet_update_interval_us = 16666;

struct AppletTitleData {
    // There are two possible applet ids for each applet.
    std::array<AppletId, 2> applet_ids;

    // There's a specific TitleId per region for each applet.
    static constexpr std::size_t NumRegions = 7;
    std::array<u64, NumRegions> title_ids;
    std::array<u64, NumRegions> n3ds_title_ids = {0, 0, 0, 0, 0, 0, 0};
};

static constexpr std::size_t NumApplets = 29;
static constexpr std::array<AppletTitleData, NumApplets> applet_titleids = {{
    {{AppletId::HomeMenu, AppletId::None},
     {0x4003000008202, 0x4003000008F02, 0x4003000009802, 0x4003000008202, 0x400300000A102,
      0x400300000A902, 0x400300000B102}},
    {{AppletId::AlternateMenu, AppletId::None},
     {0x4003000008102, 0x4003000008102, 0x4003000008102, 0x4003000008102, 0x4003000008102,
      0x4003000008102, 0x4003000008102}},
    {{AppletId::Camera, AppletId::None},
     {0x4003000008402, 0x4003000009002, 0x4003000009902, 0x4003000008402, 0x400300000A202,
      0x400300000AA02, 0x400300000B202}},
    {{AppletId::FriendList, AppletId::None},
     {0x4003000008D02, 0x4003000009602, 0x4003000009F02, 0x4003000008D02, 0x400300000A702,
      0x400300000AF02, 0x400300000B702}},
    {{AppletId::GameNotes, AppletId::None},
     {0x4003000008702, 0x4003000009302, 0x4003000009C02, 0x4003000008702, 0x400300000A502,
      0x400300000AD02, 0x400300000B502}},
    {{AppletId::InternetBrowser, AppletId::None},
     {0x4003000008802, 0x4003000009402, 0x4003000009D02, 0x4003000008802, 0x400300000A602,
      0x400300000AE02, 0x400300000B602},
     {0x4003020008802, 0x4003020009402, 0x4003020009D02, 0x4003020008802, 0, 0x400302000AE02, 0}},
    {{AppletId::InstructionManual, AppletId::None},
     {0x4003000008602, 0x4003000009202, 0x4003000009B02, 0x4003000008602, 0x400300000A402,
      0x400300000AC02, 0x400300000B402}},
    {{AppletId::Notifications, AppletId::None},
     {0x4003000008E02, 0x4003000009702, 0x400300000A002, 0x4003000008E02, 0x400300000A802,
      0x400300000B002, 0x400300000B802}},
    {{AppletId::Miiverse, AppletId::None},
     {0x400300000BC02, 0x400300000BD02, 0x400300000BE02, 0x400300000BC02, 0x4003000009E02,
      0x4003000009502, 0x400300000B902}},
    // These values obtained from an older NS dump firmware 4.5
    {{AppletId::MiiversePost, AppletId::None},
     {0x400300000BA02, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02, 0x400300000BA02,
      0x400300000BA02, 0x400300000BA02}},
    // {AppletId::MiiversePost, AppletId::None, 0x4003000008302, 0x4003000008B02, 0x400300000BA02,
    //  0x4003000008302, 0x0, 0x0, 0x0},
    {{AppletId::AmiiboSettings, AppletId::None},
     {0x4003000009502, 0x4003000009E02, 0x400300000B902, 0x4003000009502, 0x0, 0x4003000008C02,
      0x400300000BF02}},
    {{AppletId::SoftwareKeyboard1, AppletId::SoftwareKeyboard2},
     {0x400300000C002, 0x400300000C802, 0x400300000D002, 0x400300000C002, 0x400300000D802,
      0x400300000DE02, 0x400300000E402}},
    {{AppletId::Ed1, AppletId::Ed2},
     {0x400300000C102, 0x400300000C902, 0x400300000D102, 0x400300000C102, 0x400300000D902,
      0x400300000DF02, 0x400300000E502}},
    {{AppletId::PnoteApp, AppletId::PnoteApp2},
     {0x400300000C302, 0x400300000CB02, 0x400300000D302, 0x400300000C302, 0x400300000DB02,
      0x400300000E102, 0x400300000E702}},
    {{AppletId::SnoteApp, AppletId::SnoteApp2},
     {0x400300000C402, 0x400300000CC02, 0x400300000D402, 0x400300000C402, 0x400300000DC02,
      0x400300000E202, 0x400300000E802}},
    {{AppletId::Error, AppletId::Error2},
     {0x400300000C502, 0x400300000C502, 0x400300000C502, 0x400300000C502, 0x400300000CF02,
      0x400300000CF02, 0x400300000CF02}},
    {{AppletId::Mint, AppletId::Mint2},
     {0x400300000C602, 0x400300000CE02, 0x400300000D602, 0x400300000C602, 0x400300000DD02,
      0x400300000E302, 0x400300000E902}},
    {{AppletId::Extrapad, AppletId::Extrapad2},
     {0x400300000CD02, 0x400300000CD02, 0x400300000CD02, 0x400300000CD02, 0x400300000D502,
      0x400300000D502, 0x400300000D502}},
    {{AppletId::Memolib, AppletId::Memolib2},
     {0x400300000F602, 0x400300000F602, 0x400300000F602, 0x400300000F602, 0x400300000F602,
      0x400300000F602, 0x400300000F602}},
    // TODO(Subv): Fill in the rest of the titleids
}};

static u64 GetTitleIdForApplet(AppletId id, u32 region_value) {
    ASSERT_MSG(id != AppletId::None, "Invalid applet id");

    auto itr = std::find_if(applet_titleids.begin(), applet_titleids.end(),
                            [id](const AppletTitleData& data) {
                                return data.applet_ids[0] == id || data.applet_ids[1] == id;
                            });

    ASSERT_MSG(itr != applet_titleids.end(), "Unknown applet id 0x{:#05X}", id);

    auto n3ds_title_id = itr->n3ds_title_ids[region_value];
    if (n3ds_title_id != 0 && Settings::values.is_new_3ds.GetValue()) {
        return n3ds_title_id;
    }
    return itr->title_ids[region_value];
}

static bool IsSystemAppletId(AppletId applet_id) {
    return (static_cast<u32>(applet_id) & static_cast<u32>(AppletId::AnySystemApplet)) != 0;
}

static bool IsApplicationAppletId(AppletId applet_id) {
    return (static_cast<u32>(applet_id) & static_cast<u32>(AppletId::Application)) != 0;
}

AppletManager::AppletSlot AppletManager::GetAppletSlotFromId(AppletId id) {
    if (id == AppletId::Application) {
        if (GetAppletSlot(AppletSlot::Application)->applet_id != AppletId::None)
            return AppletSlot::Application;

        return AppletSlot::Error;
    }

    if (id == AppletId::AnySystemApplet) {
        if (GetAppletSlot(AppletSlot::SystemApplet)->applet_id != AppletId::None)
            return AppletSlot::SystemApplet;

        // The Home Menu is also a system applet, but it lives in its own slot to be able to run
        // concurrently with other system applets.
        if (GetAppletSlot(AppletSlot::HomeMenu)->applet_id != AppletId::None)
            return AppletSlot::HomeMenu;

        return AppletSlot::Error;
    }

    if (id == AppletId::AnyLibraryApplet || id == AppletId::AnySysLibraryApplet) {
        auto slot_data = GetAppletSlot(AppletSlot::LibraryApplet);
        if (slot_data->applet_id == AppletId::None)
            return AppletSlot::Error;

        auto applet_pos = slot_data->attributes.applet_pos.Value();
        if ((id == AppletId::AnyLibraryApplet && applet_pos == AppletPos::Library) ||
            (id == AppletId::AnySysLibraryApplet && applet_pos == AppletPos::SysLibrary))
            return AppletSlot::LibraryApplet;

        return AppletSlot::Error;
    }

    if (id == AppletId::HomeMenu || id == AppletId::AlternateMenu) {
        if (GetAppletSlot(AppletSlot::HomeMenu)->applet_id != AppletId::None)
            return AppletSlot::HomeMenu;

        return AppletSlot::Error;
    }

    for (std::size_t slot = 0; slot < applet_slots.size(); ++slot) {
        if (applet_slots[slot].applet_id == id) {
            return static_cast<AppletSlot>(slot);
        }
    }

    return AppletSlot::Error;
}

AppletManager::AppletSlot AppletManager::GetAppletSlotFromAttributes(AppletAttributes attributes) {
    // Mapping from AppletPos to AppletSlot
    static constexpr std::array<AppletSlot, 6> applet_position_slots = {
        AppletSlot::Application,   AppletSlot::LibraryApplet, AppletSlot::SystemApplet,
        AppletSlot::LibraryApplet, AppletSlot::Error,         AppletSlot::LibraryApplet};

    auto applet_pos_value = static_cast<u32>(attributes.applet_pos.Value());
    if (applet_pos_value >= applet_position_slots.size())
        return AppletSlot::Error;

    auto slot = applet_position_slots[applet_pos_value];
    if (slot == AppletSlot::Error)
        return AppletSlot::Error;

    // The Home Menu is a system applet, however, it has its own applet slot so that it can run
    // concurrently with other system applets.
    if (slot == AppletSlot::SystemApplet && attributes.is_home_menu)
        return AppletSlot::HomeMenu;

    return slot;
}

AppletManager::AppletSlot AppletManager::GetAppletSlotFromPos(AppletPos pos) {
    AppletId applet_id;
    switch (pos) {
    case AppletPos::Application:
        applet_id = AppletId::Application;
        break;
    case AppletPos::Library:
        applet_id = AppletId::AnyLibraryApplet;
        break;
    case AppletPos::System:
        applet_id = AppletId::AnySystemApplet;
        break;
    case AppletPos::SysLibrary:
        applet_id = AppletId::AnySysLibraryApplet;
        break;
    default:
        return AppletSlot::Error;
    }
    return GetAppletSlotFromId(applet_id);
}

void AppletManager::CancelAndSendParameter(const MessageParameter& parameter) {
    LOG_DEBUG(
        Service_APT, "Sending parameter from {:03X} to {:03X} with signal {:08X} and size {:08X}",
        parameter.sender_id, parameter.destination_id, parameter.signal, parameter.buffer.size());

    // If the applet is being HLEd, send directly to the applet.
    const auto applet = hle_applets[parameter.destination_id];
    if (applet != nullptr) {
        applet->ReceiveParameter(parameter);
    } else {
        // Otherwise, send the parameter the LLE way.
        next_parameter = parameter;

        if (parameter.signal == SignalType::RequestForSysApplet) {
            // APT handles RequestForSysApplet messages itself.
            LOG_DEBUG(Service_APT, "Replying to RequestForSysApplet from {:03X}",
                      parameter.sender_id);

            if (parameter.buffer.size() >= sizeof(CaptureBufferInfo)) {
                SetCaptureInfo(parameter.buffer);
                CaptureFrameBuffers();
            }

            next_parameter->sender_id = parameter.destination_id;
            next_parameter->destination_id = parameter.sender_id;
            next_parameter->signal = SignalType::Response;
            next_parameter->buffer.clear();
            next_parameter->object = nullptr;
        } else if (IsSystemAppletId(parameter.sender_id) &&
                   IsApplicationAppletId(parameter.destination_id) && parameter.object) {
            // When a message is sent from a system applet to an application, APT
            // replaces its object with the zero handle.
            next_parameter->object = nullptr;
        }

        // Signal the event to let the receiver know that a new parameter is ready to be read
        auto slot = GetAppletSlotFromId(next_parameter->destination_id);
        if (slot != AppletSlot::Error) {
            GetAppletSlot(slot)->parameter_event->Signal();
        } else {
            LOG_DEBUG(Service_APT, "No applet was registered with ID {:03X}",
                      next_parameter->destination_id);
        }
    }
}

Result AppletManager::SendParameter(const MessageParameter& parameter) {
    // A new parameter can not be sent if the previous one hasn't been consumed yet
    if (next_parameter) {
        LOG_WARNING(Service_APT, "Parameter from {:03X} to {:03X} blocked by pending parameter.",
                    parameter.sender_id, parameter.destination_id);
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    CancelAndSendParameter(parameter);
    return ResultSuccess;
}

ResultVal<MessageParameter> AppletManager::GlanceParameter(AppletId app_id) {
    if (!next_parameter) {
        return Result(ErrorDescription::NoData, ErrorModule::Applet, ErrorSummary::InvalidState,
                      ErrorLevel::Status);
    }

    if (next_parameter->destination_id != app_id) {
        return Result(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                      ErrorLevel::Status);
    }

    auto parameter = *next_parameter;

    // Note: The NS module always clears the DSPSleep and DSPWakeup signals even in GlanceParameter.
    if (next_parameter->signal == SignalType::DspSleep ||
        next_parameter->signal == SignalType::DspWakeup) {
        next_parameter = {};
    }

    return parameter;
}

ResultVal<MessageParameter> AppletManager::ReceiveParameter(AppletId app_id) {
    auto result = GlanceParameter(app_id);
    if (result.Succeeded()) {
        LOG_DEBUG(Service_APT,
                  "Received parameter from {:03X} to {:03X} with signal {:08X} and size {:08X}",
                  result->sender_id, result->destination_id, result->signal, result->buffer.size());

        // Clear the parameter
        next_parameter = {};
    }
    return result;
}

bool AppletManager::CancelParameter(bool check_sender, AppletId sender_appid, bool check_receiver,
                                    AppletId receiver_appid) {
    auto cancellation_success =
        next_parameter && (!check_sender || next_parameter->sender_id == sender_appid) &&
        (!check_receiver || next_parameter->destination_id == receiver_appid);

    if (cancellation_success)
        next_parameter = {};

    return cancellation_success;
}

ResultVal<AppletManager::GetLockHandleResult> AppletManager::GetLockHandle(
    AppletAttributes attributes) {
    auto corrected_attributes = attributes;
    if (attributes.applet_pos == AppletPos::Library ||
        attributes.applet_pos == AppletPos::SysLibrary ||
        attributes.applet_pos == AppletPos::AutoLibrary) {
        auto corrected_pos = last_library_launcher_slot == AppletSlot::Application
                                 ? AppletPos::Library
                                 : AppletPos::SysLibrary;
        corrected_attributes.applet_pos.Assign(corrected_pos);
        LOG_DEBUG(Service_APT, "Corrected applet attributes from {:08X} to {:08X}", attributes.raw,
                  corrected_attributes.raw);
    }

    return GetLockHandleResult{corrected_attributes, 0, lock};
}

ResultVal<AppletManager::InitializeResult> AppletManager::Initialize(AppletId app_id,
                                                                     AppletAttributes attributes) {
    auto slot = GetAppletSlotFromAttributes(attributes);
    // Note: The real NS service does not check if the attributes value is valid before accessing
    // the data in the array
    ASSERT_MSG(slot != AppletSlot::Error, "Invalid application attributes");

    auto slot_data = GetAppletSlot(slot);
    if (slot_data->registered) {
        LOG_WARNING(Service_APT, "Applet attempted to register in occupied slot {:02X}", slot);
        return Result(ErrorDescription::AlreadyExists, ErrorModule::Applet,
                      ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    LOG_DEBUG(Service_APT, "Initializing applet with ID {:03X} and attributes {:08X}.", app_id,
              attributes.raw);
    slot_data->applet_id = static_cast<AppletId>(app_id);
    // Note: In the real console the title id of a given applet slot is set by the APT module when
    // calling StartApplication.
    slot_data->title_id = system.Kernel().GetCurrentProcess()->codeset->program_id;
    slot_data->attributes.raw = attributes.raw;

    // Applications need to receive a Wakeup signal to actually start up, this signal is usually
    // sent by the Home Menu after starting the app by way of APT::WakeupApplication. However,
    // if nothing is running yet the signal should be sent by APT::Initialize itself.
    if (active_slot == AppletSlot::Error) {
        active_slot = slot;

        // APT automatically calls enable on the first registered applet.
        Enable(attributes);

        // Wake up the applet.
        SendParameter({
            .sender_id = AppletId::None,
            .destination_id = app_id,
            .signal = SignalType::Wakeup,
        });
    }

    return InitializeResult{slot_data->notification_event, slot_data->parameter_event};
}

Result AppletManager::Enable(AppletAttributes attributes) {
    auto slot = GetAppletSlotFromAttributes(attributes);
    if (slot == AppletSlot::Error) {
        LOG_WARNING(Service_APT,
                    "Attempted to register with attributes {:08X}, but could not find slot.",
                    attributes.raw);
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    LOG_DEBUG(Service_APT, "Registering applet with attributes {:08X}.", attributes.raw);
    auto slot_data = GetAppletSlot(slot);
    slot_data->registered = true;

    if (slot_data->applet_id != AppletId::None &&
        slot_data->attributes.applet_pos == AppletPos::System &&
        slot_data->attributes.is_home_menu) {
        slot_data->attributes.raw |= attributes.raw;
        LOG_DEBUG(Service_APT, "Updated home menu attributes to {:08X}.",
                  slot_data->attributes.raw);
    }

    // Send any outstanding parameters to the newly-registered application
    if (delayed_parameter && delayed_parameter->destination_id == slot_data->applet_id) {
        // TODO: Real APT would loop trying to send the parameter until it succeeds,
        // essentially waiting for existing parameters to be delivered.
        CancelAndSendParameter(*delayed_parameter);
        delayed_parameter.reset();
    }

    return ResultSuccess;
}

Result AppletManager::Finalize(AppletId app_id) {
    auto slot = GetAppletSlotFromId(app_id);
    if (slot == AppletSlot::Error) {
        return {ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                ErrorLevel::Status};
    }

    auto slot_data = GetAppletSlot(slot);
    slot_data->Reset();

    auto inactive = active_slot == AppletSlot::Error;
    if (!inactive) {
        auto active_slot_data = GetAppletSlot(active_slot);
        inactive = active_slot_data->applet_id == AppletId::None ||
                   active_slot_data->attributes.applet_pos.Value() == AppletPos::Invalid;
    }

    if (inactive) {
        active_slot = GetAppletSlotFromPos(AppletPos::System);
    }

    return ResultSuccess;
}

u32 AppletManager::CountRegisteredApplet() {
    return static_cast<u32>(std::count_if(applet_slots.begin(), applet_slots.end(),
                                          [](auto& slot_data) { return slot_data.registered; }));
}

bool AppletManager::IsRegistered(AppletId app_id) {
    auto slot = GetAppletSlotFromId(app_id);
    return slot != AppletSlot::Error && GetAppletSlot(slot)->registered;
}

ResultVal<AppletAttributes> AppletManager::GetAttribute(AppletId app_id) {
    auto slot = GetAppletSlotFromId(app_id);
    if (slot == AppletSlot::Error) {
        return Result(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                      ErrorLevel::Status);
    }

    auto slot_data = GetAppletSlot(slot);
    if (!slot_data->registered) {
        return Result(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                      ErrorLevel::Status);
    }

    return slot_data->attributes;
}

ResultVal<Notification> AppletManager::InquireNotification(AppletId app_id) {
    auto slot = GetAppletSlotFromId(app_id);
    if (slot != AppletSlot::Error) {
        auto slot_data = GetAppletSlot(slot);
        if (slot_data->registered) {
            auto notification = slot_data->notification;
            slot_data->notification = Notification::None;
            return notification;
        }
    }

    return Result(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                  ErrorLevel::Status);
}

Result AppletManager::SendNotification(Notification notification) {
    if (active_slot != AppletSlot::Error) {
        const auto slot_data = GetAppletSlot(active_slot);
        if (slot_data->registered) {
            slot_data->notification = notification;
            slot_data->notification_event->Signal();
            return ResultSuccess;
        }
    }

    return {ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
            ErrorLevel::Status};
}

void AppletManager::SendNotificationToAll(Notification notification) {
    for (auto& slot_data : applet_slots) {
        if (slot_data.registered) {
            slot_data.notification = notification;
            slot_data.notification_event->Signal();
        }
    }
}

Result AppletManager::CreateHLEApplet(AppletId id, AppletId parent, bool preload) {
    switch (id) {
    case AppletId::SoftwareKeyboard1:
    case AppletId::SoftwareKeyboard2:
        hle_applets[id] = std::make_shared<HLE::Applets::SoftwareKeyboard>(
            system, id, parent, preload, shared_from_this());
        break;
    case AppletId::Ed1:
    case AppletId::Ed2:
        hle_applets[id] = std::make_shared<HLE::Applets::MiiSelector>(system, id, parent, preload,
                                                                      shared_from_this());
        break;
    case AppletId::Error:
    case AppletId::Error2:
        hle_applets[id] = std::make_shared<HLE::Applets::ErrEula>(system, id, parent, preload,
                                                                  shared_from_this());
        break;
    case AppletId::Mint:
    case AppletId::Mint2:
        hle_applets[id] =
            std::make_shared<HLE::Applets::Mint>(system, id, parent, preload, shared_from_this());
        break;
    default:
        LOG_ERROR(Service_APT, "Could not create applet {}", id);
        // TODO(Subv): Find the right error code
        return Result(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotSupported,
                      ErrorLevel::Permanent);
    }

    AppletAttributes attributes;
    attributes.applet_pos.Assign(AppletPos::AutoLibrary);
    attributes.is_home_menu.Assign(false);
    const auto lock_handle_data = GetLockHandle(attributes);

    Initialize(id, lock_handle_data->corrected_attributes);
    Enable(lock_handle_data->corrected_attributes);
    if (preload) {
        FinishPreloadingLibraryApplet(id);
    }

    // Schedule the update event
    system.CoreTiming().ScheduleEvent(usToCycles(hle_applet_update_interval_us),
                                      hle_applet_update_event, static_cast<u64>(id));
    return ResultSuccess;
}

Result AppletManager::PrepareToStartLibraryApplet(AppletId applet_id) {
    // The real APT service returns an error if there's a pending APT parameter when this function
    // is called.
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    if (GetAppletSlot(AppletSlot::LibraryApplet)->registered) {
        return {ErrorDescription::AlreadyExists, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    last_library_launcher_slot = active_slot;
    last_prepared_library_applet = applet_id;

    capture_buffer_info.reset();

    if (Settings::values.lle_applets) {
        auto cfg = Service::CFG::GetModule(system);
        auto process = NS::LaunchTitle(system, FS::MediaType::NAND,
                                       GetTitleIdForApplet(applet_id, cfg->GetRegionValue()));
        if (process) {
            return ResultSuccess;
        }
    }

    // If we weren't able to load the native applet title, try to fallback to an HLE implementation.
    if (hle_applets[applet_id] != nullptr) {
        LOG_WARNING(Service_APT, "Applet has already been started id={:03X}", applet_id);
        return ResultSuccess;
    } else {
        auto parent = GetAppletSlotId(last_library_launcher_slot);
        LOG_DEBUG(Service_APT, "Creating HLE applet {:03X} with parent {:03X}", applet_id, parent);
        return CreateHLEApplet(applet_id, parent, false);
    }
}

Result AppletManager::PreloadLibraryApplet(AppletId applet_id) {
    if (GetAppletSlot(AppletSlot::LibraryApplet)->registered) {
        return {ErrorDescription::AlreadyExists, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    last_library_launcher_slot = active_slot;
    last_prepared_library_applet = applet_id;

    if (Settings::values.lle_applets) {
        auto cfg = Service::CFG::GetModule(system);
        auto process = NS::LaunchTitle(system, FS::MediaType::NAND,
                                       GetTitleIdForApplet(applet_id, cfg->GetRegionValue()));
        if (process) {
            return ResultSuccess;
        }
    }

    // If we weren't able to load the native applet title, try to fallback to an HLE implementation.
    if (hle_applets[applet_id] != nullptr) {
        LOG_WARNING(Service_APT, "Applet has already been started id={:08X}", applet_id);
        return ResultSuccess;
    } else {
        auto parent = GetAppletSlotId(last_library_launcher_slot);
        LOG_DEBUG(Service_APT, "Creating HLE applet {:03X} with parent {:03X}", applet_id, parent);
        return CreateHLEApplet(applet_id, parent, true);
    }
}

Result AppletManager::FinishPreloadingLibraryApplet(AppletId applet_id) {
    // TODO(Subv): This function should fail depending on the applet preparation state.
    GetAppletSlot(AppletSlot::LibraryApplet)->loaded = true;
    return ResultSuccess;
}

Result AppletManager::StartLibraryApplet(AppletId applet_id, std::shared_ptr<Kernel::Object> object,
                                         const std::vector<u8>& buffer) {
    active_slot = AppletSlot::LibraryApplet;

    auto send_res = SendParameter({
        .sender_id = GetAppletSlotId(last_library_launcher_slot),
        .destination_id = applet_id,
        .signal = SignalType::Wakeup,
        .object = std::move(object),
        .buffer = buffer,
    });
    if (send_res.IsError()) {
        active_slot = last_library_launcher_slot;
        return send_res;
    }

    return ResultSuccess;
}

Result AppletManager::PrepareToCloseLibraryApplet(bool not_pause, bool exiting, bool jump_home) {
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    if (!not_pause)
        library_applet_closing_command = SignalType::WakeupByPause;
    else if (jump_home)
        library_applet_closing_command = SignalType::WakeupToJumpHome;
    else if (exiting)
        library_applet_closing_command = SignalType::WakeupByCancel;
    else
        library_applet_closing_command = SignalType::WakeupByExit;

    return ResultSuccess;
}

Result AppletManager::CloseLibraryApplet(std::shared_ptr<Kernel::Object> object,
                                         const std::vector<u8>& buffer) {
    auto slot = GetAppletSlot(AppletSlot::LibraryApplet);
    auto destination_id = GetAppletSlotId(last_library_launcher_slot);

    active_slot = last_library_launcher_slot;

    MessageParameter param = {
        .sender_id = slot->applet_id,
        .destination_id = destination_id,
        .signal = library_applet_closing_command,
        .object = std::move(object),
        .buffer = buffer,
    };

    if (library_applet_closing_command != SignalType::WakeupByPause) {
        CancelAndSendParameter(param);
        // TODO: Terminate the running applet title
        slot->Reset();
    } else {
        SendParameter(param);
    }

    return ResultSuccess;
}

Result AppletManager::CancelLibraryApplet(bool app_exiting) {
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    auto slot = GetAppletSlot(AppletSlot::LibraryApplet);
    if (!slot->registered) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    return SendParameter({
        .sender_id = GetAppletSlotId(last_library_launcher_slot),
        .destination_id = slot->applet_id,
        .signal = SignalType::WakeupByCancel,
    });
}

Result AppletManager::SendDspSleep(AppletId from_applet_id,
                                   std::shared_ptr<Kernel::Object> object) {
    auto lib_slot = GetAppletSlotFromPos(AppletPos::Library);
    auto lib_app_id =
        lib_slot != AppletSlot::Error ? GetAppletSlot(lib_slot)->applet_id : AppletId::None;
    if (from_applet_id == lib_app_id) {
        SendParameter({
            .sender_id = from_applet_id,
            .destination_id = AppletId::Application,
            .signal = SignalType::DspSleep,
            .object = std::move(object),
        });
        return ResultSuccess;
    }

    auto sys_lib_slot = GetAppletSlotFromPos(AppletPos::SysLibrary);
    auto sys_lib_app_id =
        sys_lib_slot != AppletSlot::Error ? GetAppletSlot(sys_lib_slot)->applet_id : AppletId::None;
    if (from_applet_id == sys_lib_app_id) {
        auto sys_slot = GetAppletSlotFromPos(AppletPos::System);
        auto sys_app_id =
            sys_slot != AppletSlot::Error ? GetAppletSlot(sys_slot)->applet_id : AppletId::None;
        SendParameter({
            .sender_id = from_applet_id,
            .destination_id = sys_app_id,
            .signal = SignalType::DspSleep,
            .object = std::move(object),
        });
        return ResultSuccess;
    }

    return ResultSuccess;
}

Result AppletManager::SendDspWakeUp(AppletId from_applet_id,
                                    std::shared_ptr<Kernel::Object> object) {
    auto lib_slot = GetAppletSlotFromPos(AppletPos::Library);
    auto lib_app_id =
        lib_slot != AppletSlot::Error ? GetAppletSlot(lib_slot)->applet_id : AppletId::None;
    if (from_applet_id == lib_app_id) {
        SendParameter({
            .sender_id = from_applet_id,
            .destination_id = AppletId::Application,
            .signal = SignalType::DspSleep,
            .object = std::move(object),
        });
    } else {
        auto sys_lib_slot = GetAppletSlotFromPos(AppletPos::SysLibrary);
        auto sys_lib_app_id = sys_lib_slot != AppletSlot::Error
                                  ? GetAppletSlot(sys_lib_slot)->applet_id
                                  : AppletId::None;
        if (from_applet_id == sys_lib_app_id) {
            auto sys_slot = GetAppletSlotFromPos(AppletPos::System);
            auto sys_app_id =
                sys_slot != AppletSlot::Error ? GetAppletSlot(sys_slot)->applet_id : AppletId::None;
            SendParameter({
                .sender_id = from_applet_id,
                .destination_id = sys_app_id,
                .signal = SignalType::DspSleep,
                .object = std::move(object),
            });
        }
    }

    return ResultSuccess;
}

Result AppletManager::PrepareToStartSystemApplet(AppletId applet_id) {
    // The real APT service returns an error if there's a pending APT parameter when this function
    // is called.
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    last_system_launcher_slot = active_slot;
    return ResultSuccess;
}

Result AppletManager::StartSystemApplet(AppletId applet_id, std::shared_ptr<Kernel::Object> object,
                                        const std::vector<u8>& buffer) {
    auto source_applet_id = AppletId::Application;
    if (last_system_launcher_slot != AppletSlot::Error) {
        const auto launcher_slot_data = GetAppletSlot(last_system_launcher_slot);
        source_applet_id = launcher_slot_data->applet_id;

        // APT generally clears and terminates the caller of StartSystemApplet. This helps in
        // situations such as a system applet launching another system applet, which would
        // otherwise deadlock.
        // TODO: In real APT, the check for AppletSlot::Application does not exist; there is
        // TODO: something wrong with our implementation somewhere that makes this necessary.
        // TODO: Otherwise, games that attempt to launch system applets will be cleared and
        // TODO: emulation will crash.
        if (!launcher_slot_data->registered ||
            (last_system_launcher_slot != AppletSlot::Application &&
             !launcher_slot_data->attributes.no_exit_on_system_applet)) {
            launcher_slot_data->Reset();
            // TODO: Implement launcher process termination.
        }
    }

    // If a system applet is not already registered, it is started by APT.
    const auto slot_id =
        applet_id == AppletId::HomeMenu ? AppletSlot::HomeMenu : AppletSlot::SystemApplet;
    if (!GetAppletSlot(slot_id)->registered) {
        auto cfg = Service::CFG::GetModule(system);
        auto process = NS::LaunchTitle(system, FS::MediaType::NAND,
                                       GetTitleIdForApplet(applet_id, cfg->GetRegionValue()));
        if (!process) {
            // TODO: Find the right error code.
            return {ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotSupported,
                    ErrorLevel::Permanent};
        }
    }

    active_slot = slot_id;

    SendApplicationParameterAfterRegistration({
        .sender_id = source_applet_id,
        .destination_id = applet_id,
        .signal = SignalType::Wakeup,
        .object = std::move(object),
        .buffer = buffer,
    });

    return ResultSuccess;
}

Result AppletManager::PrepareToCloseSystemApplet() {
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    return ResultSuccess;
}

Result AppletManager::CloseSystemApplet(std::shared_ptr<Kernel::Object> object,
                                        const std::vector<u8>& buffer) {
    ASSERT_MSG(active_slot == AppletSlot::HomeMenu || active_slot == AppletSlot::SystemApplet,
               "Attempting to close a system applet from a non-system applet.");

    auto slot = GetAppletSlot(active_slot);
    auto closed_applet_id = slot->applet_id;

    active_slot = last_system_launcher_slot;
    slot->Reset();

    if (ordered_to_close_sys_applet) {
        ordered_to_close_sys_applet = false;

        active_slot = AppletSlot::Application;
        CancelAndSendParameter({
            .sender_id = closed_applet_id,
            .destination_id = AppletId::Application,
            .signal = SignalType::WakeupByExit,
            .object = std::move(object),
            .buffer = buffer,
        });
    }

    // TODO: Terminate the running applet title
    return ResultSuccess;
}

Result AppletManager::OrderToCloseSystemApplet() {
    if (active_slot == AppletSlot::Error) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    auto active_slot_data = GetAppletSlot(active_slot);
    if (active_slot_data->applet_id == AppletId::None ||
        active_slot_data->attributes.applet_pos != AppletPos::Application) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    auto system_slot = GetAppletSlotFromPos(AppletPos::System);
    if (system_slot == AppletSlot::Error) {
        return {ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                ErrorLevel::Status};
    }

    auto system_slot_data = GetAppletSlot(system_slot);
    if (!system_slot_data->registered) {
        return {ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                ErrorLevel::Status};
    }

    ordered_to_close_sys_applet = true;
    active_slot = system_slot;

    SendParameter({
        .sender_id = AppletId::Application,
        .destination_id = system_slot_data->applet_id,
        .signal = SignalType::WakeupByCancel,
    });

    return ResultSuccess;
}

Result AppletManager::PrepareToJumpToHomeMenu() {
    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    last_jump_to_home_slot = active_slot;

    capture_buffer_info.reset();

    if (last_jump_to_home_slot == AppletSlot::Application) {
        EnsureHomeMenuLoaded();
    }

    return ResultSuccess;
}

Result AppletManager::JumpToHomeMenu(std::shared_ptr<Kernel::Object> object,
                                     const std::vector<u8>& buffer) {
    if (last_jump_to_home_slot != AppletSlot::Error) {
        auto slot_data = GetAppletSlot(last_jump_to_home_slot);
        if (slot_data->applet_id != AppletId::None) {
            MessageParameter param;
            param.object = std::move(object);
            param.buffer = buffer;

            switch (slot_data->attributes.applet_pos) {
            case AppletPos::Application:
                active_slot = AppletSlot::HomeMenu;

                param.destination_id = AppletId::HomeMenu;
                param.sender_id = AppletId::Application;
                param.signal = SignalType::WakeupByPause;
                SendParameter(param);
                break;
            case AppletPos::Library:
                param.destination_id = slot_data->applet_id;
                param.sender_id = slot_data->applet_id;
                param.signal = SignalType::WakeupByCancel;
                SendParameter(param);
                break;
            case AppletPos::System:
                if (slot_data->attributes.is_home_menu) {
                    param.destination_id = slot_data->applet_id;
                    param.sender_id = slot_data->applet_id;
                    param.signal = SignalType::WakeupToJumpHome;
                    SendParameter(param);
                }
                break;
            case AppletPos::SysLibrary: {
                const auto system_slot_data = GetAppletSlot(AppletSlot::SystemApplet);
                param.destination_id = slot_data->applet_id;
                param.sender_id = slot_data->applet_id;
                param.signal = system_slot_data->registered ? SignalType::WakeupByCancel
                                                            : SignalType::WakeupToJumpHome;
                SendParameter(param);
                break;
            }
            default:
                break;
            }
        }
    }

    return ResultSuccess;
}

Result AppletManager::PrepareToLeaveHomeMenu() {
    if (!GetAppletSlot(AppletSlot::Application)->registered) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    return ResultSuccess;
}

Result AppletManager::LeaveHomeMenu(std::shared_ptr<Kernel::Object> object,
                                    const std::vector<u8>& buffer) {
    active_slot = AppletSlot::Application;

    SendParameter({
        .sender_id = AppletId::HomeMenu,
        .destination_id = AppletId::Application,
        .signal = SignalType::WakeupByPause,
        .object = std::move(object),
        .buffer = buffer,
    });

    return ResultSuccess;
}

Result AppletManager::OrderToCloseApplication() {
    if (active_slot == AppletSlot::Error) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    auto active_slot_data = GetAppletSlot(active_slot);
    if (active_slot_data->applet_id == AppletId::None ||
        active_slot_data->attributes.applet_pos != AppletPos::System) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    ordered_to_close_application = true;
    active_slot = AppletSlot::Application;

    SendParameter({
        .sender_id = AppletId::HomeMenu,
        .destination_id = AppletId::Application,
        .signal = SignalType::WakeupByCancel,
    });

    return ResultSuccess;
}

Result AppletManager::PrepareToCloseApplication(bool return_to_sys) {
    if (active_slot == AppletSlot::Error) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    auto active_slot_data = GetAppletSlot(active_slot);
    if (active_slot_data->applet_id == AppletId::None ||
        active_slot_data->attributes.applet_pos != AppletPos::Application) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    auto system_slot_data = GetAppletSlot(AppletSlot::SystemApplet);
    auto home_menu_slot_data = GetAppletSlot(AppletSlot::HomeMenu);

    if (!application_cancelled && return_to_sys) {
        // TODO: Left side of the OR also includes "&& !power_button_clicked", but this isn't
        // implemented yet.
        if (!ordered_to_close_application || !system_slot_data->registered) {
            application_close_target = AppletSlot::HomeMenu;
        } else {
            application_close_target = AppletSlot::SystemApplet;
        }
    } else {
        application_close_target = AppletSlot::Error;
    }

    if (application_close_target != AppletSlot::HomeMenu && !system_slot_data->registered &&
        !home_menu_slot_data->registered) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    if (next_parameter) {
        return {ErrCodes::ParameterPresent, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    if (application_close_target == AppletSlot::HomeMenu) {
        // Real APT would make sure home menu is loaded here. However, this is only really
        // needed if the home menu wasn't loaded in the first place. Since we want to
        // preserve normal behavior when the user loaded the game directly without going
        // through home menu, we skip this. Then, later we just close to the game list
        // when the application finishes closing.
        // EnsureHomeMenuLoaded();
    }

    return ResultSuccess;
}

Result AppletManager::CloseApplication(std::shared_ptr<Kernel::Object> object,
                                       const std::vector<u8>& buffer) {
    ordered_to_close_application = false;
    application_cancelled = false;

    GetAppletSlot(AppletSlot::Application)->Reset();

    if (application_close_target != AppletSlot::Error) {
        // If exiting to the home menu and it is not loaded, exit to game list.
        if (application_close_target == AppletSlot::HomeMenu &&
            !GetAppletSlot(application_close_target)->registered) {
            system.RequestShutdown();
        } else {
            active_slot = application_close_target;

            CancelAndSendParameter({
                .sender_id = AppletId::Application,
                .destination_id = GetAppletSlot(application_close_target)->applet_id,
                .signal = SignalType::WakeupByExit,
                .object = std::move(object),
                .buffer = buffer,
            });
        }
    }

    // TODO: Terminate the application process.
    return ResultSuccess;
}

ResultVal<AppletManager::AppletManInfo> AppletManager::GetAppletManInfo(
    AppletPos requested_applet_pos) {
    auto active_applet_pos = AppletPos::Invalid;
    auto active_applet_id = AppletId::None;
    if (active_slot != AppletSlot::Error) {
        auto active_slot_data = GetAppletSlot(active_slot);
        if (active_slot_data->applet_id != AppletId::None) {
            active_applet_pos = active_slot_data->attributes.applet_pos;
            active_applet_id = active_slot_data->applet_id;
        }
    }

    auto requested_applet_id = AppletId::None;
    auto requested_slot = GetAppletSlotFromPos(requested_applet_pos);
    if (requested_slot != AppletSlot::Error) {
        auto requested_slot_data = GetAppletSlot(requested_slot);
        if (requested_slot_data->registered) {
            requested_applet_id = requested_slot_data->applet_id;
        }
    }

    return AppletManInfo{
        .active_applet_pos = active_applet_pos,
        .requested_applet_id = requested_applet_id,
        .home_menu_applet_id = AppletId::HomeMenu,
        .active_applet_id = active_applet_id,
    };
}

ResultVal<AppletManager::AppletInfo> AppletManager::GetAppletInfo(AppletId app_id) {
    auto slot = GetAppletSlotFromId(app_id);
    if (slot == AppletSlot::Error) {
        return Result(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                      ErrorLevel::Status);
    }

    auto slot_data = GetAppletSlot(slot);
    if (!slot_data->registered) {
        return Result(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotFound,
                      ErrorLevel::Status);
    }

    auto media_type = Service::AM::GetTitleMediaType(slot_data->title_id);
    return AppletInfo{
        .title_id = slot_data->title_id,
        .media_type = media_type,
        .registered = slot_data->registered,
        .loaded = slot_data->loaded,
        .attributes = slot_data->attributes.raw,
    };
}

ResultVal<Service::FS::MediaType> AppletManager::Unknown54(u32 in_param) {
    auto slot_data = GetAppletSlot(AppletSlot::Application);
    if (slot_data->applet_id == AppletId::None) {
        return Result{ErrCodes::AppNotRunning, ErrorModule::Applet, ErrorSummary::InvalidState,
                      ErrorLevel::Permanent};
    }

    if (in_param >= 0x80) {
        // TODO: Add error description name when the parameter is known.
        return Result{10, ErrorModule::Applet, ErrorSummary::InvalidArgument, ErrorLevel::Usage};
    }

    // TODO: Figure out what this logic is actually for.
    auto check_target =
        in_param >= 0x40 ? Service::FS::MediaType::GameCard : Service::FS::MediaType::SDMC;
    auto check_update = in_param == 0x01 || in_param == 0x42;

    auto app_media_type = Service::AM::GetTitleMediaType(slot_data->title_id);
    auto app_update_media_type =
        Service::AM::GetTitleMediaType(Service::AM::GetTitleUpdateId(slot_data->title_id));
    if (app_media_type == check_target || (check_update && app_update_media_type == check_target)) {
        return Service::FS::MediaType::SDMC;
    } else {
        return Service::FS::MediaType::NAND;
    }
}

TargetPlatform AppletManager::GetTargetPlatform() {
    if (Settings::values.is_new_3ds.GetValue() && !new_3ds_mode_blocked) {
        return TargetPlatform::New3ds;
    } else {
        return TargetPlatform::Old3ds;
    }
}

ApplicationRunningMode AppletManager::GetApplicationRunningMode() {
    auto slot_data = GetAppletSlot(AppletSlot::Application);
    if (slot_data->applet_id == AppletId::None) {
        return ApplicationRunningMode::NoApplication;
    }

    // APT checks whether the system is a New 3DS and the 804MHz CPU speed is enabled to determine
    // the result.
    auto new_3ds_mode = GetTargetPlatform() == TargetPlatform::New3ds &&
                        system.Kernel().GetNew3dsHwCapabilities().enable_804MHz_cpu;
    if (slot_data->registered) {
        return new_3ds_mode ? ApplicationRunningMode::New3dsRegistered
                            : ApplicationRunningMode::Old3dsRegistered;
    } else {
        return new_3ds_mode ? ApplicationRunningMode::New3dsUnregistered
                            : ApplicationRunningMode::Old3dsUnregistered;
    }
}

Result AppletManager::PrepareToDoApplicationJump(u64 title_id, FS::MediaType media_type,
                                                 ApplicationJumpFlags flags) {
    // A running application can not launch another application directly because the applet state
    // for the Application slot is already in use. The way this is implemented in hardware is to
    // launch the Home Menu and tell it to launch our desired application.

    ASSERT_MSG(flags != ApplicationJumpFlags::UseStoredParameters,
               "Unimplemented application jump flags 1");

    // Save the title data to send it to the Home Menu when DoApplicationJump is called.
    auto application_slot_data = GetAppletSlot(AppletSlot::Application);
    app_jump_parameters.current_title_id = application_slot_data->title_id;
    app_jump_parameters.current_media_type =
        Service::AM::GetTitleMediaType(application_slot_data->title_id);
    if (flags == ApplicationJumpFlags::UseCurrentParameters) {
        app_jump_parameters.next_title_id = app_jump_parameters.current_title_id;
        app_jump_parameters.next_media_type = app_jump_parameters.current_media_type;
    } else {
        app_jump_parameters.next_title_id = title_id;
        app_jump_parameters.next_media_type = media_type;
    }
    app_jump_parameters.flags = flags;

    // Note: The real console uses the Home Menu to perform the application jump, therefore the menu
    // needs to be running. The real APT module starts the Home Menu here if it's not already
    // running, we don't have to do this. See `EnsureHomeMenuLoaded` for launching the Home Menu.
    return ResultSuccess;
}

Result AppletManager::DoApplicationJump(const DeliverArg& arg) {
    // Note: The real console uses the Home Menu to perform the application jump, it goes
    // OldApplication->Home Menu->NewApplication. We do not need to use the Home Menu to do this so
    // we launch the new application directly. In the real APT service, the Home Menu must be
    // running to do this, otherwise error 0xC8A0CFF0 is returned.

    auto application_slot_data = GetAppletSlot(AppletSlot::Application);
    auto title_id = application_slot_data->title_id;
    application_slot_data->Reset();

    // Set the delivery parameters.
    deliver_arg = arg;
    if (app_jump_parameters.flags != ApplicationJumpFlags::UseCurrentParameters) {
        // The source program ID is not updated when using flags 0x2.
        deliver_arg->source_program_id = title_id;
    }

    if (GetAppletSlot(AppletSlot::HomeMenu)->registered) {
        // If the home menu is running, use it to jump to the next application.
        // The home menu will call GetProgramIdOnApplicationJump and
        // PrepareToStartApplication/StartApplication to launch the title.
        active_slot = AppletSlot::HomeMenu;
        SendParameter({
            .sender_id = AppletId::Application,
            .destination_id = AppletId::HomeMenu,
            .signal = SignalType::WakeupToLaunchApplication,
        });

        // TODO: APT terminates the application here, usually it will exit itself properly though.
        return ResultSuccess;
    } else {
        // Otherwise, work around the missing home menu by launching the title directly.

        // TODO: The emulator does not support terminating the old process immediately.
        // We could call TerminateProcess but references to the process are still held elsewhere,
        // preventing clean up. This code is left commented for when this is implemented, for now we
        // cannot use NS as the old process resources would interfere with the new ones.
        /*
        auto process =
            NS::LaunchTitle(app_jump_parameters.next_media_type, app_jump_parameters.next_title_id);
        if (!process) {
            LOG_CRITICAL(Service_APT, "Failed to launch title during application jump, exiting.");
            system.RequestShutdown();
        }
        return ResultSuccess;
        */

        NS::RebootToTitle(system, app_jump_parameters.next_media_type,
                          app_jump_parameters.next_title_id);
        return ResultSuccess;
    }
}

Result AppletManager::PrepareToStartApplication(u64 title_id, FS::MediaType media_type) {
    if (active_slot == AppletSlot::Error ||
        GetAppletSlot(active_slot)->attributes.applet_pos != AppletPos::System) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    // TODO(Subv): This should return 0xc8a0cff0 if the applet preparation state is already set

    if (GetAppletSlot(AppletSlot::Application)->registered) {
        return {ErrorDescription::AlreadyExists, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    ASSERT_MSG(!app_start_parameters,
               "Trying to prepare an application when another is already prepared");

    app_start_parameters.emplace();
    app_start_parameters->next_title_id = title_id;
    app_start_parameters->next_media_type = media_type;

    capture_buffer_info.reset();

    return ResultSuccess;
}

Result AppletManager::StartApplication(const std::vector<u8>& parameter,
                                       const std::vector<u8>& hmac, bool paused) {
    // The delivery argument is always unconditionally set.
    deliver_arg.emplace(DeliverArg{parameter, hmac});

    // Note: APT first checks if we can launch the application via AM::CheckDemoLaunchRights and
    // returns 0xc8a12403 if we can't. We intentionally do not implement that check.

    // TODO(Subv): The APT service performs several checks here related to the exheader flags of the
    // process we're launching and other things like title id blacklists. We do not yet implement
    // any of that.

    // TODO(Subv): The real APT service doesn't seem to check whether the titleid to launch is set
    // or not, it either launches NATIVE_FIRM if some internal state is set, or fails when calling
    // PM::LaunchTitle. We should research more about that.
    ASSERT_MSG(app_start_parameters, "Trying to start an application without preparing it first.");

    active_slot = AppletSlot::Application;

    // Launch the title directly.
    auto process = NS::LaunchTitle(system, app_start_parameters->next_media_type,
                                   app_start_parameters->next_title_id);
    if (!process) {
        LOG_CRITICAL(Service_APT, "Failed to launch title during application start, exiting.");
        system.RequestShutdown();
    }

    app_start_parameters.reset();

    if (!paused) {
        return WakeupApplication(nullptr, {});
    }

    return ResultSuccess;
}

Result AppletManager::WakeupApplication(std::shared_ptr<Kernel::Object> object,
                                        const std::vector<u8>& buffer) {
    // Send a Wakeup signal via the apt parameter to the application once it registers itself.
    // The real APT service does this by spin waiting on another thread until the application is
    // registered.
    SendApplicationParameterAfterRegistration({
        .sender_id = AppletId::HomeMenu,
        .destination_id = AppletId::Application,
        .signal = SignalType::Wakeup,
        .object = std::move(object),
        .buffer = buffer,
    });

    return ResultSuccess;
}

Result AppletManager::CancelApplication() {
    auto application_slot_data = GetAppletSlot(AppletSlot::Application);
    if (application_slot_data->applet_id == AppletId::None) {
        return {ErrCodes::InvalidAppletSlot, ErrorModule::Applet, ErrorSummary::InvalidState,
                ErrorLevel::Status};
    }

    application_cancelled = true;

    SendApplicationParameterAfterRegistration({
        .sender_id = active_slot != AppletSlot::Error ? GetAppletSlot(active_slot)->applet_id
                                                      : AppletId::None,
        .destination_id = AppletId::Application,
        .signal = SignalType::WakeupByCancel,
    });

    return ResultSuccess;
}

void AppletManager::SendApplicationParameterAfterRegistration(const MessageParameter& parameter) {
    auto slot = GetAppletSlotFromId(parameter.destination_id);

    // If the application is already registered, immediately send the parameter
    if (slot != AppletSlot::Error && GetAppletSlot(slot)->registered) {
        CancelAndSendParameter(parameter);
        return;
    }

    // Otherwise queue it until the Application calls APT::Enable
    delayed_parameter = parameter;
}

void AppletManager::EnsureHomeMenuLoaded() {
    // TODO(Subv): The real APT service sends signal 12 (WakeupByCancel) to the currently running
    // System applet, waits for it to finish, and then launches the Home Menu.
    ASSERT_MSG(!GetAppletSlot(AppletSlot::SystemApplet)->registered,
               "A system applet is already running");

    if (GetAppletSlot(AppletSlot::HomeMenu)->registered) {
        // The Home Menu is already running.
        return;
    }

    auto cfg = Service::CFG::GetModule(system);
    auto menu_title_id = GetTitleIdForApplet(AppletId::HomeMenu, cfg->GetRegionValue());
    auto process = NS::LaunchTitle(system, FS::MediaType::NAND, menu_title_id);
    if (!process) {
        LOG_WARNING(Service_APT,
                    "The Home Menu failed to launch, application jumping will not work.");
    }
}

static u32 GetDisplayBufferModePixelSize(DisplayBufferMode mode) {
    switch (mode) {
    // NOTE: APT does in fact use pixel size 3 for R8G8B8A8 captures.
    case DisplayBufferMode::R8G8B8A8:
    case DisplayBufferMode::R8G8B8:
        return 3;
    case DisplayBufferMode::R5G6B5:
    case DisplayBufferMode::R5G5B5A1:
    case DisplayBufferMode::R4G4B4A4:
        return 2;
    case DisplayBufferMode::Unimportable:
        return 0;
    default:
        UNREACHABLE_MSG("Unknown display buffer mode {}", mode);
        return 0;
    }
}

static void CaptureFrameBuffer(Core::System& system, u32 capture_offset, VAddr src, u32 height,
                               DisplayBufferMode mode) {
    const auto bpp = GetDisplayBufferModePixelSize(mode);
    if (bpp == 0) {
        return;
    }

    system.Memory().RasterizerFlushVirtualRegion(src, GSP::FRAMEBUFFER_WIDTH * height * bpp,
                                                 Memory::FlushMode::Flush);

    // Address in VRAM that APT copies framebuffer captures to.
    constexpr VAddr screen_capture_base_vaddr = Memory::VRAM_VADDR + 0x500000;
    const auto dst_vaddr = screen_capture_base_vaddr + capture_offset;
    auto dst_ptr = system.Memory().GetPointer(dst_vaddr);
    if (!dst_ptr) {
        LOG_ERROR(Service_APT,
                  "Could not retrieve framebuffer capture destination buffer, skipping screen.");
        return;
    }

    const auto src_ptr = system.Memory().GetPointer(src);
    if (!src_ptr) {
        LOG_ERROR(Service_APT,
                  "Could not retrieve framebuffer capture source buffer, skipping screen.");
        return;
    }

    for (u32 y = 0; y < height; y++) {
        for (u32 x = 0; x < GSP::FRAMEBUFFER_WIDTH; x++) {
            const auto dst_offset = VideoCore::GetMortonOffset(x, y, bpp) +
                                    (y & ~7) * GSP::FRAMEBUFFER_WIDTH_POW2 * bpp;
            const auto src_offset = bpp * (GSP::FRAMEBUFFER_WIDTH * y + x);
            std::memcpy(dst_ptr + dst_offset, src_ptr + src_offset, bpp);
        }
    }

    system.Memory().RasterizerFlushVirtualRegion(
        dst_vaddr, GSP::FRAMEBUFFER_WIDTH_POW2 * height * bpp, Memory::FlushMode::Invalidate);
}

void AppletManager::CaptureFrameBuffers() {
    CaptureFrameBuffer(system, capture_info->bottom_screen_left_offset,
                       GSP::FRAMEBUFFER_SAVE_AREA_BOTTOM, GSP::BOTTOM_FRAMEBUFFER_HEIGHT,
                       capture_info->bottom_screen_format);
    CaptureFrameBuffer(system, capture_info->top_screen_left_offset,
                       GSP::FRAMEBUFFER_SAVE_AREA_TOP_LEFT, GSP::TOP_FRAMEBUFFER_HEIGHT,
                       capture_info->top_screen_format);
    if (capture_info->is_3d) {
        CaptureFrameBuffer(system, capture_info->top_screen_right_offset,
                           GSP::FRAMEBUFFER_SAVE_AREA_TOP_RIGHT, GSP::TOP_FRAMEBUFFER_HEIGHT,
                           capture_info->top_screen_format);
    }
}

void AppletManager::LoadInputDevices() {
    home_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.current_input_profile.buttons[Settings::NativeButton::Home]);
    power_button = Input::CreateDevice<Input::ButtonDevice>(
        Settings::values.current_input_profile.buttons[Settings::NativeButton::Power]);
}

/// Handles updating the current Applet every time it's called.
void AppletManager::HLEAppletUpdateEvent(std::uintptr_t user_data, s64 cycles_late) {
    const auto id = static_cast<AppletId>(user_data);
    const auto applet = hle_applets[id];
    if (applet == nullptr) {
        // Dead applet, exit event loop.
        LOG_WARNING(Service_APT, "Attempted to update missing applet id={:03X}", id);
        return;
    }

    if (applet->IsActive()) {
        applet->Update();
    }

    // If the applet is still running after the last update, reschedule the event
    if (applet->IsRunning()) {
        system.CoreTiming().ScheduleEvent(usToCycles(hle_applet_update_interval_us) - cycles_late,
                                          hle_applet_update_event, user_data);
    } else {
        // Otherwise the applet has terminated, in which case we should clean it up
        hle_applets[id] = nullptr;
    }
}

void AppletManager::ButtonUpdateEvent(std::uintptr_t user_data, s64 cycles_late) {
    if (is_device_reload_pending.exchange(false)) {
        LoadInputDevices();
    }

    // NOTE: We technically do support loading and jumping to home menu even if it isn't
    // initially registered. However since the home menu suspend is not bug-free, we don't
    // want normal users who didn't launch the home menu accidentally pressing the home
    // button binding and freezing their game, so for now, gate it to only environments
    // where the home menu was already loaded by the user (last condition).

    if (GetAppletSlot(AppletSlot::HomeMenu)->registered) {
        const bool home_state = home_button->GetStatus();
        if (home_state && !last_home_button_state) {
            SendNotification(Notification::HomeButtonSingle);
        }
        last_home_button_state = home_state;

        const bool power_state = power_button->GetStatus();
        if (power_state && !last_power_button_state) {
            SendNotificationToAll(Notification::PowerButtonClick);
        }
        last_power_button_state = power_state;
    }

    // Reschedule recurrent event
    system.CoreTiming().ScheduleEvent(usToCycles(button_update_interval_us) - cycles_late,
                                      button_update_event);
}

AppletManager::AppletManager(Core::System& system) : system(system) {
    lock = system.Kernel().CreateMutex(false, "APT_U:Lock");
    for (std::size_t slot = 0; slot < applet_slots.size(); ++slot) {
        auto& slot_data = applet_slots[slot];
        slot_data.slot = static_cast<AppletSlot>(slot);
        slot_data.applet_id = AppletId::None;
        slot_data.attributes.raw = 0;
        slot_data.registered = false;
        slot_data.loaded = false;
        slot_data.notification_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "APT:Notification");
        slot_data.parameter_event =
            system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "APT:Parameter");
    }
    hle_applet_update_event = system.CoreTiming().RegisterEvent(
        "HLE Applet Update Event", [this](std::uintptr_t user_data, s64 cycles_late) {
            HLEAppletUpdateEvent(user_data, cycles_late);
        });
    button_update_event = system.CoreTiming().RegisterEvent(
        "APT Button Update Event", [this](std::uintptr_t user_data, s64 cycles_late) {
            ButtonUpdateEvent(user_data, cycles_late);
        });
    system.CoreTiming().ScheduleEvent(usToCycles(button_update_interval_us), button_update_event);
}

AppletManager::~AppletManager() {
    system.CoreTiming().RemoveEvent(hle_applet_update_event);
    system.CoreTiming().RemoveEvent(button_update_event);
}

void AppletManager::ReloadInputDevices() {
    is_device_reload_pending.store(true);
}

} // namespace Service::APT
