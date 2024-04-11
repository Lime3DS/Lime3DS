// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdexcept>
#include <utility>
#include <boost/serialization/array.hpp>
#include "audio_core/dsp_interface.h"
#include "audio_core/hle/hle.h"
#include "audio_core/lle/lle.h"
#include "common/arch.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/arm/arm_interface.h"
#include "core/arm/exclusive_monitor.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_user.h"
#if CITRA_ARCH(x86_64) || CITRA_ARCH(arm64)
#include "core/arm/dynarmic/arm_dynarmic.h"
#endif
#include "core/arm/dyncom/arm_dyncom.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/dumping/backend.h"
#include "core/frontend/image_interface.h"
#include "core/gdbstub/gdbstub.h"
#include "core/global.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/service/apt/applet_manager.h"
#include "core/hle/service/apt/apt.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/gsp/gsp_gpu.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/mic/mic_u.h"
#include "core/hle/service/plgldr/plgldr.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hw/aes/key.h"
#include "core/loader/loader.h"
#include "core/movie.h"
#ifdef ENABLE_SCRIPTING
#include "core/rpc/server.h"
#endif
#include "core/telemetry_session.h"
#include "network/network.h"
#include "video_core/custom_textures/custom_tex_manager.h"
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"

namespace Core {

/*static*/ System System::s_instance;

template <>
Core::System& Global() {
    return System::GetInstance();
}

template <>
Kernel::KernelSystem& Global() {
    return System::GetInstance().Kernel();
}

template <>
Core::Timing& Global() {
    return System::GetInstance().CoreTiming();
}

System::System() : movie{*this}, cheat_engine{*this} {}

System::~System() = default;

System::ResultStatus System::RunLoop(bool tight_loop) {
    status = ResultStatus::Success;
    if (!IsPoweredOn()) {
        return ResultStatus::ErrorNotInitialized;
    }

    if (GDBStub::IsServerEnabled()) {
        Kernel::Thread* thread = kernel->GetCurrentThreadManager().GetCurrentThread();
        if (thread && running_core) {
            running_core->SaveContext(thread->context);
        }
        GDBStub::HandlePacket(*this);

        // If the loop is halted and we want to step, use a tiny (1) number of instructions to
        // execute. Otherwise, get out of the loop function.
        if (GDBStub::GetCpuHaltFlag()) {
            if (GDBStub::GetCpuStepFlag()) {
                tight_loop = false;
            } else {
                return ResultStatus::Success;
            }
        }
    }

    Signal signal{Signal::None};
    u32 param{};
    {
        std::scoped_lock lock{signal_mutex};
        if (current_signal != Signal::None) {
            signal = current_signal;
            param = signal_param;
            current_signal = Signal::None;
        }
    }
    switch (signal) {
    case Signal::Reset:
        Reset();
        return ResultStatus::Success;
    case Signal::Shutdown:
        return ResultStatus::ShutdownRequested;
    case Signal::Load: {
        const u32 slot = param;
        LOG_INFO(Core, "Begin load of slot {}", slot);
        try {
            System::LoadState(slot);
            LOG_INFO(Core, "Load completed");
        } catch (const std::exception& e) {
            LOG_ERROR(Core, "Error loading: {}", e.what());
            status_details = e.what();
            return ResultStatus::ErrorSavestate;
        }
        frame_limiter.WaitOnce();
        return ResultStatus::Success;
    }
    case Signal::Save: {
        const u32 slot = param;
        LOG_INFO(Core, "Begin save to slot {}", slot);
        try {
            System::SaveState(slot);
            LOG_INFO(Core, "Save completed");
        } catch (const std::exception& e) {
            LOG_ERROR(Core, "Error saving: {}", e.what());
            status_details = e.what();
            return ResultStatus::ErrorSavestate;
        }
        frame_limiter.WaitOnce();
        return ResultStatus::Success;
    }
    default:
        break;
    }

    // All cores should have executed the same amount of ticks. If this is not the case an event was
    // scheduled with a cycles_into_future smaller then the current downcount.
    // So we have to get those cores to the same global time first
    u64 global_ticks = timing->GetGlobalTicks();
    s64 max_delay = 0;
    ARM_Interface* current_core_to_execute = nullptr;
    for (auto& cpu_core : cpu_cores) {
        if (cpu_core->GetTimer().GetTicks() < global_ticks) {
            s64 delay = global_ticks - cpu_core->GetTimer().GetTicks();
            running_core = cpu_core.get();
            kernel->SetRunningCPU(running_core);
            cpu_core->GetTimer().Advance();
            cpu_core->PrepareReschedule();
            kernel->GetThreadManager(cpu_core->GetID()).Reschedule();
            cpu_core->GetTimer().SetNextSlice(delay);
            if (max_delay < delay) {
                max_delay = delay;
                current_core_to_execute = cpu_core.get();
            }
        }
    }

    // jit sometimes overshoot by a few ticks which might lead to a minimal desync in the cores.
    // This small difference shouldn't make it necessary to sync the cores and would only cost
    // performance. Thus we don't sync delays below min_delay
    static constexpr s64 min_delay = 100;
    if (max_delay > min_delay) {
        LOG_TRACE(Core_ARM11, "Core {} running (delayed) for {} ticks",
                  current_core_to_execute->GetID(),
                  current_core_to_execute->GetTimer().GetDowncount());
        if (running_core != current_core_to_execute) {
            running_core = current_core_to_execute;
            kernel->SetRunningCPU(running_core);
        }
        if (kernel->GetCurrentThreadManager().GetCurrentThread() == nullptr) {
            LOG_TRACE(Core_ARM11, "Core {} idling", current_core_to_execute->GetID());
            current_core_to_execute->GetTimer().Idle();
            PrepareReschedule();
        } else {
            if (tight_loop) {
                current_core_to_execute->Run();
            } else {
                current_core_to_execute->Step();
            }
        }
    } else {
        // Now all cores are at the same global time. So we will run them one after the other
        // with a max slice that is the minimum of all max slices of all cores
        // TODO: Make special check for idle since we can easily revert the time of idle cores
        s64 max_slice = Timing::MAX_SLICE_LENGTH;
        for (const auto& cpu_core : cpu_cores) {
            running_core = cpu_core.get();
            kernel->SetRunningCPU(running_core);
            cpu_core->GetTimer().Advance();
            cpu_core->PrepareReschedule();
            kernel->GetThreadManager(cpu_core->GetID()).Reschedule();
            max_slice = std::min(max_slice, cpu_core->GetTimer().GetMaxSliceLength());
        }
        for (auto& cpu_core : cpu_cores) {
            cpu_core->GetTimer().SetNextSlice(max_slice);
            auto start_ticks = cpu_core->GetTimer().GetTicks();
            LOG_TRACE(Core_ARM11, "Core {} running for {} ticks", cpu_core->GetID(),
                      cpu_core->GetTimer().GetDowncount());
            running_core = cpu_core.get();
            kernel->SetRunningCPU(running_core);
            // If we don't have a currently active thread then don't execute instructions,
            // instead advance to the next event and try to yield to the next thread
            if (kernel->GetCurrentThreadManager().GetCurrentThread() == nullptr) {
                LOG_TRACE(Core_ARM11, "Core {} idling", cpu_core->GetID());
                cpu_core->GetTimer().Idle();
                PrepareReschedule();
            } else {
                if (tight_loop) {
                    cpu_core->Run();
                } else {
                    cpu_core->Step();
                }
            }
            max_slice = cpu_core->GetTimer().GetTicks() - start_ticks;
        }
    }

    if (GDBStub::IsServerEnabled()) {
        GDBStub::SetCpuStepFlag(false);
    }

    Reschedule();

    return status;
}

bool System::SendSignal(System::Signal signal, u32 param) {
    std::scoped_lock lock{signal_mutex};
    if (current_signal != signal && current_signal != Signal::None) {
        LOG_ERROR(Core, "Unable to {} as {} is ongoing", signal, current_signal);
        return false;
    }
    current_signal = signal;
    signal_param = param;
    return true;
}

System::ResultStatus System::SingleStep() {
    return RunLoop(false);
}

System::ResultStatus System::Load(Frontend::EmuWindow& emu_window, const std::string& filepath,
                                  Frontend::EmuWindow* secondary_window) {
    FileUtil::SetCurrentRomPath(filepath);
    app_loader = Loader::GetLoader(filepath);
    if (!app_loader) {
        LOG_CRITICAL(Core, "Failed to obtain loader for {}!", filepath);
        return ResultStatus::ErrorGetLoader;
    }

    auto memory_mode = app_loader->LoadKernelMemoryMode();
    if (memory_mode.second != Loader::ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to determine system mode (Error {})!",
                     static_cast<int>(memory_mode.second));

        switch (memory_mode.second) {
        case Loader::ResultStatus::ErrorEncrypted:
            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            return ResultStatus::ErrorLoader_ErrorInvalidFormat;
        case Loader::ResultStatus::ErrorGbaTitle:
            return ResultStatus::ErrorLoader_ErrorGbaTitle;
        default:
            return ResultStatus::ErrorSystemMode;
        }
    }

