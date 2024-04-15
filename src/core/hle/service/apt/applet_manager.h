// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <vector>
#include <boost/serialization/array.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/vector.hpp>
#include "core/frontend/input.h"
#include "core/global.h"
#include "core/hle/kernel/event.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"

namespace Core {
class System;
}

namespace HLE::Applets {
class Applet;
}

namespace Service::APT {

/// Signals used by APT functions
enum class SignalType : u32 {
    None = 0x0,
    Wakeup = 0x1,
    Request = 0x2,
    Response = 0x3,
    Exit = 0x4,
    Message = 0x5,
    HomeButtonSingle = 0x6,
    HomeButtonDouble = 0x7,
    DspSleep = 0x8,
    DspWakeup = 0x9,
    WakeupByExit = 0xA,
    WakeupByPause = 0xB,
    WakeupByCancel = 0xC,
    WakeupByCancelAll = 0xD,
    WakeupByPowerButtonClick = 0xE,
    WakeupToJumpHome = 0xF,
    RequestForSysApplet = 0x10,
    WakeupToLaunchApplication = 0x11,
};

enum class Notification : u32 {
    None = 0,
    HomeButtonSingle = 1,
    HomeButtonDouble = 2,
    SleepQuery = 3,
    SleepCancelledByOpen = 4,
    SleepAccepted = 5,
    SleepAwake = 6,
    Shutdown = 7,
    PowerButtonClick = 8,
    PowerButtonClear = 9,
    TrySleep = 10,
    OrderToClose = 11,
};

/// App Id's used by APT functions
enum class AppletId : u32 {
    None = 0,
    AnySystemApplet = 0x100,
    HomeMenu = 0x101,
    AlternateMenu = 0x103,
    Camera = 0x110,
    FriendList = 0x112,
    GameNotes = 0x113,
    InternetBrowser = 0x114,
    InstructionManual = 0x115,
    Notifications = 0x116,
    Miiverse = 0x117,
    MiiversePost = 0x118,
    AmiiboSettings = 0x119,
    AnySysLibraryApplet = 0x200,
    SoftwareKeyboard1 = 0x201,
    Ed1 = 0x202,
    PnoteApp = 0x204,
    SnoteApp = 0x205,
    Error = 0x206,
    Mint = 0x207,
    Extrapad = 0x208,
    Memolib = 0x209,
    Application = 0x300,
    Tiger = 0x301,
    AnyLibraryApplet = 0x400,
    SoftwareKeyboard2 = 0x401,
    Ed2 = 0x402,
    PnoteApp2 = 0x404,
    SnoteApp2 = 0x405,
    Error2 = 0x406,
    Mint2 = 0x407,
    Extrapad2 = 0x408,
    Memolib2 = 0x409,
};

/// Application Old/New 3DS target platforms
enum class TargetPlatform : u8 {
    Old3ds = 0,
    New3ds = 1,
};

/// Application Old/New 3DS running modes
enum class ApplicationRunningMode : u8 {
    NoApplication = 0,
    Old3dsRegistered = 1,
    New3dsRegistered = 2,
    Old3dsUnregistered = 3,
    New3dsUnregistered = 4,
};

/// Holds information about the parameters used in Send/Glance/ReceiveParameter
struct MessageParameter {
    AppletId sender_id = AppletId::None;
    AppletId destination_id = AppletId::None;
    SignalType signal = SignalType::None;
    std::shared_ptr<Kernel::Object> object = nullptr;
    std::vector<u8> buffer;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & sender_id;
        ar & destination_id;
        ar & signal;
        ar & object;
        ar & buffer;
    }
    friend class boost::serialization::access;
};

enum class AppletPos : u32 {
    Application = 0,
    Library = 1,
    System = 2,
    SysLibrary = 3,
    Resident = 4,
    AutoLibrary = 5,
    Invalid = 0xFF,
};

union AppletAttributes {
    u32 raw;

    BitField<0, 3, AppletPos> applet_pos;
    BitField<28, 1, u32> no_exit_on_system_applet;
    BitField<29, 1, u32> is_home_menu;

    AppletAttributes() : raw(0) {}
    AppletAttributes(u32 attributes) : raw(attributes) {}
};

enum class ApplicationJumpFlags : u8 {
    UseInputParameters = 0,
    UseStoredParameters = 1,
    UseCurrentParameters = 2
};

struct DeliverArg {
    std::vector<u8> param;
    std::vector<u8> hmac;
    u64 source_program_id = std::numeric_limits<u64>::max();

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & param;
        ar & hmac;
        ar & source_program_id;
    }
    friend class boost::serialization::access;
};

