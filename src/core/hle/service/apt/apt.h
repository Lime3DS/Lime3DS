// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "common/archives.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/global.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Mutex;
class SharedMemory;
} // namespace Kernel

namespace Service::APT {

class AppletManager;

/// Each APT service can only have up to 2 sessions connected at the same time.
static const u32 MaxAPTSessions = 2;

constexpr std::size_t SysMenuArgSize = 0x40;

enum class StartupArgumentType : u32 {
    OtherApp = 0,
    Restart = 1,
    OtherMedia = 2,
};

enum class ScreencapPostPermission : u32 {
    CleanThePermission = 0, // TODO(JamePeng): verify what "zero" means
    NoExplicitSetting = 1,
    EnableScreenshotPostingToMiiverse = 2,
    DisableScreenshotPostingToMiiverse = 3
};

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    std::shared_ptr<AppletManager> GetAppletManager() const;

    class NSInterface : public ServiceFramework<NSInterface> {
    public:
        NSInterface(std::shared_ptr<Module> apt, const char* name, u32 max_session);
        ~NSInterface();

        std::shared_ptr<Module> GetModule() const;

    protected:
        std::shared_ptr<Module> apt;

        /**
         * NS::SetWirelessRebootInfo service function. This sets the wireless reboot info.
         * Inputs:
         *     1 : size
         *     2 : (Size<<14) | 2
         *     3 : Wireless reboot info buffer ptr
         * Outputs:
         *     0 : Result of function, 0 on success, otherwise error code
         */
        void SetWirelessRebootInfo(Kernel::HLERequestContext& ctx);