    ASSERT(memory_mode.first);
    auto n3ds_hw_caps = app_loader->LoadNew3dsHwCapabilities();
    ASSERT(n3ds_hw_caps.first);
    u32 num_cores = 2;
    if (Settings::values.is_new_3ds) {
        num_cores = 4;
    }
    ResultStatus init_result{
        Init(emu_window, secondary_window, *memory_mode.first, *n3ds_hw_caps.first, num_cores)};
    if (init_result != ResultStatus::Success) {
        LOG_CRITICAL(Core, "Failed to initialize system (Error {})!",
                     static_cast<u32>(init_result));
        System::Shutdown();
        return init_result;
    }

    // Restore any parameters that should be carried through a reset.
    if (restore_deliver_arg.has_value()) {
        if (auto apt = Service::APT::GetModule(*this)) {
            apt->GetAppletManager()->SetDeliverArg(restore_deliver_arg);
        }
        restore_deliver_arg.reset();
    }
    if (restore_plugin_context.has_value()) {
        if (auto plg_ldr = Service::PLGLDR::GetService(*this)) {
            plg_ldr->SetPluginLoaderContext(restore_plugin_context.value());
        }
        restore_plugin_context.reset();
    }

    telemetry_session->AddInitialInfo(*app_loader);
    std::shared_ptr<Kernel::Process> process;
    const Loader::ResultStatus load_result{app_loader->Load(process)};
    if (Loader::ResultStatus::Success != load_result) {
        LOG_CRITICAL(Core, "Failed to load ROM (Error {})!", load_result);
        System::Shutdown();

        switch (load_result) {
        case Loader::ResultStatus::ErrorEncrypted:
            return ResultStatus::ErrorLoader_ErrorEncrypted;
        case Loader::ResultStatus::ErrorInvalidFormat:
            return ResultStatus::ErrorLoader_ErrorInvalidFormat;
        case Loader::ResultStatus::ErrorGbaTitle:
            return ResultStatus::ErrorLoader_ErrorGbaTitle;
        default:
            return ResultStatus::ErrorLoader;
        }
    }
    kernel->SetCurrentProcess(process);
    title_id = 0;
    if (app_loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        LOG_ERROR(Core, "Failed to find title id for ROM (Error {})",
                  static_cast<u32>(load_result));
    }

