// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/result.h"

namespace FileSys {

/// Archive backend for general save data archive type (SaveData and SystemSaveData)
class SaveDataArchive : public ArchiveBackend {
public:
    explicit SaveDataArchive(const std::string& mount_point_, bool allow_zero_size_create_ = true)
        : mount_point(mount_point_), allow_zero_size_create(allow_zero_size_create_) {}

    std::string GetName() const override {
        return "SaveDataArchive: " + mount_point;
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
    std::string mount_point;
    bool allow_zero_size_create;
    SaveDataArchive() = default;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveBackend>(*this);
        ar & mount_point;
        ar & allow_zero_size_create;
    }
    friend class boost::serialization::access;
};

class SaveDataDelayGenerator;
class ExtSaveDataArchive;

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::SaveDataArchive)
BOOST_CLASS_EXPORT_KEY(FileSys::SaveDataDelayGenerator)
BOOST_CLASS_EXPORT_KEY(FileSys::ExtSaveDataArchive)
