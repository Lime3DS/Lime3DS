// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "audio_core/dsp_interface.h"
#include "core/hle/kernel/event.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::DSP {

/// There are three types of interrupts
enum class InterruptType : u32 { Zero = 0, One = 1, Pipe = 2, Count };

class DSP_DSP final : public ServiceFramework<DSP_DSP> {
public:
    explicit DSP_DSP(Core::System& system);
    ~DSP_DSP();

    /// Actual service implementation only has 6 'slots' for interrupts.
    static constexpr std::size_t max_number_of_interrupt_events = 6;

    /// Signal interrupt on pipe
    void SignalInterrupt(InterruptType type, AudioCore::DspPipe pipe);

private:
    /**
     * DSP_DSP::RecvData service function
     *      This function reads a value out of a DSP register.
     *  Inputs:
     *      0 : Header Code[0x00010040]
     *      1 : Register Number
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u16, Value in the register
     *  Notes:
     *      This function has only been observed being called with a register number of 0.
     */
    void RecvData(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::RecvDataIsReady service function
     *      This function checks whether a DSP register is ready to be read.
     *  Inputs:
     *      0 : Header Code[0x00020040]
     *      1 : Register Number
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Reply Register Update Flag (0 = not ready, 1 = ready)
     *  Note:
     *      This function has only been observed being called with a register number of 0.
     */
    void RecvDataIsReady(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::SetSemaphore service function
     *  Inputs:
     *      0 : Header Code[0x00070040]
     *      1 : u16, Semaphore value
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetSemaphore(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::ConvertProcessAddressFromDspDram service function
     *  Inputs:
     *      0 : Header Code[0x000C0040]
     *      1 : Address
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Address. (inaddr << 1) + 0x1FF40000 (where 0x1FF00000 is the DSP RAM address)
     */
    void ConvertProcessAddressFromDspDram(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::WriteProcessPipe service function
     *  Inputs:
     *      0 : Header Code[0x000D0082]
     *      1 : Channel (0 - 7 0:Debug from DSP 1:P-DMA 2:audio 3:binary 4-7: free ?)
     *      2 : Size
     *      3 : (size << 14) | 0x402
     *      4 : Buffer
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void WriteProcessPipe(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::ReadPipe service function
     *  Inputs:
     *      0 : Header Code[0x000E00C0]
     *      1 : Channel (0 - 7 0:Debug from DSP 1:P-DMA 2:audio 3:binary 4-7: free ?)
     *      2 : Peer (0 = from DSP, 1 = from ARM)
     *      3 : u16, Size
     *      0x41 : Virtual address of memory buffer to write pipe contents to
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ReadPipe(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::GetPipeReadableSize service function
     *  Inputs:
     *      0 : Header Code[0x000F0080]
     *      1 : Channel (0 - 7 0:Debug from DSP 1:P-DMA 2:audio 3:binary 4-7: free ?)
     *      2 : Peer (0 = from DSP, 1 = from ARM)
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u16, Readable size
     */
    void GetPipeReadableSize(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::ReadPipeIfPossible service function
     *      A pipe is a means of communication between the ARM11 and DSP that occurs on
     *      hardware by writing to/reading from the DSP registers at 0x10203000.
     *      Pipes are used for initialisation. See also DspInterface::PipeRead.
     *  Inputs:
     *      0 : Header Code[0x001000C0]
     *      1 : Channel (0 - 7 0:Debug from DSP 1:P-DMA 2:audio 3:binary 4-7: free ?)
     *      2 : Peer (0 = from DSP, 1 = from ARM)
     *      3 : u16, Size
     *      0x41 : Virtual address of memory buffer to write pipe contents to
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u16, Actual read size
     */
    void ReadPipeIfPossible(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::LoadComponent service function
     *  Inputs:
     *      0 : Header Code[0x001100C2]
     *      1 : Size
     *      2 : Program mask (observed only half word used)
     *      3 : Data mask (observed only half word used)
     *      4 : (size << 4) | 0xA
     *      5 : Component Buffer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u8, Component Loaded (0 = not loaded, 1 = loaded)
     *      3 : (Size << 4) | 0xA
     *      4 : Component Buffer
     */
    void LoadComponent(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::UnloadComponent service function
     *  Inputs:
     *      0 : Header Code[0x00120000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void UnloadComponent(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::FlushDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x00130082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void FlushDataCache(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::InvalidateDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x00140082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::RegisterInterruptEvents service function
     *  Inputs:
     *      0 : Header Code[0x00150082]
     *      1 : Interrupt
     *      2 : Channel
     *      3 : 0x0, some descriptor for the Event Handle
     *      4 : Interrupt Event handle (0 = unregister the event that was previous registered)
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void RegisterInterruptEvents(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::GetSemaphoreEventHandle service function
     *  Inputs:
     *      0 : Header Code[0x00160000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : 0x0, some descriptor for the Event Handle
     *      3 : Semaphore Event Handle
     */
    void GetSemaphoreEventHandle(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::SetSemaphoreMask service function
     *  Inputs:
     *      0 : Header Code[0x00170040]
     *      1 : u16, Mask
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetSemaphoreMask(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::GetHeadphoneStatus service function
     *  Inputs:
     *      0 : Header Code[0x001F0000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : u8, The headphone status response, 0 = Not inserted, 1 = inserted
     */
    void GetHeadphoneStatus(Kernel::HLERequestContext& ctx);

    /**
     * DSP_DSP::ForceHeadphoneOut service function
     *  Inputs:
     *      0 : Header Code[0x00020040]
     *      1 : u8, 0 = don't force, 1 = force
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ForceHeadphoneOut(Kernel::HLERequestContext& ctx);

    /// Returns the Interrupt Event for a given pipe
    std::shared_ptr<Kernel::Event>& GetInterruptEvent(InterruptType type, AudioCore::DspPipe pipe);
    /// Checks if we are trying to register more than 6 events
    bool HasTooManyEventsRegistered() const;

    Core::System& system;

    std::shared_ptr<Kernel::Event> semaphore_event;
    u16 preset_semaphore = 0;

    std::shared_ptr<Kernel::Event> interrupt_zero = nullptr; /// Currently unknown purpose
    std::shared_ptr<Kernel::Event> interrupt_one = nullptr;  /// Currently unknown purpose

    /// Each DSP pipe has an associated interrupt
    std::array<std::shared_ptr<Kernel::Event>, AudioCore::num_dsp_pipe> pipes = {{}};

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar & semaphore_event;
        ar & preset_semaphore;
        ar & interrupt_zero;
        ar & interrupt_one;
        ar & pipes;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::DSP

BOOST_CLASS_EXPORT_KEY(Service::DSP::DSP_DSP)
SERVICE_CONSTRUCT(Service::DSP::DSP_DSP)
