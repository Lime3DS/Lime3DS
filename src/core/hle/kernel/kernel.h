// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/result.h"
#include "core/memory.h"

namespace ConfigMem {
class Handler;
}

namespace SharedPage {
class Handler;
}

namespace Memory {
class MemorySystem;
}

namespace Core {
class ARM_Interface;
class Timing;
} // namespace Core

namespace IPCDebugger {
class Recorder;
}

namespace Kernel {

class AddressArbiter;
class Event;
class Mutex;
class CodeSet;
class Process;
class Thread;
class Semaphore;
class Timer;
class ClientPort;
class ServerPort;
class ClientSession;
class ServerSession;
class ResourceLimitList;
class SharedMemory;
class ThreadManager;
class TimerManager;
class VMManager;
struct AddressMapping;

enum class ResetType {
    OneShot,
    Sticky,
    Pulse,
};

/// Permissions for mapped shared memory blocks
enum class MemoryPermission : u32 {
    None = 0,
    Read = (1u << 0),
    Write = (1u << 1),
    ReadWrite = (Read | Write),
    Execute = (1u << 2),
    ReadExecute = (Read | Execute),
    WriteExecute = (Write | Execute),
    ReadWriteExecute = (Read | Write | Execute),
    DontCare = (1u << 28)
};

enum class MemoryRegion : u16 {
    APPLICATION = 1,
    SYSTEM = 2,
    BASE = 3,
};

union CoreVersion {
    CoreVersion(u32 version) : raw(version) {}
    CoreVersion(u32 major_ver, u32 minor_ver, u32 revision_ver) {
        revision.Assign(revision_ver);
        minor.Assign(minor_ver);
        major.Assign(major_ver);
    }

    u32 raw = 0;
    BitField<8, 8, u32> revision;
    BitField<16, 8, u32> minor;
    BitField<24, 8, u32> major;
};

/// Common memory memory modes.
enum class MemoryMode : u8 {
    Prod = 0, ///< 64MB app memory
    Dev1 = 2, ///< 96MB app memory
    Dev2 = 3, ///< 80MB app memory
    Dev3 = 4, ///< 72MB app memory
    Dev4 = 5, ///< 32MB app memory
};

/// New 3DS memory modes.
enum class New3dsMemoryMode : u8 {
    Legacy = 0,  ///< Use Old 3DS system mode.
    NewProd = 1, ///< 124MB app memory
    NewDev1 = 2, ///< 178MB app memory
    NewDev2 = 3, ///< 124MB app memory
};

/// Structure containing N3DS hardware capability flags.
struct New3dsHwCapabilities {
    bool enable_l2_cache;         ///< Whether extra L2 cache should be enabled.
    bool enable_804MHz_cpu;       ///< Whether the CPU should run at 804MHz.
    New3dsMemoryMode memory_mode; ///< The New 3DS memory mode.

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

class KernelSystem {
public:
    explicit KernelSystem(Memory::MemorySystem& memory, Core::Timing& timing,
                          std::function<void()> prepare_reschedule_callback, MemoryMode memory_mode,
                          u32 num_cores, const New3dsHwCapabilities& n3ds_hw_caps,
                          u64 override_init_time = 0);
    ~KernelSystem();

    using PortPair = std::pair<std::shared_ptr<ServerPort>, std::shared_ptr<ClientPort>>;
    using SessionPair = std::pair<std::shared_ptr<ServerSession>, std::shared_ptr<ClientSession>>;

    /**
     * Creates an address arbiter.
     *
     * @param name Optional name used for debugging.
     * @returns The created AddressArbiter.
     */
    std::shared_ptr<AddressArbiter> CreateAddressArbiter(std::string name = "Unknown");

