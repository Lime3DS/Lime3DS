// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <boost/optional.hpp>
#include <boost/serialization/version.hpp>
#include "common/common_types.h"
#include "core/arm/arm_interface.h"
#include "core/cheats/cheats.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/plgldr/plgldr.h"
#include "core/movie.h"
#include "core/perf_stats.h"

namespace Frontend {
class EmuWindow;
class ImageInterface;
class MiiSelector;
class SoftwareKeyboard;
} // namespace Frontend

namespace Memory {
class MemorySystem;
}

namespace AudioCore {
class DspInterface;
}

namespace Core::RPC {
class Server;
}

namespace Service {
namespace SM {
class ServiceManager;
}
namespace FS {
class ArchiveManager;
}
} // namespace Service

namespace Kernel {
class KernelSystem;
struct New3dsHwCapabilities;
enum class MemoryMode : u8;
} // namespace Kernel

namespace VideoDumper {
class Backend;
}

namespace VideoCore {
class CustomTexManager;
class GPU;
} // namespace VideoCore

namespace Pica {
class DebugContext;
}

namespace Loader {
class AppLoader;
}

namespace Core {

class ARM_Interface;
class TelemetrySession;
class ExclusiveMonitor;
class Timing;

class System {
public:
    /**
     * Gets the instance of the System singleton class.
     * @returns Reference to the instance of the System singleton class.
     */
    [[nodiscard]] static System& GetInstance() {
        return s_instance;
    }

    /// Enumeration representing the return values of the System Initialize and Load process.
    enum class ResultStatus : u32 {
        Success,                    ///< Succeeded
        ErrorNotInitialized,        ///< Error trying to use core prior to initialization
        ErrorGetLoader,             ///< Error finding the correct application loader
        ErrorSystemMode,            ///< Error determining the system mode
        ErrorLoader,                ///< Error loading the specified application
        ErrorLoader_ErrorEncrypted, ///< Error loading the specified application due to encryption
        ErrorLoader_ErrorInvalidFormat, ///< Error loading the specified application due to an
                                        /// invalid format
        ErrorLoader_ErrorGbaTitle, ///< Error loading the specified application as it is GBA Virtual
                                   ///< Console
        ErrorSystemFiles,          ///< Error in finding system files
        ErrorSavestate,            ///< Error saving or loading
        ShutdownRequested,         ///< Emulated program requested a system shutdown
        ErrorUnknown               ///< Any other error
    };

    explicit System();
    ~System();

    /**
     * Run the core CPU loop
     * This function runs the core for the specified number of CPU instructions before trying to
     * update hardware. This is much faster than SingleStep (and should be equivalent), as the CPU
     * is not required to do a full dispatch with each instruction. NOTE: the number of instructions
     * requested is not guaranteed to run, as this will be interrupted preemptively if a hardware
     * update is requested (e.g. on a thread switch).
     * @param tight_loop If false, the CPU single-steps.
     * @return Result status, indicating whethor or not the operation succeeded.
     */
    [[nodiscard]] ResultStatus RunLoop(bool tight_loop = true);

    /**
     * Step the CPU one instruction
     * @return Result status, indicating whethor or not the operation succeeded.
     */
    [[nodiscard]] ResultStatus SingleStep();

    /// Shutdown the emulated system.
    void Shutdown(bool is_deserializing = false);

    /// Shutdown and then load again
    void Reset();

    enum class Signal : u32 { None, Shutdown, Reset, Save, Load };

    bool SendSignal(Signal signal, u32 param = 0);

    /// Request reset of the system
    void RequestReset(const std::string& chainload = "") {
        m_chainloadpath = chainload;
        SendSignal(Signal::Reset);
    }

    /// Request shutdown of the system
    void RequestShutdown() {
        SendSignal(Signal::Shutdown);
    }

    /**
     * Load an executable application.
     * @param emu_window Reference to the host-system window used for video output and keyboard
     *                   input.
     * @param filepath String path to the executable application to load on the host file system.
     * @returns ResultStatus code, indicating if the operation succeeded.
     */
    [[nodiscard]] ResultStatus Load(Frontend::EmuWindow& emu_window, const std::string& filepath,
                                    Frontend::EmuWindow* secondary_window = {});

    /**
     * Indicates if the emulated system is powered on (all subsystems initialized and able to run an
     * application).
     * @returns True if the emulated system is powered on, otherwise false.
     */
    [[nodiscard]] bool IsPoweredOn() const {
        return is_powered_on;
    }

    /**
     * Returns a reference to the telemetry session for this emulation session.
     * @returns Reference to the telemetry session.
     */
    [[nodiscard]] Core::TelemetrySession& TelemetrySession() const {
        return *telemetry_session;
    }