    cheat_engine.LoadCheatFile(title_id);
    cheat_engine.Connect();

    perf_stats = std::make_unique<PerfStats>(title_id);

    if (Settings::values.dump_textures) {
        custom_tex_manager->PrepareDumping(title_id);
    }
    if (Settings::values.custom_textures) {
        custom_tex_manager->FindCustomTextures();
    }

    status = ResultStatus::Success;
    m_emu_window = &emu_window;
    m_secondary_window = secondary_window;
    m_filepath = filepath;
    self_delete_pending = false;

    // Reset counters and set time origin to current frame
    [[maybe_unused]] const PerfStats::Results result = GetAndResetPerfStats();
    perf_stats->BeginSystemFrame();
    return status;
}

void System::PrepareReschedule() {
    running_core->PrepareReschedule();
    reschedule_pending = true;
}

PerfStats::Results System::GetAndResetPerfStats() {
    return (perf_stats && timing) ? perf_stats->GetAndResetStats(timing->GetGlobalTimeUs())
                                  : PerfStats::Results{};
}

PerfStats::Results System::GetLastPerfStats() {
    return perf_stats ? perf_stats->GetLastStats() : PerfStats::Results{};
}

void System::Reschedule() {
    if (!reschedule_pending) {
        return;
    }

    reschedule_pending = false;
    for (const auto& core : cpu_cores) {
        LOG_TRACE(Core_ARM11, "Reschedule core {}", core->GetID());
        kernel->GetThreadManager(core->GetID()).Reschedule();
    }
}

