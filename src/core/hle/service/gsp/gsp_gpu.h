// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <boost/optional/optional.hpp>
#include <boost/serialization/export.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/gsp/gsp_command.h"
#include "core/hle/service/gsp/gsp_interrupt.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class HLERequestContext;
class Process;
class SharedMemory;
} // namespace Kernel

namespace Service::GSP {

struct FrameBufferInfo {
    u32 active_fb; // 0 = first, 1 = second
    u32 address_left;
    u32 address_right;
    u32 stride;   // maps to 0x1EF00X90 ?
    u32 format;   // maps to 0x1EF00X70 ?
    u32 shown_fb; // maps to 0x1EF00X78 ?
    u32 unknown;
};
static_assert(sizeof(FrameBufferInfo) == 0x1c, "Struct has incorrect size");

struct FrameBufferUpdate {
    BitField<0, 1, u8> index;    // Index used for GSP::SetBufferSwap
    BitField<0, 1, u8> is_dirty; // true if GSP should update GPU framebuffer registers
    u16 pad1;

    FrameBufferInfo framebuffer_info[2];

    u32 pad2;
};
static_assert(sizeof(FrameBufferUpdate) == 0x40, "Struct has incorrect size");
static_assert(offsetof(FrameBufferUpdate, framebuffer_info[1]) == 0x20,
              "FrameBufferInfo element has incorrect alignment");

constexpr u32 FRAMEBUFFER_WIDTH = 240;
constexpr u32 FRAMEBUFFER_WIDTH_POW2 = 256;
constexpr u32 TOP_FRAMEBUFFER_HEIGHT = 400;
constexpr u32 BOTTOM_FRAMEBUFFER_HEIGHT = 320;
constexpr u32 FRAMEBUFFER_HEIGHT_POW2 = 512;

// These are the VRAM addresses that GSP copies framebuffers to in SaveVramSysArea.
constexpr VAddr FRAMEBUFFER_SAVE_AREA_TOP_LEFT = Memory::VRAM_VADDR + 0x273000;
constexpr VAddr FRAMEBUFFER_SAVE_AREA_TOP_RIGHT = Memory::VRAM_VADDR + 0x2B9800;
constexpr VAddr FRAMEBUFFER_SAVE_AREA_BOTTOM = Memory::VRAM_VADDR + 0x4C7800;

class GSP_GPU;

class SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
public:
    SessionData() = default;
    SessionData(GSP_GPU* gsp);
    ~SessionData();

    GSP_GPU* gsp;

    /// Event triggered when GSP interrupt has been signalled
    std::shared_ptr<Kernel::Event> interrupt_event;
    /// Thread index into interrupt relay queue
    u32 thread_id;
    /// Whether RegisterInterruptRelayQueue was called for this session
    bool registered = false;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

class GSP_GPU final : public ServiceFramework<GSP_GPU, SessionData> {
public:
    explicit GSP_GPU(Core::System& system);
    ~GSP_GPU() = default;

    void ClientDisconnected(std::shared_ptr<Kernel::ServerSession> server_session) override;

    /**
     * Signals that the specified interrupt type has occurred to userland code
     * @param interrupt_id ID of interrupt that is being signalled
     */
    void SignalInterrupt(InterruptId interrupt_id);

    /**
     * Retrieves the framebuffer info stored in the GSP shared memory for the
     * specified screen index and thread id.
     * @param thread_id GSP thread id of the process that accesses the structure that we are
     * requesting.
     * @param screen_index Index of the screen we are requesting (Top = 0, Bottom = 1).
     * @returns FramebufferUpdate Information about the specified framebuffer.
     */
    FrameBufferUpdate* GetFrameBufferInfo(u32 thread_id, u32 screen_index);

    /// Gets a pointer to a thread command buffer in GSP shared memory
    CommandBuffer* GetCommandBuffer(u32 thread_id);

    /// Gets a pointer to the interrupt relay queue for a given thread index
    InterruptRelayQueue* GetInterruptRelayQueue(u32 thread_id);

    /**
     * Retreives the ID of the thread with GPU rights.
     */
    u32 GetActiveThreadId() {
        return active_thread_id;
    }

private:
    /**
     * Signals that the specified interrupt type has occurred to userland code for the specified GSP
     * thread id.
     * @param interrupt_id ID of interrupt that is being signalled.
     * @param thread_id GSP thread that will receive the interrupt.
     */
    void SignalInterruptForThread(InterruptId interrupt_id, u32 thread_id);

