// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/artic_cache.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/result.h"
#include "network/artic_base/artic_base_client.h"

namespace Service::FS {
enum class MediaType : u32;
} // namespace Service::FS

namespace FileSys {

enum class NCCHFilePathType : u32 {
    RomFS = 0,
    Code = 1,
    ExeFS = 2,
};

enum class NCCHFileOpenType : u32 {
    NCCHData = 0,
    SaveData = 1,
};

/// Helper function to generate a Path for NCCH archives
Path MakeNCCHArchivePath(u64 tid, Service::FS::MediaType media_type);

/// Helper function to generate a Path for NCCH files
Path MakeNCCHFilePath(NCCHFileOpenType open_type, u32 content_index, NCCHFilePathType filepath_type,
                      std::array<char, 8>& exefs_filepath);

/// Archive backend for NCCH Archives (RomFS, ExeFS)
class NCCHArchive : public ArchiveBackend {
public:
    explicit NCCHArchive(u64 title_id, Service::FS::MediaType media_type)
        : title_id(title_id), media_type(media_type) {}

    std::string GetName() const override {
        return "NCCHArchive";
    }

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
    u64 title_id;
    Service::FS::MediaType media_type;

private:
    NCCHArchive() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveBackend>(*this);
        ar & title_id;
        ar & media_type;
    }
    friend class boost::serialization::access;
};

// File backend for NCCH files
class NCCHFile : public FileBackend {
public:
    explicit NCCHFile(std::vector<u8> buffer, std::unique_ptr<DelayGenerator> delay_generator_);

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
    std::vector<u8> file_buffer;

    NCCHFile() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<FileBackend>(*this);
        ar & file_buffer;
    }
    friend class boost::serialization::access;
};

/// File system interface to the NCCH archive
class ArchiveFactory_NCCH final : public ArchiveFactory, public ArticCacheProvider {
public:
    explicit ArchiveFactory_NCCH();

    std::string GetName() const override {
        return "NCCH";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path, u64 program_id) override;
    Result Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info, u64 program_id,
                  u32 directory_buckets, u32 file_buckets) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path, u64 program_id) const override;

    bool IsSlow() override {
        return IsUsingArtic();
    }

    void RegisterArtic(std::shared_ptr<Network::ArticBase::Client>& client) {
        artic_client = client;
    }

    bool IsUsingArtic() const {
        return artic_client.get() != nullptr;
    }

private:
    std::shared_ptr<Network::ArticBase::Client> artic_client = nullptr;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveFactory>(*this);
        ar& boost::serialization::base_object<ArticCacheProvider>(*this);
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::NCCHArchive)
BOOST_CLASS_EXPORT_KEY(FileSys::NCCHFile)
BOOST_CLASS_EXPORT_KEY(FileSys::ArchiveFactory_NCCH)