System::ResultStatus System::Init(Frontend::EmuWindow& emu_window,
                                  Frontend::EmuWindow* secondary_window,
                                  Kernel::MemoryMode memory_mode,
                                  const Kernel::New3dsHwCapabilities& n3ds_hw_caps, u32 num_cores) {
    LOG_DEBUG(HW_Memory, "initialized OK");

    memory = std::make_unique<Memory::MemorySystem>(*this);

    timing = std::make_unique<Timing>(num_cores, Settings::values.cpu_clock_percentage.GetValue(),
                                      movie.GetOverrideBaseTicks());

    kernel = std::make_unique<Kernel::KernelSystem>(
        *memory, *timing, [this] { PrepareReschedule(); }, memory_mode, num_cores, n3ds_hw_caps,
        movie.GetOverrideInitTime());

    exclusive_monitor = MakeExclusiveMonitor(*memory, num_cores);
    cpu_cores.reserve(num_cores);
    if (Settings::values.use_cpu_jit) {
#if CITRA_ARCH(x86_64) || CITRA_ARCH(arm64)
        for (u32 i = 0; i < num_cores; ++i) {
            cpu_cores.push_back(std::make_shared<ARM_Dynarmic>(
                *this, *memory, i, timing->GetTimer(i), *exclusive_monitor));
        }
#else
        for (u32 i = 0; i < num_cores; ++i) {
            cpu_cores.push_back(
                std::make_shared<ARM_DynCom>(this, *memory, USER32MODE, i, timing->GetTimer(i)));
        }
        LOG_WARNING(Core, "CPU JIT requested, but Dynarmic not available");
#endif
    } else {
        for (u32 i = 0; i < num_cores; ++i) {
            cpu_cores.push_back(
                std::make_shared<ARM_DynCom>(*this, *memory, USER32MODE, i, timing->GetTimer(i)));
        }
    }
    running_core = cpu_cores[0].get();

    kernel->SetCPUs(cpu_cores);
    kernel->SetRunningCPU(cpu_cores[0].get());

    const auto audio_emulation = Settings::values.audio_emulation.GetValue();
    if (audio_emulation == Settings::AudioEmulation::HLE) {
        dsp_core = std::make_unique<AudioCore::DspHle>(*this);
    } else {
        const bool multithread = audio_emulation == Settings::AudioEmulation::LLEMultithreaded;
        dsp_core = std::make_unique<AudioCore::DspLle>(*this, multithread);
    }

    memory->SetDSP(*dsp_core);

    dsp_core->SetSink(Settings::values.output_type.GetValue(),
                      Settings::values.output_device.GetValue());
    dsp_core->EnableStretching(Settings::values.enable_audio_stretching.GetValue());

    telemetry_session = std::make_unique<Core::TelemetrySession>();

#ifdef ENABLE_SCRIPTING
    rpc_server = std::make_unique<RPC::Server>(*this);
#endif

    service_manager = std::make_unique<Service::SM::ServiceManager>(*this);
    archive_manager = std::make_unique<Service::FS::ArchiveManager>(*this);

    HW::AES::InitKeys();
    Service::Init(*this);
    GDBStub::DeferStart();

    if (!registered_image_interface) {
        registered_image_interface = std::make_shared<Frontend::ImageInterface>();
    }

    custom_tex_manager = std::make_unique<VideoCore::CustomTexManager>(*this);

    auto gsp = service_manager->GetService<Service::GSP::GSP_GPU>("gsp::Gpu");
    gpu = std::make_unique<VideoCore::GPU>(*this, emu_window, secondary_window);
    gpu->SetInterruptHandler(
        [gsp](Service::GSP::InterruptId interrupt_id) { gsp->SignalInterrupt(interrupt_id); });

    auto plg_ldr = Service::PLGLDR::GetService(*this);
    if (plg_ldr) {
        plg_ldr->SetEnabled(Settings::values.plugin_loader_enabled.GetValue());
        plg_ldr->SetAllowGameChangeState(Settings::values.allow_plugin_loader.GetValue());
    }

    LOG_DEBUG(Core, "Initialized OK");

    is_powered_on = true;

    return ResultStatus::Success;
}

VideoCore::GPU& System::GPU() {
    return *gpu;
}

Service::SM::ServiceManager& System::ServiceManager() {
    return *service_manager;
}

const Service::SM::ServiceManager& System::ServiceManager() const {
    return *service_manager;
}