    /**
     * GSP_GPU::WriteHWRegs service function
     *
     * Writes sequential GSP GPU hardware registers
     *
     *  Inputs:
     *      1 : address of first GPU register
     *      2 : number of registers to write sequentially
     *      4 : pointer to source data array
     */
    void WriteHWRegs(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::WriteHWRegsWithMask service function
     *
     * Updates sequential GSP GPU hardware registers using masks
     *
     *  Inputs:
     *      1 : address of first GPU register
     *      2 : number of registers to update sequentially
     *      4 : pointer to source data array
     *      6 : pointer to mask array
     */
    void WriteHWRegsWithMask(Kernel::HLERequestContext& ctx);

    /// Read a GSP GPU hardware register
    void ReadHWRegs(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::SetBufferSwap service function
     *
     * Updates GPU display framebuffer configuration using the specified parameters.
     *
     *  Inputs:
     *      1 : Screen ID (0 = top screen, 1 = bottom screen)
     *      2-7 : FrameBufferInfo structure
     *  Outputs:
     *      1: Result code
     */
    void SetBufferSwap(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::FlushDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void FlushDataCache(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::InvalidateDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::SetLcdForceBlack service function
     *
     * Enable or disable REG_LCDCOLORFILL with the color black.
     *
     *  Inputs:
     *      1: Black color fill flag (0 = don't fill, !0 = fill)
     *  Outputs:
     *      1: Result code
     */
    void SetLcdForceBlack(Kernel::HLERequestContext& ctx);

    /// This triggers handling of the GX command written to the command buffer in shared memory.
    void TriggerCmdReqQueue(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::SetAxiConfigQoSMode service function
     *  Inputs:
     *      1 : Mode, unused in emulator
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetAxiConfigQoSMode(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::RegisterInterruptRelayQueue service function
     *  Inputs:
     *      1 : "Flags" field, purpose is unknown
     *      3 : Handle to GSP synchronization event
     *  Outputs:
     *      1 : Result of function, 0x2A07 on success, otherwise error code
     *      2 : Thread index into GSP command buffer
     *      4 : Handle to GSP shared memory
     */
    void RegisterInterruptRelayQueue(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::UnregisterInterruptRelayQueue service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void UnregisterInterruptRelayQueue(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::TryAcquireRight service function
     *  Inputs:
     *      0 : Header code [0x00150002]
     *      1 : Handle translate header (0x0)
     *      2 : Process handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void TryAcquireRight(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::AcquireRight service function
     *  Inputs:
     *      0 : Header code [0x00160042]
     *      1 : Flags
     *      2 : Handle translate header (0x0)
     *      3 : Process handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AcquireRight(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::ReleaseRight service function
     *  Outputs:
     *      1: Result code
     */
    void ReleaseRight(Kernel::HLERequestContext& ctx);

    /**
     * Releases rights to the GPU.
     * Will fail if the session_data doesn't have the GPU right
     */
    void ReleaseRight(const SessionData* session_data);

    /**
     * GSP_GPU::ImportDisplayCaptureInfo service function
     *
     * Returns information about the current framebuffer state
     *
     *  Inputs:
     *      0: Header 0x00180000
     *  Outputs:
     *      0: Header Code[0x00180240]
     *      1: Result code
     *      2: Left framebuffer virtual address for the main screen
     *      3: Right framebuffer virtual address for the main screen
     *      4: Main screen framebuffer format
     *      5: Main screen framebuffer width
     *      6: Left framebuffer virtual address for the bottom screen
     *      7: Right framebuffer virtual address for the bottom screen
     *      8: Bottom screen framebuffer format
     *      9: Bottom screen framebuffer width
     */
    void ImportDisplayCaptureInfo(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::SaveVramSysArea service function
     *
     * Returns information about the current framebuffer state
     *
     *  Inputs:
     *      0: Header 0x00190000
     *  Outputs:
     *      0: Header Code[0x00190040]
     *      1: Result code
     */
    void SaveVramSysArea(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::RestoreVramSysArea service function
     *
     * Returns information about the current framebuffer state
     *
     *  Inputs:
     *      0: Header 0x001A0000
     *  Outputs:
     *      0: Header Code[0x001A0040]
     *      1: Result code
     */
    void RestoreVramSysArea(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::StoreDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header code [0x001F0082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void StoreDataCache(Kernel::HLERequestContext& ctx);

    /// Force the 3D LED State (0 = On, Non-Zero = Off)
    void SetLedForceOff(Kernel::HLERequestContext& ctx);

    /**
     * GSP_GPU::SetInternalPriorities service function
     *  Inputs:
     *      0 : Header code [0x001E0080]
     *      1 : Session thread priority
     *      2 : Session thread priority with rights
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetInternalPriorities(Kernel::HLERequestContext& ctx);

    /// Returns the session data for the specified registered thread id, or nullptr if not found.
    SessionData* FindRegisteredThreadData(u32 thread_id);

    u32 GetUnusedThreadId() const;

    std::unique_ptr<Kernel::SessionRequestHandler::SessionDataBase> MakeSessionData() override;

    Result AcquireGpuRight(const Kernel::HLERequestContext& ctx,
                           const std::shared_ptr<Kernel::Process>& process, u32 flag,
                           bool blocking);

    Core::System& system;

    /// GSP shared memory
    std::shared_ptr<Kernel::SharedMemory> shared_memory;

    /// Thread id that currently has GPU rights or std::numeric_limits<u32>::max() if none.
    u32 active_thread_id = std::numeric_limits<u32>::max();

    bool first_initialization = true;

    /// VRAM copy saved using SaveVramSysArea.
    boost::optional<std::vector<u8>> saved_vram;

    /// Maximum number of threads that can be registered at the same time in the GSP module.
    static constexpr u32 MaxGSPThreads = 4;

    /// Thread ids currently in use by the sessions connected to the GSPGPU service.
    std::array<bool, MaxGSPThreads> used_thread_ids{};

    friend class SessionData;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Service::GSP

BOOST_CLASS_EXPORT_KEY(Service::GSP::SessionData)
BOOST_CLASS_EXPORT_KEY(Service::GSP::GSP_GPU)
SERVICE_CONSTRUCT(Service::GSP::GSP_GPU)