struct ApplicationJumpParameters {
    u64 next_title_id;
    FS::MediaType next_media_type;
    ApplicationJumpFlags flags;

    u64 current_title_id;
    FS::MediaType current_media_type;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar & next_title_id;
        ar & next_media_type;
        ar & flags;
        ar & current_title_id;
        ar & current_media_type;
    }
    friend class boost::serialization::access;
};

struct ApplicationStartParameters {
    u64 next_title_id;
    FS::MediaType next_media_type;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & next_title_id;
        ar & next_media_type;
    }
    friend class boost::serialization::access;
};

enum class DisplayBufferMode : u32_le {
    R8G8B8A8 = 0,
    R8G8B8 = 1,
    R5G6B5 = 2,
    R5G5B5A1 = 3,
    R4G4B4A4 = 4,
    Unimportable = 0xFFFFFFFF,
};

/// Used by the application to pass information about the current framebuffer to applets.
struct CaptureBufferInfo {
    u32_le size;
    u8 is_3d;
    INSERT_PADDING_BYTES(0x3); // Padding for alignment
    u32_le top_screen_left_offset;
    u32_le top_screen_right_offset;
    DisplayBufferMode top_screen_format;
    u32_le bottom_screen_left_offset;
    u32_le bottom_screen_right_offset;
    DisplayBufferMode bottom_screen_format;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & size;
        ar & is_3d;
        ar & top_screen_left_offset;
        ar & top_screen_right_offset;
        ar & top_screen_format;
        ar & bottom_screen_left_offset;
        ar & bottom_screen_right_offset;
        ar & bottom_screen_format;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(CaptureBufferInfo) == 0x20, "CaptureBufferInfo struct has incorrect size");

enum class SleepQueryReply : u32 {
    Reject = 0,
    Accept = 1,
    Later = 2,
};

class AppletManager : public std::enable_shared_from_this<AppletManager> {
public:
    explicit AppletManager(Core::System& system);
    ~AppletManager();

    void ReloadInputDevices();

    /**
     * Clears any existing parameter and places a new one. This function is currently only used by
     * HLE Applets and should be likely removed in the future
     */
    void CancelAndSendParameter(const MessageParameter& parameter);

    Result SendParameter(const MessageParameter& parameter);
    ResultVal<MessageParameter> GlanceParameter(AppletId app_id);
    ResultVal<MessageParameter> ReceiveParameter(AppletId app_id);
    bool CancelParameter(bool check_sender, AppletId sender_appid, bool check_receiver,
                         AppletId receiver_appid);

    struct GetLockHandleResult {
        AppletAttributes corrected_attributes;
        u32 state;
        std::shared_ptr<Kernel::Mutex> lock;
    };
    ResultVal<GetLockHandleResult> GetLockHandle(AppletAttributes attributes);

    struct InitializeResult {
        std::shared_ptr<Kernel::Event> notification_event;
        std::shared_ptr<Kernel::Event> parameter_event;
    };
    ResultVal<InitializeResult> Initialize(AppletId app_id, AppletAttributes attributes);

    Result Enable(AppletAttributes attributes);
    Result Finalize(AppletId app_id);
    u32 CountRegisteredApplet();
    bool IsRegistered(AppletId app_id);
    ResultVal<AppletAttributes> GetAttribute(AppletId app_id);

    ResultVal<Notification> InquireNotification(AppletId app_id);
    Result SendNotification(Notification notification);

    Result PrepareToStartLibraryApplet(AppletId applet_id);
    Result PreloadLibraryApplet(AppletId applet_id);
    Result FinishPreloadingLibraryApplet(AppletId applet_id);
    Result StartLibraryApplet(AppletId applet_id, std::shared_ptr<Kernel::Object> object,
                              const std::vector<u8>& buffer);
    Result PrepareToCloseLibraryApplet(bool not_pause, bool exiting, bool jump_home);
    Result CloseLibraryApplet(std::shared_ptr<Kernel::Object> object,
                              const std::vector<u8>& buffer);
    Result CancelLibraryApplet(bool app_exiting);

    Result SendDspSleep(AppletId from_applet_id, std::shared_ptr<Kernel::Object> object);
    Result SendDspWakeUp(AppletId from_applet_id, std::shared_ptr<Kernel::Object> object);

