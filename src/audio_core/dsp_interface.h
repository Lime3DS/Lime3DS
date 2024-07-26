// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <boost/serialization/access.hpp>
#include "audio_core/audio_types.h"
#include "audio_core/time_stretch.h"
#include "common/common_types.h"
#include "common/ring_buffer.h"
#include "core/memory.h"

namespace Core {
class System;
} // namespace Core

namespace Service::DSP {
enum class InterruptType : u32;
} // namespace Service::DSP

namespace AudioCore {

class Sink;
enum class SinkType : u32;

class DspInterface {
public:
    DspInterface(Core::System& system_);
    virtual ~DspInterface();

    DspInterface(const DspInterface&) = delete;
    DspInterface(DspInterface&&) = delete;
    DspInterface& operator=(const DspInterface&) = delete;
    DspInterface& operator=(DspInterface&&) = delete;

    /**
     * Reads data from one of three DSP registers
     * @note this function blocks until the data is available
     * @param register_number the index of the register to read
     * @returns the value of the register
     */
    virtual u16 RecvData(u32 register_number) = 0;

    /**
     * Checks whether data is ready in one of three DSP registers
     * @param register_number the index of the register to check
     * @returns true if data is ready
     */
    virtual bool RecvDataIsReady(u32 register_number) const = 0;

    /**
     * Sets the DSP semaphore register
     * @param semaphore_value the value set to the semaphore register
     */
    virtual void SetSemaphore(u16 semaphore_value) = 0;

    /**
     * Reads `length` bytes from the DSP pipe identified with `pipe_number`.
     * @note Can read up to the maximum value of a u16 in bytes (65,535).
     * @note IF an error is encoutered with either an invalid `pipe_number` or `length` value, an
     * empty vector will be returned.
     * @note IF `length` is set to 0, an empty vector will be returned.
     * @note IF `length` is greater than the amount of data available, this function will only read
     * the available amount.
     * @param pipe_number a `DspPipe`
     * @param length the number of bytes to read. The max is 65,535 (max of u16).
     * @returns a vector of bytes from the specified pipe. On error, will be empty.
     */
    virtual std::vector<u8> PipeRead(DspPipe pipe_number, std::size_t length) = 0;

    /**
     * How much data is left in pipe
     * @param pipe_number The Pipe ID
     * @return The amount of data remaning in the pipe. This is the maximum length PipeRead will
     * return.
     */
    virtual std::size_t GetPipeReadableSize(DspPipe pipe_number) const = 0;

    /**
     * Write to a DSP pipe.
     * @param pipe_number The Pipe ID
     * @param buffer The data to write to the pipe.
     */
    virtual void PipeWrite(DspPipe pipe_number, std::span<const u8> buffer) = 0;

    /// Returns a reference to the array backing DSP memory
    virtual std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory() = 0;

    /// Sets the handler for the interrupts we trigger
    virtual void SetInterruptHandler(
        std::function<void(Service::DSP::InterruptType type, DspPipe pipe)> handler) = 0;

    /// Loads the DSP program
    virtual void LoadComponent(std::span<const u8> buffer) = 0;

    /// Unloads the DSP program
    virtual void UnloadComponent() = 0;

    /// Select the sink to use based on sink type.
    void SetSink(SinkType sink_type, std::string_view audio_device);
    /// Get the current sink
    Sink& GetSink();
    /// Enable/Disable audio stretching.
    void EnableStretching(bool enable);

protected:
    void OutputFrame(StereoFrame16 frame);
    void OutputSample(std::array<s16, 2> sample);

private:
    void FlushResidualStretcherAudio();
    void OutputCallback(s16* buffer, std::size_t num_frames);

    Core::System& system;

    std::atomic<bool> enable_time_stretching = false;
    std::atomic<bool> performing_time_stretching = false;
    std::atomic<bool> flushing_time_stretcher = false;
    Common::RingBuffer<s16, 0x2000, 2> fifo;
    std::array<s16, 2> last_frame{};
    TimeStretcher time_stretcher;
    std::unique_ptr<Sink> sink;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;
};

} // namespace AudioCore
