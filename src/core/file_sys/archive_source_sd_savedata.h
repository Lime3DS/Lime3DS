// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <boost/serialization/export.hpp>
#include <boost/serialization/string.hpp>
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/artic_cache.h"
#include "core/hle/result.h"
#include "network/artic_base/artic_base_client.h"

namespace Service::FS {
enum class ArchiveIdCode : u32;
} // namespace Service::FS

namespace FileSys {

/// A common source of SD save data archive
class ArchiveSource_SDSaveData : public ArticCacheProvider {
public:
    explicit ArchiveSource_SDSaveData(const std::string& mount_point);

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(Service::FS::ArchiveIdCode archive_id,
                                                    const Path& path, u64 program_id);
    Result Format(u64 program_id, const FileSys::ArchiveFormatInfo& format_info,
                  Service::FS::ArchiveIdCode archive_id, const Path& path, u32 directory_buckets,
                  u32 file_buckets);
    ResultVal<ArchiveFormatInfo> GetFormatInfo(u64 program_id,
                                               Service::FS::ArchiveIdCode archive_id,
                                               const Path& path) const;

    static std::string GetSaveDataPathFor(const std::string& mount_point, u64 program_id);

    void RegisterArtic(std::shared_ptr<Network::ArticBase::Client>& client) {
        artic_client = client;
    }

    bool IsUsingArtic() const {
        return artic_client.get() != nullptr;
    }

private:
    std::string mount_point;
    std::shared_ptr<Network::ArticBase::Client> artic_client = nullptr;

    ArchiveSource_SDSaveData() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArticCacheProvider>(*this);
        ar & mount_point;
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::ArchiveSource_SDSaveData)