    Result PrepareToStartSystemApplet(AppletId applet_id);
    Result StartSystemApplet(AppletId applet_id, std::shared_ptr<Kernel::Object> object,
                             const std::vector<u8>& buffer);
    Result PrepareToCloseSystemApplet();
    Result CloseSystemApplet(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer);
    Result OrderToCloseSystemApplet();

    Result PrepareToJumpToHomeMenu();
    Result JumpToHomeMenu(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer);
    Result PrepareToLeaveHomeMenu();
    Result LeaveHomeMenu(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer);

    Result OrderToCloseApplication();
    Result PrepareToCloseApplication(bool return_to_sys);
    Result CloseApplication(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer);

    Result PrepareToDoApplicationJump(u64 title_id, FS::MediaType media_type,
                                      ApplicationJumpFlags flags);
    Result DoApplicationJump(const DeliverArg& arg);

    boost::optional<DeliverArg> ReceiveDeliverArg() {
        auto arg = deliver_arg;
        deliver_arg = boost::none;
        return arg;
    }
    void SetDeliverArg(boost::optional<DeliverArg> arg) {
        deliver_arg = std::move(arg);
    }

    std::vector<u8> GetCaptureInfo() {
        std::vector<u8> buffer;
        if (capture_info) {
            buffer.resize(sizeof(CaptureBufferInfo));
            std::memcpy(buffer.data(), &capture_info.get(), sizeof(CaptureBufferInfo));
        }
        return buffer;
    }
    void SetCaptureInfo(std::vector<u8> buffer) {
        ASSERT_MSG(buffer.size() >= sizeof(CaptureBufferInfo), "CaptureBufferInfo is too small.");

        capture_info.emplace();
        std::memcpy(&capture_info.get(), buffer.data(), sizeof(CaptureBufferInfo));
    }

    std::vector<u8> ReceiveCaptureBufferInfo() {
        std::vector<u8> buffer;
        if (capture_buffer_info) {
            buffer.resize(sizeof(CaptureBufferInfo));
            std::memcpy(buffer.data(), &capture_buffer_info.get(), sizeof(CaptureBufferInfo));
            capture_buffer_info.reset();
        }
        return buffer;
    }
    void SendCaptureBufferInfo(std::vector<u8> buffer) {
        ASSERT_MSG(buffer.size() >= sizeof(CaptureBufferInfo), "CaptureBufferInfo is too small.");

        capture_buffer_info.emplace();
        std::memcpy(&capture_buffer_info.get(), buffer.data(), sizeof(CaptureBufferInfo));
    }

    Result PrepareToStartApplication(u64 title_id, FS::MediaType media_type);
    Result StartApplication(const std::vector<u8>& parameter, const std::vector<u8>& hmac,
                            bool paused);
    Result WakeupApplication(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer);
    Result CancelApplication();

    struct AppletManInfo {
        AppletPos active_applet_pos;
        AppletId requested_applet_id;
        AppletId home_menu_applet_id;
        AppletId active_applet_id;
    };

    ResultVal<AppletManInfo> GetAppletManInfo(AppletPos requested_applet_pos);

    struct AppletInfo {
        u64 title_id;
        Service::FS::MediaType media_type;
        bool registered;
        bool loaded;
        u32 attributes;
    };
    ResultVal<AppletInfo> GetAppletInfo(AppletId app_id);

    ApplicationJumpParameters GetApplicationJumpParameters() const {
        return app_jump_parameters;
    }

    ResultVal<Service::FS::MediaType> Unknown54(u32 in_param);
    TargetPlatform GetTargetPlatform();
    ApplicationRunningMode GetApplicationRunningMode();

private:
    /// APT lock retrieved via GetLockHandle.
    std::shared_ptr<Kernel::Mutex> lock;

    /// Parameter data to be returned in the next call to Glance/ReceiveParameter.
    // NOTE: A bug in gcc prevents serializing std::optional
    boost::optional<MessageParameter> next_parameter;

    /// This parameter will be sent to the application/applet once they register themselves by using
    /// APT::Initialize.
    boost::optional<MessageParameter> delayed_parameter;

    ApplicationJumpParameters app_jump_parameters{};
    boost::optional<ApplicationStartParameters> app_start_parameters{};
    boost::optional<DeliverArg> deliver_arg{};

    boost::optional<CaptureBufferInfo> capture_info;
    boost::optional<CaptureBufferInfo> capture_buffer_info;

    static constexpr std::size_t NumAppletSlot = 4;

    enum class AppletSlot : u8 {
        Application,
        SystemApplet,
        HomeMenu,
        LibraryApplet,