    /// Prepare the core emulation for a reschedule
    void PrepareReschedule();

    [[nodiscard]] PerfStats::Results GetAndResetPerfStats();

    [[nodiscard]] PerfStats::Results GetLastPerfStats();

    /**
     * Gets a reference to the emulated CPU.
     * @returns A reference to the emulated CPU.
     */

    [[nodiscard]] ARM_Interface& GetRunningCore() {
        return *running_core;
    };

    /**
     * Gets a reference to the emulated CPU.
     * @param core_id The id of the core requested.
     * @returns A reference to the emulated CPU.
     */

    [[nodiscard]] ARM_Interface& GetCore(u32 core_id) {
        return *cpu_cores[core_id];
    };

    [[nodiscard]] const ARM_Interface& GetCore(u32 core_id) const {
        return *cpu_cores[core_id];
    };

    [[nodiscard]] u32 GetNumCores() const {
        return static_cast<u32>(cpu_cores.size());
    }

    void InvalidateCacheRange(u32 start_address, std::size_t length) {
        for (const auto& cpu : cpu_cores) {
            cpu->InvalidateCacheRange(start_address, length);
        }
    }

    /**
     * Gets a reference to the emulated DSP.
     * @returns A reference to the emulated DSP.
     */
    [[nodiscard]] AudioCore::DspInterface& DSP() {
        return *dsp_core;
    }

    [[nodiscard]] VideoCore::GPU& GPU();

    /**
     * Gets a reference to the service manager.
     * @returns A reference to the service manager.
     */
    [[nodiscard]] Service::SM::ServiceManager& ServiceManager();

    /**
     * Gets a const reference to the service manager.
     * @returns A const reference to the service manager.
     */
    [[nodiscard]] const Service::SM::ServiceManager& ServiceManager() const;

    /// Gets a reference to the archive manager
    [[nodiscard]] Service::FS::ArchiveManager& ArchiveManager();

    /// Gets a const reference to the archive manager
    [[nodiscard]] const Service::FS::ArchiveManager& ArchiveManager() const;

    /// Gets a reference to the kernel
    [[nodiscard]] Kernel::KernelSystem& Kernel();

    /// Gets a const reference to the kernel
    [[nodiscard]] const Kernel::KernelSystem& Kernel() const;

    /// Get kernel is running
    [[nodiscard]] bool KernelRunning();

    /// Gets a reference to the timing system
    [[nodiscard]] Timing& CoreTiming();

    /// Gets a const reference to the timing system
    [[nodiscard]] const Timing& CoreTiming() const;

    /// Gets a reference to the memory system
    [[nodiscard]] Memory::MemorySystem& Memory();

    /// Gets a const reference to the memory system
    [[nodiscard]] const Memory::MemorySystem& Memory() const;

    /// Gets a reference to the cheat engine
    [[nodiscard]] Cheats::CheatEngine& CheatEngine();

    /// Gets a const reference to the cheat engine
    [[nodiscard]] const Cheats::CheatEngine& CheatEngine() const;

    /// Gets a reference to the custom texture cache system
    [[nodiscard]] VideoCore::CustomTexManager& CustomTexManager();

    /// Gets a const reference to the custom texture cache system
    [[nodiscard]] const VideoCore::CustomTexManager& CustomTexManager() const;

    /// Gets a reference to the movie recorder
    [[nodiscard]] Core::Movie& Movie();

    /// Gets a const reference to the movie recorder
    [[nodiscard]] const Core::Movie& Movie() const;

    /// Video Dumper interface

    void RegisterVideoDumper(std::shared_ptr<VideoDumper::Backend> video_dumper);

    [[nodiscard]] std::shared_ptr<VideoDumper::Backend> GetVideoDumper() const {
        return video_dumper;
    }

    std::unique_ptr<PerfStats> perf_stats;
    FrameLimiter frame_limiter;

    void SetStatus(ResultStatus new_status, const char* details = nullptr) {
        status = new_status;
        if (details) {
            status_details = details;
        }
    }

    [[nodiscard]] const std::string& GetStatusDetails() const {
        return status_details;
    }

    [[nodiscard]] Loader::AppLoader& GetAppLoader() const {
        return *app_loader;
    }

    /// Frontend Applets

    void RegisterMiiSelector(std::shared_ptr<Frontend::MiiSelector> mii_selector);

    void RegisterSoftwareKeyboard(std::shared_ptr<Frontend::SoftwareKeyboard> swkbd);

    [[nodiscard]] std::shared_ptr<Frontend::MiiSelector> GetMiiSelector() const {
        return registered_mii_selector;
    }

    [[nodiscard]] std::shared_ptr<Frontend::SoftwareKeyboard> GetSoftwareKeyboard() const {
        return registered_swkbd;
    }

