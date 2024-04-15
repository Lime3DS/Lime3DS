// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <boost/serialization/array.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::CSND {

enum class Encoding : u8 {
    Pcm8 = 0,
    Pcm16 = 1,
    Adpcm = 2,
    Psg = 3,
};

enum class LoopMode : u8 {
    Manual = 0,  // Play block 1 endlessly ignoring the size
    Normal = 1,  // Play block 1 once, then repeat with block 2. Block size is reloaded every time a
                 // new block is started
    OneShot = 2, // Play block 1 once and stop
    ConstantSize = 3, // Similar to Normal, but only load block size once at the beginning
};

struct AdpcmState {
    s16 predictor = 0;
    u8 step_index = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & predictor;
        ar & step_index;
    }
    friend class boost::serialization::access;
};

struct Channel {
    PAddr block1_address = 0;
    PAddr block2_address = 0;
    u32 block1_size = 0;
    u32 block2_size = 0;
    AdpcmState block1_adpcm_state;
    AdpcmState block2_adpcm_state;
    bool block2_adpcm_reload = false;
    u16 left_channel_volume = 0;
    u16 right_channel_volume = 0;
    u16 left_capture_volume = 0;
    u16 right_capture_volume = 0;
    u32 sample_rate = 0;
    bool linear_interpolation = false;
    LoopMode loop_mode = LoopMode::Manual;
    Encoding encoding = Encoding::Pcm8;
    u8 psg_duty = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & block1_address;
        ar & block2_address;
        ar & block1_size;
        ar & block2_size;
        ar & block1_adpcm_state;
        ar & block2_adpcm_state;
        ar & block2_adpcm_reload;
        ar & left_channel_volume;
        ar & right_channel_volume;
        ar & left_capture_volume;
        ar & right_capture_volume;
        ar & sample_rate;
        ar & linear_interpolation;
        ar & loop_mode;
        ar & encoding;
        ar & psg_duty;
    }
    friend class boost::serialization::access;
};

class CSND_SND final : public ServiceFramework<CSND_SND> {
public:
    explicit CSND_SND(Core::System& system);
    ~CSND_SND() = default;

private:
    /**
     * CSND_SND::Initialize service function
     *  Inputs:
     *      0 : Header Code[0x00010140]
     *      1 : Shared memory block size, for mem-block creation
     *      2 : offset to master state located in the shared-memory, region size=8
     *      3 : offset to channel state located in the shared-memory, region size=12*num_channels
     *      4 : offset to capture state located in the shared-memory, region size=8*num_captures
     *      5 : offset to type 1 commands (?) located in the shared-memory.
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Handle-list header
     *      3 : Mutex handle
     *      4 : Shared memory block handle
     */
    void Initialize(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::Shutdown service function
     *  Inputs:
     *      0 : Header Code[0x00020000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Shutdown(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::ExecuteCommands service function
     *  Inputs:
     *      0 : Header Code[0x00030040]
     *      1 : Command offset in shared memory.
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ExecuteCommands(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::ExecuteType1Commands service function
     *  Inputs:
     *      0 : Header Code[0x00040080]
     *      1 : unknown
     *      2 : unknown
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ExecuteType1Commands(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::AcquireSoundChannels service function
     *  Inputs:
     *      0 : Header Code[0x00050000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Available channel bit mask
     */
    void AcquireSoundChannels(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::ReleaseSoundChannels service function
     *  Inputs:
     *      0 : Header Code[0x00060000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ReleaseSoundChannels(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::AcquireCapUnit service function
     *     This function tries to acquire one capture device (max: 2).
     *     Returns index of which capture device was acquired.
     *  Inputs:
     *      0 : Header Code[0x00070000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Capture Unit
     */
    void AcquireCapUnit(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::ReleaseCapUnit service function
     *  Inputs:
     *      0 : Header Code[0x00080040]
     *      1 : Capture Unit
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ReleaseCapUnit(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::FlushDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x00090082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void FlushDataCache(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::StoreDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x000A0082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void StoreDataCache(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::InvalidateDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x000B0082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::Reset service function
     *  Inputs:
     *      0 : Header Code[0x000C0000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Reset(Kernel::HLERequestContext& ctx);

    Core::System& system;

    std::shared_ptr<Kernel::Mutex> mutex = nullptr;
    std::shared_ptr<Kernel::SharedMemory> shared_memory = nullptr;

    static constexpr u32 MaxCaptureUnits = 2;
    std::array<bool, MaxCaptureUnits> capture_units = {false, false};

    static constexpr u32 ChannelCount = 32;
    std::array<Channel, ChannelCount> channels;

    u32 master_state_offset = 0;
    u32 channel_state_offset = 0;
    u32 capture_state_offset = 0;
    u32 type1_command_offset = 0;

    u32 acquired_channel_mask = 0;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar & mutex;
        ar & shared_memory;
        ar & capture_units;
        ar & channels;
        ar & master_state_offset;
        ar & channel_state_offset;
        ar & capture_state_offset;
        ar & type1_command_offset;
        ar & acquired_channel_mask;
    }
    friend class boost::serialization::access;
};

/// Initializes the CSND_SND Service
void InstallInterfaces(Core::System& system);

} // namespace Service::CSND

BOOST_CLASS_EXPORT_KEY(Service::CSND::CSND_SND)
SERVICE_CONSTRUCT(Service::CSND::CSND_SND)