        // An invalid tag
        Error,
    };

    struct AppletSlotData {
        AppletId applet_id;
        AppletSlot slot;
        u64 title_id;
        bool registered;
        bool loaded;
        AppletAttributes attributes;
        Notification notification;
        std::shared_ptr<Kernel::Event> notification_event;
        std::shared_ptr<Kernel::Event> parameter_event;

        void Reset() {
            applet_id = AppletId::None;
            registered = false;
            title_id = 0;
            attributes.raw = 0;
        }

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & applet_id;
            ar & slot;
            ar & title_id;
            ar & registered;
            ar & loaded;
            ar & attributes.raw;
            ar & notification;
            ar & notification_event;
            ar & parameter_event;
        }
        friend class boost::serialization::access;
    };

    // Holds data about the concurrently running applets in the system.
    std::array<AppletSlotData, NumAppletSlot> applet_slots = {};
    AppletSlot active_slot = AppletSlot::Error;

    AppletSlot last_library_launcher_slot = AppletSlot::Error;
    SignalType library_applet_closing_command = SignalType::None;
    AppletId last_prepared_library_applet = AppletId::None;
    AppletSlot last_system_launcher_slot = AppletSlot::Error;
    AppletSlot last_jump_to_home_slot = AppletSlot::Error;
    bool ordered_to_close_sys_applet = false;
    bool ordered_to_close_application = false;
    bool application_cancelled = false;
    AppletSlot application_close_target = AppletSlot::Error;

    // This flag is used to determine if an app that supports New 3DS capabilities should use them.
    // It also affects the results of APT:GetTargetPlatform and APT:GetApplicationRunningMode.
    bool new_3ds_mode_blocked = false;

    std::unordered_map<AppletId, std::shared_ptr<HLE::Applets::Applet>> hle_applets;
    Core::TimingEventType* hle_applet_update_event;

    Core::TimingEventType* button_update_event;
    std::atomic<bool> is_device_reload_pending{true};
    std::unique_ptr<Input::ButtonDevice> home_button;
    std::unique_ptr<Input::ButtonDevice> power_button;
    bool last_home_button_state = false;
    bool last_power_button_state = false;

    Core::System& system;

    AppletSlotData* GetAppletSlot(AppletSlot slot) {
        return &applet_slots[static_cast<std::size_t>(slot)];
    }

    AppletId GetAppletSlotId(AppletSlot slot) {
        return slot != AppletSlot::Error ? GetAppletSlot(slot)->applet_id : AppletId::None;
    }

    AppletSlot GetAppletSlotFromId(AppletId id);
    AppletSlot GetAppletSlotFromAttributes(AppletAttributes attributes);
    AppletSlot GetAppletSlotFromPos(AppletPos pos);

    /// Checks if the Application slot has already been registered and sends the parameter to it,
    /// otherwise it queues for sending when the application registers itself with APT::Enable.
    void SendApplicationParameterAfterRegistration(const MessageParameter& parameter);

    void SendNotificationToAll(Notification notification);

    void EnsureHomeMenuLoaded();

    void CaptureFrameBuffers();

    Result CreateHLEApplet(AppletId id, AppletId parent, bool preload);
    void HLEAppletUpdateEvent(std::uintptr_t user_data, s64 cycles_late);

    void LoadInputDevices();
    void ButtonUpdateEvent(std::uintptr_t user_data, s64 cycles_late);

    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar & next_parameter;
        ar & app_jump_parameters;
        ar & delayed_parameter;
        ar & app_start_parameters;
        ar & deliver_arg;
        ar & capture_info;
        ar & capture_buffer_info;
        ar & active_slot;
        ar & last_library_launcher_slot;
        ar & last_prepared_library_applet;
        ar & last_system_launcher_slot;
        ar & last_jump_to_home_slot;
        ar & ordered_to_close_sys_applet;
        ar & ordered_to_close_application;
        ar & application_cancelled;
        ar & application_close_target;
        ar & new_3ds_mode_blocked;
        ar & lock;
        ar & capture_info;
        ar & applet_slots;
        ar & library_applet_closing_command;

        if (Archive::is_loading::value) {
            LoadInputDevices();
        }
    }
    friend class boost::serialization::access;
};

} // namespace Service::APT

BOOST_CLASS_VERSION(Service::APT::ApplicationJumpParameters, 1)
BOOST_CLASS_VERSION(Service::APT::AppletManager, 1)

SERVICE_CONSTRUCT(Service::APT::AppletManager)