Service::FS::ArchiveManager& System::ArchiveManager() {
    return *archive_manager;
}

const Service::FS::ArchiveManager& System::ArchiveManager() const {
    return *archive_manager;
}

Kernel::KernelSystem& System::Kernel() {
    return *kernel;
}

const Kernel::KernelSystem& System::Kernel() const {
    return *kernel;
}

bool System::KernelRunning() {
    return kernel != nullptr;
}

Timing& System::CoreTiming() {
    return *timing;
}

const Timing& System::CoreTiming() const {
    return *timing;
}

Memory::MemorySystem& System::Memory() {
    return *memory;
}

const Memory::MemorySystem& System::Memory() const {
    return *memory;
}

Cheats::CheatEngine& System::CheatEngine() {
    return cheat_engine;
}

const Cheats::CheatEngine& System::CheatEngine() const {
    return cheat_engine;
}

void System::RegisterVideoDumper(std::shared_ptr<VideoDumper::Backend> dumper) {
    video_dumper = std::move(dumper);
}

VideoCore::CustomTexManager& System::CustomTexManager() {
    return *custom_tex_manager;
}

const VideoCore::CustomTexManager& System::CustomTexManager() const {
    return *custom_tex_manager;
}

Core::Movie& System::Movie() {
    return movie;
}

const Core::Movie& System::Movie() const {
    return movie;
}

void System::RegisterMiiSelector(std::shared_ptr<Frontend::MiiSelector> mii_selector) {
    registered_mii_selector = std::move(mii_selector);
}

void System::RegisterSoftwareKeyboard(std::shared_ptr<Frontend::SoftwareKeyboard> swkbd) {
    registered_swkbd = std::move(swkbd);
}

void System::RegisterImageInterface(std::shared_ptr<Frontend::ImageInterface> image_interface) {
    registered_image_interface = std::move(image_interface);
}

void System::Shutdown(bool is_deserializing) {
    // Log last frame performance stats
    const auto perf_results = GetAndResetPerfStats();
    constexpr auto performance = Common::Telemetry::FieldType::Performance;

    telemetry_session->AddField(performance, "Shutdown_EmulationSpeed",
                                perf_results.emulation_speed * 100.0);
    telemetry_session->AddField(performance, "Shutdown_Framerate", perf_results.game_fps);
    telemetry_session->AddField(performance, "Shutdown_Frametime", perf_results.frametime * 1000.0);
    telemetry_session->AddField(performance, "Mean_Frametime_MS",
                                perf_stats ? perf_stats->GetMeanFrametime() : 0);

    // Shutdown emulation session
    is_powered_on = false;

    gpu.reset();
    if (!is_deserializing) {
        GDBStub::Shutdown();
        perf_stats.reset();
        app_loader.reset();
    }
    custom_tex_manager.reset();
    telemetry_session.reset();
#ifdef ENABLE_SCRIPTING
    rpc_server.reset();
#endif
    archive_manager.reset();
    service_manager.reset();
    dsp_core.reset();
    kernel.reset();
    cpu_cores.clear();
    exclusive_monitor.reset();
    timing.reset();

    if (video_dumper && video_dumper->IsDumping()) {
        video_dumper->StopDumping();
    }

    if (auto room_member = Network::GetRoomMember().lock()) {
        Network::GameInfo game_info{};
        room_member->SendGameInfo(game_info);
    }

    memory.reset();

    if (self_delete_pending)
        FileUtil::Delete(m_filepath);
    self_delete_pending = false;

    LOG_DEBUG(Core, "Shutdown OK");
}

void System::Reset() {
    // This is NOT a proper reset, but a temporary workaround by shutting down the system and
    // reloading.
    // TODO: Properly implement the reset

    // Save the APT deliver arg and plugin loader context across resets.
    // This is needed as we don't currently support proper app jumping.
    if (auto apt = Service::APT::GetModule(*this)) {
        restore_deliver_arg = apt->GetAppletManager()->ReceiveDeliverArg();
    }
    if (auto plg_ldr = Service::PLGLDR::GetService(*this)) {
        restore_plugin_context = plg_ldr->GetPluginLoaderContext();
    }

    Shutdown();

    if (!m_chainloadpath.empty()) {
        m_filepath = m_chainloadpath;
        m_chainloadpath.clear();
    }

    // Reload the system with the same setting
    [[maybe_unused]] const System::ResultStatus result =
        Load(*m_emu_window, m_filepath, m_secondary_window);
}