    /// Image interface

    void RegisterImageInterface(std::shared_ptr<Frontend::ImageInterface> image_interface);

    [[nodiscard]] std::shared_ptr<Frontend::ImageInterface> GetImageInterface() const {
        return registered_image_interface;
    }

    /// Function for checking OS microphone permissions.

    void RegisterMicPermissionCheck(const std::function<bool()>& permission_func) {
        mic_permission_func = permission_func;
    }

    [[nodiscard]] bool HasMicPermission() {
        return !mic_permission_func || mic_permission_granted ||
               (mic_permission_granted = mic_permission_func());
    }

    void SaveState(u32 slot) const;

    void LoadState(u32 slot);

    /// Self delete ncch
    bool SetSelfDelete(const std::string& file) {
        if (m_filepath == file) {
            self_delete_pending = true;
            return true;
        }
        return false;
    }

    /// Applies any changes to settings to this core instance.
    void ApplySettings();

private:
    /**
     * Initialize the emulated system.
     * @param emu_window Reference to the host-system window used for video output and keyboard
     *                   input.
     * @param system_mode The system mode.
     * @return ResultStatus code, indicating if the operation succeeded.
     */
    [[nodiscard]] ResultStatus Init(Frontend::EmuWindow& emu_window,
                                    Frontend::EmuWindow* secondary_window,
                                    Kernel::MemoryMode memory_mode,
                                    const Kernel::New3dsHwCapabilities& n3ds_hw_caps,
                                    u32 num_cores);

    /// Reschedule the core emulation
    void Reschedule();

    /// AppLoader used to load the current executing application
    std::unique_ptr<Loader::AppLoader> app_loader;

    /// ARM11 CPU core
    std::vector<std::shared_ptr<ARM_Interface>> cpu_cores;
    ARM_Interface* running_core = nullptr;

    /// DSP core
    std::unique_ptr<AudioCore::DspInterface> dsp_core;

    /// When true, signals that a reschedule should happen
    bool reschedule_pending{};

    /// Telemetry session for this emulation session
    std::unique_ptr<Core::TelemetrySession> telemetry_session;

    std::unique_ptr<VideoCore::GPU> gpu;

    /// Service manager
    std::unique_ptr<Service::SM::ServiceManager> service_manager;

    /// Frontend applets
    std::shared_ptr<Frontend::MiiSelector> registered_mii_selector;
    std::shared_ptr<Frontend::SoftwareKeyboard> registered_swkbd;

    /// Movie recorder
    Core::Movie movie;

    /// Cheats manager
    Cheats::CheatEngine cheat_engine;

    /// Video dumper backend
    std::shared_ptr<VideoDumper::Backend> video_dumper;

    /// Custom texture cache system
    std::unique_ptr<VideoCore::CustomTexManager> custom_tex_manager;

    /// Image interface
    std::shared_ptr<Frontend::ImageInterface> registered_image_interface;

#ifdef ENABLE_SCRIPTING
    /// RPC Server for scripting support
    std::unique_ptr<RPC::Server> rpc_server;
#endif

    std::unique_ptr<Service::FS::ArchiveManager> archive_manager;

    std::unique_ptr<Memory::MemorySystem> memory;
    std::unique_ptr<Kernel::KernelSystem> kernel;
    std::unique_ptr<Timing> timing;

    std::unique_ptr<Core::ExclusiveMonitor> exclusive_monitor;

private:
    static System s_instance;

    std::atomic_bool is_powered_on{};

    ResultStatus status = ResultStatus::Success;
    std::string status_details = "";
    /// Saved variables for reset
    Frontend::EmuWindow* m_emu_window;
    Frontend::EmuWindow* m_secondary_window;
    std::string m_filepath;
    std::string m_chainloadpath;
    u64 title_id;
    bool self_delete_pending;

    std::mutex signal_mutex;
    Signal current_signal;
    u32 signal_param;

    std::function<bool()> mic_permission_func;
    bool mic_permission_granted = false;

    boost::optional<Service::APT::DeliverArg> restore_deliver_arg;
    boost::optional<Service::PLGLDR::PLG_LDR::PluginLoaderContext> restore_plugin_context;

    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive& ar, const unsigned int file_version);
};

[[nodiscard]] inline ARM_Interface& GetRunningCore() {
    return System::GetInstance().GetRunningCore();
}

[[nodiscard]] inline ARM_Interface& GetCore(u32 core_id) {
    return System::GetInstance().GetCore(core_id);
}

[[nodiscard]] inline u32 GetNumCores() {
    return System::GetInstance().GetNumCores();
}

} // namespace Core

BOOST_CLASS_VERSION(Core::System, 1)
