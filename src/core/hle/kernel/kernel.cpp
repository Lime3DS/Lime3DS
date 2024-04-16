// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include "common/archives.h"
#include "common/serialization/atomic.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/config_mem.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

SERIALIZE_EXPORT_IMPL(Kernel::New3dsHwCapabilities)

namespace Kernel {

/// Initialize the kernel
KernelSystem::KernelSystem(Memory::MemorySystem& memory, Core::Timing& timing,
                           std::function<void()> prepare_reschedule_callback,
                           MemoryMode memory_mode, u32 num_cores,
                           const New3dsHwCapabilities& n3ds_hw_caps, u64 override_init_time)
    : memory(memory), timing(timing),
      prepare_reschedule_callback(std::move(prepare_reschedule_callback)), memory_mode(memory_mode),
      n3ds_hw_caps(n3ds_hw_caps) {
    std::generate(memory_regions.begin(), memory_regions.end(),
                  [] { return std::make_shared<MemoryRegionInfo>(); });
    MemoryInit(memory_mode, n3ds_hw_caps.memory_mode, override_init_time);

    resource_limits = std::make_unique<ResourceLimitList>(*this);
    for (u32 core_id = 0; core_id < num_cores; ++core_id) {
        thread_managers.push_back(std::make_unique<ThreadManager>(*this, core_id));
    }
    timer_manager = std::make_unique<TimerManager>(timing);
    ipc_recorder = std::make_unique<IPCDebugger::Recorder>();
    stored_processes.assign(num_cores, nullptr);

    next_thread_id = 1;
}

/// Shutdown the kernel
KernelSystem::~KernelSystem() {
    ResetThreadIDs();
};

ResourceLimitList& KernelSystem::ResourceLimit() {
    return *resource_limits;
}

const ResourceLimitList& KernelSystem::ResourceLimit() const {
    return *resource_limits;
}

u32 KernelSystem::GenerateObjectID() {
    return next_object_id++;
}

std::shared_ptr<Process> KernelSystem::GetCurrentProcess() const {
    return current_process;
}

void KernelSystem::SetCurrentProcess(std::shared_ptr<Process> process) {
    current_process = process;
    SetCurrentMemoryPageTable(process->vm_manager.page_table);
}

void KernelSystem::SetCurrentProcessForCPU(std::shared_ptr<Process> process, u32 core_id) {
    if (current_cpu->GetID() == core_id) {
        current_process = process;
        SetCurrentMemoryPageTable(process->vm_manager.page_table);
    } else {
        stored_processes[core_id] = process;
        thread_managers[core_id]->cpu->SetPageTable(process->vm_manager.page_table);
    }
}

void KernelSystem::SetCurrentMemoryPageTable(std::shared_ptr<Memory::PageTable> page_table) {
    memory.SetCurrentPageTable(page_table);
    if (current_cpu != nullptr) {
        current_cpu->SetPageTable(page_table);
    }
}

void KernelSystem::SetCPUs(std::vector<std::shared_ptr<Core::ARM_Interface>> cpus) {
    ASSERT(cpus.size() == thread_managers.size());
    for (u32 i = 0; i < cpus.size(); i++) {
        thread_managers[i]->SetCPU(*cpus[i]);
    }
}

void KernelSystem::SetRunningCPU(Core::ARM_Interface* cpu) {
    if (current_process) {
        stored_processes[current_cpu->GetID()] = current_process;
    }
    current_cpu = cpu;
    timing.SetCurrentTimer(cpu->GetID());
    if (stored_processes[current_cpu->GetID()]) {
        SetCurrentProcess(stored_processes[current_cpu->GetID()]);
    }
}

ThreadManager& KernelSystem::GetThreadManager(u32 core_id) {
    return *thread_managers[core_id];
}

const ThreadManager& KernelSystem::GetThreadManager(u32 core_id) const {
    return *thread_managers[core_id];
}

ThreadManager& KernelSystem::GetCurrentThreadManager() {
    return *thread_managers[current_cpu->GetID()];
}

const ThreadManager& KernelSystem::GetCurrentThreadManager() const {
    return *thread_managers[current_cpu->GetID()];
}

TimerManager& KernelSystem::GetTimerManager() {
    return *timer_manager;
}

const TimerManager& KernelSystem::GetTimerManager() const {
    return *timer_manager;
}

SharedPage::Handler& KernelSystem::GetSharedPageHandler() {
    return *shared_page_handler;
}

const SharedPage::Handler& KernelSystem::GetSharedPageHandler() const {
    return *shared_page_handler;
}

ConfigMem::Handler& KernelSystem::GetConfigMemHandler() {
    return *config_mem_handler;
}

IPCDebugger::Recorder& KernelSystem::GetIPCRecorder() {
    return *ipc_recorder;
}

const IPCDebugger::Recorder& KernelSystem::GetIPCRecorder() const {
    return *ipc_recorder;
}

void KernelSystem::AddNamedPort(std::string name, std::shared_ptr<ClientPort> port) {
    named_ports.emplace(std::move(name), std::move(port));
}

u32 KernelSystem::NewThreadId() {
    return next_thread_id++;
}

void KernelSystem::ResetThreadIDs() {
    next_thread_id = 0;
}

template <class Archive>
void KernelSystem::serialize(Archive& ar, const unsigned int) {
    ar & memory_regions;
    ar & named_ports;
    // current_cpu set externally
    // NB: subsystem references and prepare_reschedule_callback are constant
    ar&* resource_limits.get();
    ar & next_object_id;
    ar&* timer_manager.get();
    ar & next_process_id;
    ar & process_list;
    ar & current_process;
    // NB: core count checked in 'core'
    for (auto& thread_manager : thread_managers) {
        ar&* thread_manager.get();
    }
    ar & config_mem_handler;
    ar & shared_page_handler;
    ar & stored_processes;
    ar & next_thread_id;
    ar & memory_mode;
    ar & n3ds_hw_caps;
    ar & main_thread_extended_sleep;
    // Deliberately don't include debugger info to allow debugging through loads

    if (Archive::is_loading::value) {
        for (auto& memory_region : memory_regions) {
            memory_region->Unlock();
        }
        for (auto& process : process_list) {
            process->vm_manager.Unlock();
        }
    }
}
SERIALIZE_IMPL(KernelSystem)

template <class Archive>
void New3dsHwCapabilities::serialize(Archive& ar, const unsigned int) {
    ar & enable_l2_cache;
    ar & enable_804MHz_cpu;
    ar & memory_mode;
}
SERIALIZE_IMPL(New3dsHwCapabilities)

} // namespace Kernel
