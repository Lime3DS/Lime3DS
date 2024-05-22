// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <boost/serialization/unique_ptr.hpp>
#include "common/common_types.h"
#include "core/hle/result.h"
#include "delay_generator.h"

namespace FileSys {

class FileBackend : NonCopyable {
public:
    FileBackend() {}
    virtual ~FileBackend() {}

    /**
     * Read data from the file
     * @param offset Offset in bytes to start reading data from
     * @param length Length in bytes of data to read from file
     * @param buffer Buffer to read data into
     * @return Number of bytes read, or error code
     */
    virtual ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const = 0;

    /**
     * Write data to the file
     * @param offset Offset in bytes to start writing data to
     * @param length Length in bytes of data to write to file
     * @param flush The flush parameters (0 == do not flush)
     * @param buffer Buffer to read data from
     * @return Number of bytes written, or error code
     */
    virtual ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush,
                                         bool update_timestamp, const u8* buffer) = 0;

    /**
     * Get the amount of time a 3ds needs to read those data
     * @param length Length in bytes of data read from file
     * @return Nanoseconds for the delay
     */
    u64 GetReadDelayNs(std::size_t length) {
        if (delay_generator != nullptr) {
            return delay_generator->GetReadDelayNs(length);
        }
        LOG_ERROR(Service_FS, "Delay generator was not initalized. Using default");
        delay_generator = std::make_unique<DefaultDelayGenerator>();
        return delay_generator->GetReadDelayNs(length);
    }

    u64 GetOpenDelayNs() {
        if (delay_generator != nullptr) {
            return delay_generator->GetOpenDelayNs();
        }
        LOG_ERROR(Service_FS, "Delay generator was not initalized. Using default");
        delay_generator = std::make_unique<DefaultDelayGenerator>();
        return delay_generator->GetOpenDelayNs();
    }

    /**
     * Get the size of the file in bytes
     * @return Size of the file in bytes
     */
    virtual u64 GetSize() const = 0;

    /**
     * Set the size of the file in bytes
     * @param size New size of the file
     * @return true if successful
     */
    virtual bool SetSize(u64 size) const = 0;

    /**
     * Close the file
     * @return true if the file closed correctly
     */
    virtual bool Close() = 0;

    /**
     * Flushes the file
     */
    virtual void Flush() const = 0;

    /**
     * Whether the backend supports cached reads.
     */
    virtual bool AllowsCachedReads() const {
        return false;
    }

    /**
     * Whether the cache is ready for a specified offset and length.
     */
    virtual bool CacheReady(std::size_t file_offset, std::size_t length) {
        return false;
    }

protected:
    std::unique_ptr<DelayGenerator> delay_generator;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & delay_generator;
    }
    friend class boost::serialization::access;
};

} // namespace FileSys