void System::ApplySettings() {
    GDBStub::SetServerPort(Settings::values.gdbstub_port.GetValue());
    GDBStub::ToggleServer(Settings::values.use_gdbstub.GetValue());

    if (gpu) {
#ifndef ANDROID
        gpu->Renderer().UpdateCurrentFramebufferLayout();
#endif
        auto& settings = gpu->Renderer().Settings();
        settings.bg_color_update_requested = true;
        settings.shader_update_requested = true;
    }

    if (IsPoweredOn()) {
        CoreTiming().UpdateClockSpeed(Settings::values.cpu_clock_percentage.GetValue());
        dsp_core->SetSink(Settings::values.output_type.GetValue(),
                          Settings::values.output_device.GetValue());
        dsp_core->EnableStretching(Settings::values.enable_audio_stretching.GetValue());

        auto hid = Service::HID::GetModule(*this);
        if (hid) {
            hid->ReloadInputDevices();
        }

        auto apt = Service::APT::GetModule(*this);
        if (apt) {
            apt->GetAppletManager()->ReloadInputDevices();
        }

        auto ir_user = service_manager->GetService<Service::IR::IR_USER>("ir:USER");
        if (ir_user)
            ir_user->ReloadInputDevices();
        auto ir_rst = service_manager->GetService<Service::IR::IR_RST>("ir:rst");
        if (ir_rst)
            ir_rst->ReloadInputDevices();

        auto cam = Service::CAM::GetModule(*this);
        if (cam) {
            cam->ReloadCameraDevices();
        }

        Service::MIC::ReloadMic(*this);
    }

    auto plg_ldr = Service::PLGLDR::GetService(*this);
    if (plg_ldr) {
        plg_ldr->SetEnabled(Settings::values.plugin_loader_enabled.GetValue());
        plg_ldr->SetAllowGameChangeState(Settings::values.allow_plugin_loader.GetValue());
    }
}

template <class Archive>
void System::serialize(Archive& ar, const unsigned int file_version) {

    u32 num_cores;
    if (Archive::is_saving::value) {
        num_cores = this->GetNumCores();
    }
    ar& num_cores;

    if (Archive::is_loading::value) {
        // When loading, we want to make sure any lingering state gets cleared out before we begin.
        // Shutdown, but persist a few things between loads...
        Shutdown(true);

        // Re-initialize everything like it was before
        auto memory_mode = this->app_loader->LoadKernelMemoryMode();
        auto n3ds_hw_caps = this->app_loader->LoadNew3dsHwCapabilities();
        [[maybe_unused]] const System::ResultStatus result = Init(
            *m_emu_window, m_secondary_window, *memory_mode.first, *n3ds_hw_caps.first, num_cores);
    }

    // Flush on save, don't flush on load
    const bool should_flush = !Archive::is_loading::value;
    gpu->ClearAll(should_flush);
    ar&* timing.get();
    for (u32 i = 0; i < num_cores; i++) {
        ar&* cpu_cores[i].get();
    }
    ar&* service_manager.get();
    ar&* archive_manager.get();

    // NOTE: DSP doesn't like being destroyed and recreated. So instead we do an inline
    // serialization; this means that the DSP Settings need to match for loading to work.
    auto dsp_hle = dynamic_cast<AudioCore::DspHle*>(dsp_core.get());
    if (dsp_hle) {
        ar&* dsp_hle;
    } else {
        throw std::runtime_error("LLE audio not supported for save states");
    }

    ar&* memory.get();
    ar&* kernel.get();
    ar&* gpu.get();
    ar& movie;

    // This needs to be set from somewhere - might as well be here!
    if (Archive::is_loading::value) {
        timing->UnlockEventQueue();
        memory->SetDSP(*dsp_core);
        cheat_engine.Connect();
        gpu->Sync();

        // Re-register gpu callback, because gsp service changed after service_manager got
        // serialized
        auto gsp = service_manager->GetService<Service::GSP::GSP_GPU>("gsp::Gpu");
        gpu->SetInterruptHandler(
            [gsp](Service::GSP::InterruptId interrupt_id) { gsp->SignalInterrupt(interrupt_id); });
    }
}

SERIALIZE_IMPL(System)

} // namespace Core
