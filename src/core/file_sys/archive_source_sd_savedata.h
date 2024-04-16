// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <boost/serialization/export.hpp>
#include <boost/serialization/string.hpp>
#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"

namespace FileSys {

/// A common source of SD save data archive
class ArchiveSource_SDSaveData {
public:
    explicit ArchiveSource_SDSaveData(const std::string& mount_point);

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(u64 program_id);
    Result Format(u64 program_id, const FileSys::ArchiveFormatInfo& format_info);
    ResultVal<ArchiveFormatInfo> GetFormatInfo(u64 program_id) const;

    static std::string GetSaveDataPathFor(const std::string& mount_point, u64 program_id);

private:
    std::string mount_point;

    ArchiveSource_SDSaveData() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & mount_point;
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::ArchiveSource_SDSaveData)