    /**
     * Creates an event
     * @param reset_type ResetType describing how to create event
     * @param name Optional name of event
     */
    std::shared_ptr<Event> CreateEvent(ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates a mutex.
     * @param initial_locked Specifies if the mutex should be locked initially
     * @param name Optional name of mutex
     * @return Pointer to new Mutex object
     */
    std::shared_ptr<Mutex> CreateMutex(bool initial_locked, std::string name = "Unknown");

    std::shared_ptr<CodeSet> CreateCodeSet(std::string name, u64 program_id);

    std::shared_ptr<Process> CreateProcess(std::shared_ptr<CodeSet> code_set);

    /**
     * Terminates a process, killing its threads and removing it from the process list.
     * @param process Process to terminate.
     */
    void TerminateProcess(std::shared_ptr<Process> process);

    /**
     * Creates and returns a new thread. The new thread is immediately scheduled
     * @param name The friendly name desired for the thread
     * @param entry_point The address at which the thread should start execution
     * @param priority The thread's priority
     * @param arg User data to pass to the thread
     * @param processor_id The ID(s) of the processors on which the thread is desired to be run
     * @param stack_top The address of the thread's stack top
     * @param owner_process The parent process for the thread
     * @param make_ready If the thread should be put in the ready queue
     * @return A shared pointer to the newly created thread
     */
    ResultVal<std::shared_ptr<Thread>> CreateThread(std::string name, VAddr entry_point,
                                                    u32 priority, u32 arg, s32 processor_id,
                                                    VAddr stack_top,
                                                    std::shared_ptr<Process> owner_process,
                                                    bool make_ready = true);

    /**
     * Creates a semaphore.
     * @param initial_count Number of slots reserved for other threads
     * @param max_count Maximum number of slots the semaphore can have
     * @param name Optional name of semaphore
     * @return The created semaphore
     */
    ResultVal<std::shared_ptr<Semaphore>> CreateSemaphore(s32 initial_count, s32 max_count,
                                                          std::string name = "Unknown");

    /**
     * Creates a timer
     * @param reset_type ResetType describing how to create the timer
     * @param name Optional name of timer
     * @return The created Timer
     */
    std::shared_ptr<Timer> CreateTimer(ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates a pair of ServerPort and an associated ClientPort.
     *
     * @param max_sessions Maximum number of sessions to the port
     * @param name Optional name of the ports
     * @return The created port tuple
     */
    PortPair CreatePortPair(u32 max_sessions, std::string name = "UnknownPort");

    /**
     * Creates a pair of ServerSession and an associated ClientSession.
     * @param name        Optional name of the ports.
     * @param client_port Optional The ClientPort that spawned this session.
     * @return The created session tuple
     */
    SessionPair CreateSessionPair(const std::string& name = "Unknown",
                                  std::shared_ptr<ClientPort> client_port = nullptr);

    ResourceLimitList& ResourceLimit();
    const ResourceLimitList& ResourceLimit() const;

    /**
     * Creates a shared memory object.
     * @param owner_process Process that created this shared memory object.
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the
     * block.
     * @param address The address from which to map the Shared Memory.
     * @param region If the address is 0, the shared memory will be allocated in this region of the
     * linear heap.
     * @param name Optional object name, used for debugging purposes.
     */
    ResultVal<std::shared_ptr<SharedMemory>> CreateSharedMemory(
        std::shared_ptr<Process> owner_process, u32 size, MemoryPermission permissions,
        MemoryPermission other_permissions, VAddr address = 0,
        MemoryRegion region = MemoryRegion::BASE, std::string name = "Unknown");

    /**
     * Creates a shared memory object from a block of memory managed by an HLE applet.
     * @param offset The offset into the heap block that the SharedMemory will map.
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the
     * block.
     * @param name Optional object name, used for debugging purposes.
     */
    std::shared_ptr<SharedMemory> CreateSharedMemoryForApplet(u32 offset, u32 size,
                                                              MemoryPermission permissions,
                                                              MemoryPermission other_permissions,
                                                              std::string name = "Unknown Applet");

    u32 GenerateObjectID();

    /// Retrieves a process from the current list of processes.
    std::shared_ptr<Process> GetProcessById(u32 process_id) const;

    std::span<const std::shared_ptr<Process>> GetProcessList() const {
        return process_list;
    }

    std::shared_ptr<Process> GetCurrentProcess() const;
    void SetCurrentProcess(std::shared_ptr<Process> process);
    void SetCurrentProcessForCPU(std::shared_ptr<Process> process, u32 core_id);

    void SetCurrentMemoryPageTable(std::shared_ptr<Memory::PageTable> page_table);

    void SetCPUs(std::vector<std::shared_ptr<Core::ARM_Interface>> cpu);

    void SetRunningCPU(Core::ARM_Interface* cpu);

    ThreadManager& GetThreadManager(u32 core_id);
    const ThreadManager& GetThreadManager(u32 core_id) const;

    ThreadManager& GetCurrentThreadManager();
    const ThreadManager& GetCurrentThreadManager() const;

    TimerManager& GetTimerManager();
    const TimerManager& GetTimerManager() const;

    void MapSharedPages(VMManager& address_space);

    SharedPage::Handler& GetSharedPageHandler();
    const SharedPage::Handler& GetSharedPageHandler() const;

    ConfigMem::Handler& GetConfigMemHandler();

    IPCDebugger::Recorder& GetIPCRecorder();
    const IPCDebugger::Recorder& GetIPCRecorder() const;

    std::shared_ptr<MemoryRegionInfo> GetMemoryRegion(MemoryRegion region);

    void HandleSpecialMapping(VMManager& address_space, const AddressMapping& mapping);

    std::array<std::shared_ptr<MemoryRegionInfo>, 3> memory_regions{};

    /// Adds a port to the named port table
    void AddNamedPort(std::string name, std::shared_ptr<ClientPort> port);

    void PrepareReschedule() {
        prepare_reschedule_callback();
    }

    u32 NewThreadId();

    void ResetThreadIDs();

    MemoryMode GetMemoryMode() const {
        return memory_mode;
    }

    const New3dsHwCapabilities& GetNew3dsHwCapabilities() const {
        return n3ds_hw_caps;
    }

    std::recursive_mutex& GetHLELock() {
        return hle_lock;
    }

    /// Map of named ports managed by the kernel, which can be retrieved using the ConnectToPort
    std::unordered_map<std::string, std::shared_ptr<ClientPort>> named_ports;

    Core::ARM_Interface* current_cpu = nullptr;

    Memory::MemorySystem& memory;

    Core::Timing& timing;

    /// Sleep main thread of the first ever launched non-sysmodule process.
    void SetAppMainThreadExtendedSleep(bool requires_sleep) {
        main_thread_extended_sleep = requires_sleep;
    }

    bool GetAppMainThreadExtendedSleep() const {
        return main_thread_extended_sleep;
    }

private:
    void MemoryInit(MemoryMode memory_mode, New3dsMemoryMode n3ds_mode, u64 override_init_time);

    std::function<void()> prepare_reschedule_callback;

    std::unique_ptr<ResourceLimitList> resource_limits;
    std::atomic<u32> next_object_id{0};

    // Note: keep the member order below in order to perform correct destruction.
    // Thread manager is destructed before process list in order to Stop threads and clear thread
    // info from their parent processes first. Timer manager is destructed after process list
    // because timers are destructed along with process list and they need to clear info from the
    // timer manager.
    // TODO (wwylele): refactor the cleanup sequence to make this less complicated and sensitive.

    std::unique_ptr<TimerManager> timer_manager;

    // TODO(Subv): Start the process ids from 10 for now, as lower PIDs are
    // reserved for low-level services
    u32 next_process_id = 10;

    // Lists all processes that exist in the current session.
    std::vector<std::shared_ptr<Process>> process_list;

    std::shared_ptr<Process> current_process;
    std::vector<std::shared_ptr<Process>> stored_processes;

    std::vector<std::unique_ptr<ThreadManager>> thread_managers;

    std::shared_ptr<ConfigMem::Handler> config_mem_handler;
    std::shared_ptr<SharedPage::Handler> shared_page_handler;

    std::unique_ptr<IPCDebugger::Recorder> ipc_recorder;

    u32 next_thread_id;

    MemoryMode memory_mode;
    New3dsHwCapabilities n3ds_hw_caps;

    /*
     * Synchronizes access to the internal HLE kernel structures, it is acquired when a guest
     * application thread performs a syscall. It should be acquired by any host threads that read or
     * modify the HLE kernel state. Note: Any operation that directly or indirectly reads from or
     * writes to the emulated memory is not protected by this mutex, and should be avoided in any
     * threads other than the CPU thread.
     */
    std::recursive_mutex hle_lock;

    /*
     * Flags non system module main threads to wait a bit before running. On real hardware,
     * system modules have plenty of time to load before the game is loaded, but on lime3ds they
     * start at the same time as the game. The artificial wait gives system modules some time
     * to load and setup themselves before the game starts.
     */
    bool main_thread_extended_sleep = false;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::New3dsHwCapabilities)
