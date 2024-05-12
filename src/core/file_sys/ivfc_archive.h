// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <boost/serialization/export.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/file_sys/romfs_reader.h"
#include "core/hle/result.h"

namespace FileSys {

class IVFCDelayGenerator : public DelayGenerator {
    u64 GetReadDelayNs(std::size_t length) override {
        // This is the delay measured for a romfs read.
        // For now we will take that as a default
        static constexpr u64 slope(94);
        static constexpr u64 offset(582778);
        static constexpr u64 minimum(663124);
        u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return IPCDelayNanoseconds;
    }

    u64 GetOpenDelayNs() override {
        // This is the delay measured for a romfs open.
        // For now we will take that as a default
        static constexpr u64 IPCDelayNanoseconds(9438006);
        return IPCDelayNanoseconds;
    }

    SERIALIZE_DELAY_GENERATOR
};

class RomFSDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // The delay was measured on O3DS and O2DS with
        // https://gist.github.com/B3n30/ac40eac20603f519ff106107f4ac9182
        // from the results the average of each length was taken.
        static constexpr u64 slope(94);
        static constexpr u64 offset(582778);
        static constexpr u64 minimum(663124);
        u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return IPCDelayNanoseconds;
    }

    u64 GetOpenDelayNs() override {
        // This is the delay measured on O3DS and O2DS with
        // https://gist.github.com/FearlessTobi/eb1d70619c65c7e6f02141d71e79a36e
        // from the results the average of each length was taken.
        static constexpr u64 IPCDelayNanoseconds(9438006);
        return IPCDelayNanoseconds;
    }

    SERIALIZE_DELAY_GENERATOR
};

class ExeFSDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // The delay was measured on O3DS and O2DS with
        // https://gist.github.com/B3n30/ac40eac20603f519ff106107f4ac9182
        // from the results the average of each length was taken.
        static constexpr u64 slope(94);
        static constexpr u64 offset(582778);
        static constexpr u64 minimum(663124);
        u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return IPCDelayNanoseconds;
    }

    u64 GetOpenDelayNs() override {
        // This is the delay measured on O3DS and O2DS with
        // https://gist.github.com/FearlessTobi/eb1d70619c65c7e6f02141d71e79a36e
        // from the results the average of each length was taken.
        static constexpr u64 IPCDelayNanoseconds(9438006);
        return IPCDelayNanoseconds;
    }

    SERIALIZE_DELAY_GENERATOR
};

/**
 * Helper which implements an interface to deal with IVFC images used in some archives
 * This should be subclassed by concrete archive types, which will provide the
 * input data (load the raw IVFC archive) and override any required methods
 */
class IVFCArchive : public ArchiveBackend {
public:
    IVFCArchive(std::shared_ptr<RomFSReader> file,
                std::unique_ptr<DelayGenerator> delay_generator_);

    std::string GetName() const override;

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path, const Mode& mode,
                                                     u32 attributes) override;
    Result DeleteFile(const Path& path) const override;
    Result RenameFile(const Path& src_path, const Path& dest_path) const override;
    Result DeleteDirectory(const Path& path) const override;
    Result DeleteDirectoryRecursively(const Path& path) const override;
    Result CreateFile(const Path& path, u64 size, u32 attributes) const override;
    Result CreateDirectory(const Path& path, u32 attributes) const override;
    Result RenameDirectory(const Path& src_path, const Path& dest_path) const override;
    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) override;
    u64 GetFreeBytes() const override;

protected:
    std::shared_ptr<RomFSReader> romfs_file;
};

class IVFCFile : public FileBackend {
public:
    IVFCFile(std::shared_ptr<RomFSReader> file, std::unique_ptr<DelayGenerator> delay_generator_);

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;
    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush, bool update_timestamp,
                                 const u8* buffer) override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() override {
        return false;
    }
    void Flush() const override {}

    bool AllowsCachedReads() const override {
        return romfs_file->AllowsCachedReads();
    }

    bool CacheReady(std::size_t file_offset, std::size_t length) override {
        return romfs_file->CacheReady(file_offset, length);
    }

private:
    std::shared_ptr<RomFSReader> romfs_file;

    IVFCFile() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<FileBackend>(*this);
        ar & romfs_file;
    }
    friend class boost::serialization::access;
};

class IVFCDirectory : public DirectoryBackend {
public:
    u32 Read(const u32 count, Entry* entries) override {
        return 0;
    }
    bool Close() override {
        return false;
    }
};

class IVFCFileInMemory : public FileBackend {
public:
    IVFCFileInMemory(std::vector<u8> bytes, u64 offset, u64 size,
                     std::unique_ptr<DelayGenerator> delay_generator_);

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;
    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush, bool update_timestamp,
                                 const u8* buffer) override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() override {
        return false;
    }
    void Flush() const override {}

private:
    std::vector<u8> romfs_file;
    u64 data_offset;
    u64 data_size;

    IVFCFileInMemory() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<FileBackend>(*this);
        ar & romfs_file;
        ar & data_offset;
        ar & data_size;
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::IVFCFile)
BOOST_CLASS_EXPORT_KEY(FileSys::IVFCFileInMemory)
BOOST_CLASS_EXPORT_KEY(FileSys::IVFCDelayGenerator)
BOOST_CLASS_EXPORT_KEY(FileSys::RomFSDelayGenerator)
BOOST_CLASS_EXPORT_KEY(FileSys::ExeFSDelayGenerator)