        /**
         * NS::ShutdownAsync service function.
         * Inputs:
         *     1 : None
         * Outputs:
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void ShutdownAsync(Kernel::HLERequestContext& ctx);

        /**
         * NS::RebootSystem service function.
         * Inputs:
         *     1 : Boolean indicating whether to launch a title.
         *     2-3 : Title ID
         *     4 : Media Type
         *     5 : Padding
         *     6 : Launch memory type
         * Outputs:
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void RebootSystem(Kernel::HLERequestContext& ctx);

        /**
         * NS::RebootSystemClean service function.
         * Inputs:
         *     1 : None
         * Outputs:
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void RebootSystemClean(Kernel::HLERequestContext& ctx);
    };

    class APTInterface : public ServiceFramework<APTInterface> {
    public:
        APTInterface(std::shared_ptr<Module> apt, const char* name, u32 max_session);
        ~APTInterface();

        std::shared_ptr<Module> GetModule() const;

    protected:
        /**
         * APT::Initialize service function
         * Service function that initializes the APT process for the running application
         *  Outputs:
         *      1 : Result of the function, 0 on success, otherwise error code
         *      3 : Handle to the notification event
         *      4 : Handle to the pause event
         */
        void Initialize(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetSharedFont service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Virtual address of where shared font will be loaded in memory
         *      4 : Handle to shared font memory
         */
        void GetSharedFont(Kernel::HLERequestContext& ctx);

        /**
         * APT::Wrap service function
         *  Inputs:
         *      1 : Output buffer size
         *      2 : Input buffer size
         *      3 : Nonce offset to the input buffer
         *      4 : Nonce size
         *      5 : Buffer mapping descriptor ((input_buffer_size << 4) | 0xA)
         *      6 : Input buffer address
         *      7 : Buffer mapping descriptor ((input_buffer_size << 4) | 0xC)
         *      8 : Output buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Buffer unmapping descriptor ((input_buffer_size << 4) | 0xA)
         *      3 : Input buffer address
         *      4 : Buffer unmapping descriptor ((input_buffer_size << 4) | 0xC)
         *      5 : Output buffer address
         */
        void Wrap(Kernel::HLERequestContext& ctx);

        /**
         * APT::Unwrap service function
         *  Inputs:
         *      1 : Output buffer size
         *      2 : Input buffer size
         *      3 : Nonce offset to the output buffer
         *      4 : Nonce size
         *      5 : Buffer mapping descriptor ((input_buffer_size << 4) | 0xA)
         *      6 : Input buffer address
         *      7 : Buffer mapping descriptor ((input_buffer_size << 4) | 0xC)
         *      8 : Output buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Buffer unmapping descriptor ((input_buffer_size << 4) | 0xA)
         *      3 : Input buffer address
         *      4 : Buffer unmapping descriptor ((input_buffer_size << 4) | 0xC)
         *      5 : Output buffer address
         */
        void Unwrap(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetWirelessRebootInfo service function
         *  Inputs:
         *      1 : size
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output parameter buffer ptr
         */
        void GetWirelessRebootInfo(Kernel::HLERequestContext& ctx);

        /**
         * APT::NotifyToWait service function
         *  Inputs:
         *      1 : AppID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void NotifyToWait(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetLockHandle service function
         *  Inputs:
         *      1 : Applet attributes
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Applet attributes
         *      3 : Power button state
         *      4 : IPC handle descriptor
         *      5 : APT mutex handle
         */
        void GetLockHandle(Kernel::HLERequestContext& ctx);

        /**
         * APT::Enable service function
         *  Inputs:
         *      1 : Applet attributes
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Enable(Kernel::HLERequestContext& ctx);

        /**
         * APT::Finalize service function
         *  Inputs:
         *      1 : Applet ID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Finalize(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetAppletManInfo service function.
         *  Inputs:
         *      1 : Unknown
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Unknown u32 value
         *      3 : Unknown u8 value
         *      4 : Home Menu AppId
         *      5 : AppID of currently active app
         */
        void GetAppletManInfo(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetAppletInfo service function.
         *  Inputs:
         *      1 : AppId
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2-3 : Title ID
         *      4 : Media Type
         *      5 : Registered
         *      6 : Loaded
         *      7 : Attributes
         */
        void GetAppletInfo(Kernel::HLERequestContext& ctx);

        /**
         * APT::CountRegisteredApplet service function
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Number of registered applets
         */
        void CountRegisteredApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::IsRegistered service function. This returns whether the specified AppID is
         * registered with NS yet. An AppID is "registered" once the process associated with the
         * AppID uses APT:Enable. Home Menu uses this command to determine when the launched process
         * is running and to determine when to stop using GSP, etc., while displaying the "Nintendo
         * 3DS" loading screen.
         *
         *  Inputs:
         *      1 : AppID
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output, 0 = not registered, 1 = registered.
         */
        void IsRegistered(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetAttribute service function
         *  Inputs:
         *      1 : AppID
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Applet Attributes
         */
        void GetAttribute(Kernel::HLERequestContext& ctx);

        void InquireNotification(Kernel::HLERequestContext& ctx);

        /**
         * APT::SendParameter service function. This sets the parameter data state.
         * Inputs:
         *     1 : Source AppID
         *     2 : Destination AppID
         *     3 : Signal type
         *     4 : Parameter buffer size, max size is 0x1000 (this can be zero)
         *     5 : Value
         *     6 : Handle to the destination process, likely used for shared memory (this can be
         * zero)
         *     7 : (Size<<14) | 2
         *     8 : Input parameter buffer ptr
         * Outputs:
         *     0 : Return Header
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void SendParameter(Kernel::HLERequestContext& ctx);

        /**
         * APT::ReceiveParameter service function. This returns the current parameter data from NS
         * state, from the source process which set the parameters. Once finished, NS will clear a
         * flag in the NS state so that this command will return an error if this command is used
         * again if parameters were not set again. This is called when the second Initialize event
         * is triggered. It returns a signal type indicating why it was triggered.
         *  Inputs:
         *      1 : AppID
         *      2 : Parameter buffer size, max size is 0x1000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : AppID of the process which sent these parameters
         *      3 : Signal type
         *      4 : Actual parameter buffer size, this is <= to the the input size
         *      5 : Value
         *      6 : Handle from the source process which set the parameters, likely used for shared
         * memory
         *      7 : Size
         *      8 : Output parameter buffer ptr
         */
        void ReceiveParameter(Kernel::HLERequestContext& ctx);

        /**
         * APT::GlanceParameter service function. This is exactly the same as
         * APT_U::ReceiveParameter (except for the word value prior to the output handle), except
         * this will not clear the flag (except when responseword[3]==8 || responseword[3]==9) in NS
         * state.
         *  Inputs:
         *      1 : AppID
         *      2 : Parameter buffer size, max size is 0x1000
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Unknown, for now assume AppID of the process which sent these parameters
         *      3 : Unknown, for now assume Signal type
         *      4 : Actual parameter buffer size, this is <= to the the input size
         *      5 : Value
         *      6 : Handle from the source process which set the parameters, likely used for shared
         * memory
         *      7 : Size
         *      8 : Output parameter buffer ptr
         */
        void GlanceParameter(Kernel::HLERequestContext& ctx);

        /**
         * APT::CancelParameter service function. When the parameter data is available, and when the
         * above specified fields match the ones in NS state(for the ones where the checks are
         * enabled), this clears the flag which indicates that parameter data is available (same
         * flag cleared by APT:ReceiveParameter).
         *  Inputs:
         *      1 : Flag, when non-zero NS will compare the word after this one with a field in the
         * NS
         *          state.
         *      2 : Unknown, this is the same as the first unknown field returned by
         * APT:ReceiveParameter.
         *      3 : Flag, when non-zero NS will compare the word after this one with a field in the
         * NS
         *          state.
         *      4 : AppID
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Status flag, 0 = failure due to no parameter data being available, or the above
         * enabled
         *          fields don't match the fields in NS state. 1 = success.
         */
        void CancelParameter(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToStartApplication service function. When the input title-info programID is
         * zero, NS will load the actual program ID via AMNet:GetTitleIDList. After doing some
         * checks with the programID, NS will then set a NS state flag to value 1, then set the
         * programID for AppID 0x300(application) to the input program ID(or the one from
         * GetTitleIDList). A media-type field in the NS state is also set to the input media-type
         * value (other state fields are set at this point as well). With 8.0.0-18, NS will set an
         * u8 NS state field to value 1 when input flags bit8 is set.
         *  Inputs:
         *    1-4 : 0x10-byte title-info struct
         *      4 : Flags
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void PrepareToStartApplication(Kernel::HLERequestContext& ctx);

        /**
         * APT::StartApplication service function. Buf0 is copied to NS FIRMparams+0x0, then Buf1 is
         * copied to the NS FIRMparams+0x480. Then the application is launched.
         * Inputs:
         *     1 : Buffer 0 size, max size is 0x300
         *     2 : Buffer 1 size, max size is 0x20 (this can be zero)
         *     3 : u8 flag
         *     4 : (Size0<<14) | 2
         *     5 : Buffer 0 pointer
         *     6 : (Size1<<14) | 0x802
         *     7 : Buffer 1 pointer
         * Outputs:
         *     0 : Return Header
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void StartApplication(Kernel::HLERequestContext& ctx);

        /**
         * APT::WakeupApplication service function.
         *  Inputs:
         *      0 : Command header [0x001C0000]
         * Outputs:
         *     0 : Return Header
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void WakeupApplication(Kernel::HLERequestContext& ctx);

        /**
         * APT::CancelApplication service function.
         *  Inputs:
         *      0 : Command header [0x001D0000]
         * Outputs:
         *     0 : Return Header
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void CancelApplication(Kernel::HLERequestContext& ctx);

        /**
         * APT::AppletUtility service function
         *  Inputs:
         *      1 : Unknown, but clearly used for something
         *      2 : Buffer 1 size (purpose is unknown)
         *      3 : Buffer 2 size (purpose is unknown)
         *      5 : Buffer 1 address (purpose is unknown)
         *      65 : Buffer 2 address (purpose is unknown)
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void AppletUtility(Kernel::HLERequestContext& ctx);

        /**
         * APT::SetAppCpuTimeLimit service function
         *  Inputs:
         *      1 : Value, must be one
         *      2 : Percentage of CPU time from 5 to 80
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetAppCpuTimeLimit(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetAppCpuTimeLimit service function
         *  Inputs:
         *      1 : Value, must be one
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : System core CPU time percentage
         */
        void GetAppCpuTimeLimit(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToStartLibraryApplet service function
         *  Inputs:
         *      0 : Command header [0x00180040]
         *      1 : Id of the applet to start
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void PrepareToStartLibraryApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToStartSystemApplet service function
         *  Inputs:
         *      0 : Command header [0x00190040]
         *      1 : Id of the applet to start
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void PrepareToStartSystemApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToStartNewestHomeMenu service function
         *  Inputs:
         *      0 : Command header [0x001A0000]
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function
         */
        void PrepareToStartNewestHomeMenu(Kernel::HLERequestContext& ctx);

        /**
         * APT::PreloadLibraryApplet service function
         *  Inputs:
         *      0 : Command header [0x00160040]
         *      1 : Id of the applet to start
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void PreloadLibraryApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::FinishPreloadingLibraryApplet service function
         *  Inputs:
         *      0 : Command header [0x00170040]
         *      1 : Id of the applet
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void FinishPreloadingLibraryApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::StartLibraryApplet service function
         *  Inputs:
         *      0 : Command header [0x001E0084]
         *      1 : Id of the applet to start
         *      2 : Buffer size
         *      3 : Always 0?
         *      4 : Handle passed to the applet
         *      5 : (Size << 14) | 2
         *      6 : Input buffer virtual address
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StartLibraryApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::StartSystemApplet service function
         *  Inputs:
         *      0 : Command header [0x001F0084]
         *      1 : Id of the applet to start
         *      2 : Buffer size
         *      3 : 0x0
         *      4 : Handle passed to the applet
         *      5 : (Size << 14) | 2
         *      6 : Input buffer virtual address
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StartSystemApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::OrderToCloseApplication service function
         *  Inputs:
         *      0 : Command header [0x00210000]
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void OrderToCloseApplication(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToCloseApplication service function
         *  Inputs:
         *      0 : Command header [0x00220040]
         *      1 : Boolean indicating whether to cancel applet preloads.
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void PrepareToCloseApplication(Kernel::HLERequestContext& ctx);

        /**
         * APT::CloseApplication service function
         *  Inputs:
         *      0 : Command header [0x00270044]
         *      1 : Parameters Size
         *      2 : 0x0
         *      3 : Handle Parameter
         *      4 : (Parameters Size << 14) | 2
         *      5 : void*, Parameters
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void CloseApplication(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToDoApplicationJump service function
         *  Inputs:
         *      0 : Command header [0x00310100]
         *      1 : Flags
         *      2 : Program ID low
         *      3 : Program ID high
         *      4 : Media type
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         * @param ctx
         */
        void PrepareToDoApplicationJump(Kernel::HLERequestContext& ctx);

        /**
         * APT::DoApplicationJump service function
         *  Inputs:
         *      0 : Command header [0x00320084]
         *      1 : Parameter Size (capped to 0x300)
         *      2 : HMAC Size (capped to 0x20)
         *      3 : (Parameter Size << 14) | 2
         *      4 : void*, Parameter
         *      5 : (HMAC Size << 14) | 0x802
         *      6 : void*, HMAC
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DoApplicationJump(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetProgramIdOnApplicationJump service function
         *  Inputs:
         *      0 : Command header [0x00330000]
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         *    2-3 : Current Application title id
         *      4 : Current Application media type
         *    5-6 : Next Application title id to jump to
         *      7 : Next Application media type
         */
        void GetProgramIdOnApplicationJump(Kernel::HLERequestContext& ctx);

        /**
         * APT::SendDeliverArg service function
         *  Inputs:
         *      0 : Command header [0x00340084]
         *      1 : Parameter Size (capped to 0x300)
         *      2 : HMAC Size (capped to 0x20)
         *      3 : (Parameter Size << 14) | 2
         *      4 : Input buffer for Parameter
         *      5 : (HMAC Size << 14) | 0x802
         *      6 : Input buffer for HMAC
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SendDeliverArg(Kernel::HLERequestContext& ctx);

        /**
         * APT::ReceiveDeliverArg service function
         *  Inputs:
         *      0 : Command header [0x00350080]
         *      1 : Parameter Size (capped to 0x300)
         *      2 : HMAC Size (capped to 0x20)
         *     64 : (Parameter Size << 14) | 2
         *     65 : Output buffer for Parameter
         *     66 : (HMAC Size << 14) | 0x802
         *     67 : Output buffer for HMAC
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *    2-3 : Source program id
         *      4 : u8, whether the arg is received (0 = not received, 1 = received)
         */
        void ReceiveDeliverArg(Kernel::HLERequestContext& ctx);

        /**
         * APT::CancelLibraryApplet service function
         *  Inputs:
         *      0 : Command header [0x003B0040]
         *      1 : u8, Application exiting (0 = not exiting, 1 = exiting)
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void CancelLibraryApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToCloseLibraryApplet service function
         *  Inputs:
         *      0 : Command header [0x002500C0]
         *      1 : bool, Not pause
         *      2 : bool, Caller exiting
         *      3 : bool, Jump to home
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void PrepareToCloseLibraryApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToCloseSystemApplet service function
         *  Inputs:
         *      0 : Command header [0x00260000]
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void PrepareToCloseSystemApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::CloseLibraryApplet service function
         *  Inputs:
         *      0 : Command header [0x00280044]
         *      1 : Buffer size
         *      2 : 0x0
         *      3 : Object handle
         *      4 : (Size << 14) | 2
         *      5 : Input buffer virtual address
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void CloseLibraryApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::CloseSystemApplet service function
         *  Inputs:
         *      0 : Command header [0x00290044]
         *      1 : Buffer size
         *      2 : 0x0
         *      3 : Object handle
         *      4 : (Size << 14) | 2
         *      5 : Input buffer virtual address
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void CloseSystemApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::OrderToCloseSystemApplet service function
         *  Inputs:
         *      0 : Command header [0x002A0000]
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void OrderToCloseSystemApplet(Kernel::HLERequestContext& ctx);

        /**
         * APT::SendDspSleep service function
         *  Inputs:
         *      1 : Source App ID
         *      2 : Handle translation header (0x0)
         *      3 : Handle parameter
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void SendDspSleep(Kernel::HLERequestContext& ctx);

        /**
         * APT::SendDspWakeUp service function
         *  Inputs:
         *      1 : Source App ID
         *      2 : Handle translation header (0x0)
         *      3 : Handle parameter
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void SendDspWakeUp(Kernel::HLERequestContext& ctx);

        /**
         * APT::ReplySleepQuery service function
         *  Inputs:
         *      1 : Source App ID
         *      2 : Reply Value
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void ReplySleepQuery(Kernel::HLERequestContext& ctx);

        /**
         * APT::ReplySleepNotificationComplete service function
         *  Inputs:
         *      1 : Source App ID
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void ReplySleepNotificationComplete(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToJumpToHomeMenu service function
         *  Inputs:
         *      0 : Command header [0x002B0000]
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void PrepareToJumpToHomeMenu(Kernel::HLERequestContext& ctx);

        /**
         * APT::JumpToHomeMenu service function
         *  Inputs:
         *      0 : Command header [0x002C0044]
         *      1 : Buffer size
         *      2 : 0x0
         *      3 : Object handle
         *      4 : (Size << 14) | 2
         *      5 : Input buffer virtual address
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void JumpToHomeMenu(Kernel::HLERequestContext& ctx);

        /**
         * APT::PrepareToLeaveHomeMenu service function
         *  Inputs:
         *      0 : Command header [0x002B0000]
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void PrepareToLeaveHomeMenu(Kernel::HLERequestContext& ctx);

        /**
         * APT::LeaveHomeMenu service function
         *  Inputs:
         *      0 : Command header [0x002C0044]
         *      1 : Buffer size
         *      2 : 0x0
         *      3 : Object handle
         *      4 : (Size << 14) | 2
         *      5 : Input buffer virtual address
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void LeaveHomeMenu(Kernel::HLERequestContext& ctx);

        /**
         * APT::LoadSysMenuArg service function
         *  Inputs:
         *      0 : Command header [0x00360040]
         *      1 : Buffer size
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         *     64 : Size << 14 | 2
         *     65 : void* Output Buffer
         */
        void LoadSysMenuArg(Kernel::HLERequestContext& ctx);

        /**
         * APT::StoreSysMenuArg service function
         *  Inputs:
         *      0 : Command header [0x00370042]
         *      1 : Buffer size
         *      2 : (Size << 14) | 2
         *      3 : Input buffer virtual address
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void StoreSysMenuArg(Kernel::HLERequestContext& ctx);

        /**
         * APT::SendCaptureBufferInfo service function
         *  Inputs:
         *      0 : Command header [0x00400042]
         *      1 : Size
         *      2 : (Size << 14) | 2
         *      3 : void*, CaptureBufferInfo
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         */
        void SendCaptureBufferInfo(Kernel::HLERequestContext& ctx);

        /**
         * APT::ReceiveCaptureBufferInfo service function
         *  Inputs:
         *      0 : Command header [0x00410040]
         *      1 : Size
         *      64 : Size << 14 | 2
         *      65 : void*, CaptureBufferInfo
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         *      2 : Actual Size
         */
        void ReceiveCaptureBufferInfo(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetCaptureInfo service function
         *  Inputs:
         *      0 : Command header [0x004A0040]
         *      1 : Size
         *      64 : Size << 14 | 2
         *      65 : void*, CaptureBufferInfo
         *  Outputs:
         *      0 : Header code
         *      1 : Result code
         *      2 : Actual Size
         */
        void GetCaptureInfo(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetStartupArgument service function
         *  Inputs:
         *      1 : Parameter Size (capped to 0x300)
         *      2 : StartupArgumentType
         *      65 : Output buffer for startup argument
         *  Outputs:
         *      0 : Return header
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8, Exists (0 = does not exist, 1 = exists)
         */
        void GetStartupArgument(Kernel::HLERequestContext& ctx);

        /**
         * APT::Unknown54 service function
         *  Inputs:
         *      0 : Header Code[0x00540040]
         *      1 : Unknown
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Media Type
         */
        void Unknown54(Kernel::HLERequestContext& ctx);

        /**
         * APT::SetScreenCapturePostPermission service function
         *  Inputs:
         *      0 : Header Code[0x00550040]
         *      1 : u8 The screenshot posting permission
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetScreenCapturePostPermission(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetScreenCapturePostPermission service function
         *  Inputs:
         *      0 : Header Code[0x00560000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : u8 The screenshot posting permission
         */
        void GetScreenCapturePostPermission(Kernel::HLERequestContext& ctx);

        /**
         * APT::WakeupApplication2 service function.
         *  Inputs:
         *     1 : Buffer parameter size, (max is 0x1000)
         *     2 : Handle translation header (0x0)
         *     3 : Handle parameter
         *     4 : (Buffer parameter size << 14) | 2
         *     5 : Buffer parameter pointer
         * Outputs:
         *     0 : Return Header
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void WakeupApplication2(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetProgramId service function.
         *  Inputs:
         *     1 : Process ID translation header (0x20)
         *     2 : Process ID (filled in by kernel)
         * Outputs:
         *     0 : Return Header
         *     1 : Result of function, 0 on success, otherwise error code
         *     2-3 : u64 Program ID
         */
        void GetProgramId(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetProgramInfo service function.
         * Inputs:
         *     1-2 : Title ID
         *     3 : Media Type
         *     4 : Padding
         * Outputs:
         *     1 : Result of function, 0 on success, otherwise error code
         *     2 : Required app memory mode
         *     3 : Required app FIRM title ID low
         */
        void GetProgramInfo(Kernel::HLERequestContext& ctx);

        /**
         * APT::Reboot service function.
         * Inputs:
         *     1-2 : Title ID
         *     3 : Media Type
         *     4 : Padding
         *     5 : Launch memory type
         *     6 : FIRM title ID low 32-bits
         * Outputs:
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void Reboot(Kernel::HLERequestContext& ctx);

        /**
         * APT::HardwareResetAsync service function.
         * Outputs:
         *     1 : Result of function, 0 on success, otherwise error code
         */
        void HardwareResetAsync(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetTargetPlatform service function
         *  Outputs:
         *      1: Result code, 0 on success, otherwise error code
         *      2: u8 output: 0 = Old3DS, 1 = New3DS.
         *  Note:
         *  This uses PTMSYSM:CheckNew3DS.
         *  When a certain NS state field is non-zero, the output value is zero,
         *  Otherwise the output is from PTMSYSM:CheckNew3DS.
         *  Normally this NS state field is zero, however this state field is set to 1
         *  when APT:PrepareToStartApplication is used with flags bit8 is set.
         */
        void GetTargetPlatform(Kernel::HLERequestContext& ctx);

        /**
         * Wrapper for PTMSYSM:CheckNew3DS
         * APT::CheckNew3DS service function
         *  Outputs:
         *      1: Result code, 0 on success, otherwise error code
         *      2: u8 output: 0 = Old3DS, 1 = New3DS.
         */
        void CheckNew3DS(Kernel::HLERequestContext& ctx);

        /**
         * APT::GetApplicationRunningMode service function
         *  Outputs:
         *      1: Result code, 0 on success otherwise error code
         *      2: u8 output: 0 = No application, 1/3 = Old 3DS, 2/4 = New 3DS
         */
        void GetApplicationRunningMode(Kernel::HLERequestContext& ctx);

        /**
         * APT::IsStandardMemoryLayout service function
         *  Outputs:
         *      1: Result code, 0 on success otherwise error code
         *      2: bool output: Whether the system is in its standard memory layout.
         */
        void IsStandardMemoryLayout(Kernel::HLERequestContext& ctx);

        /**
         * APT::IsTitleAllowed service function
         *  Inputs:
         *      0 : Header Code[0x01050100]
         *    1-2 : Program id
         *      3 : Media type
         *      4 : Padding
         *  Outputs:
         *      1: Result code, 0 on success, otherwise error code
         *      2: u8 output, 0 if the title is not allowed, 1 if it is
         */
        void IsTitleAllowed(Kernel::HLERequestContext& ctx);

    protected:
        bool application_reset_prepared{};
        std::shared_ptr<Module> apt;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & application_reset_prepared;
        }
        friend class boost::serialization::access;
    };

private:
    bool LoadSharedFont();
    bool LoadLegacySharedFont();

    Core::System& system;

    /// Handle to shared memory region designated to for shared system font
    std::shared_ptr<Kernel::SharedMemory> shared_font_mem;
    bool shared_font_loaded = false;
    bool shared_font_relocated = false;

    u32 cpu_percent = 0; ///< CPU time available to the running application

    std::array<u8, SysMenuArgSize> sys_menu_arg_buffer;

    ScreencapPostPermission screen_capture_post_permission =
        ScreencapPostPermission::CleanThePermission; // TODO(JamePeng): verify the initial value

    std::shared_ptr<AppletManager> applet_manager;

    std::vector<u8> wireless_reboot_info;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

std::shared_ptr<Module> GetModule(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::APT

SERVICE_CONSTRUCT(Service::APT::Module)
BOOST_CLASS_VERSION(Service::APT::Module, 1)
