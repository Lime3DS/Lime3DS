// Copyright 2018 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/file_sys/archive_backend.h"
#include "core/hle/service/service.h"

namespace Service::FS {

class Directory final : public ServiceFramework<Directory> {
public:
    Directory(std::unique_ptr<FileSys::DirectoryBackend>&& backend, const FileSys::Path& path);
    ~Directory();

    std::string GetName() const {
        return "Directory: " + path.DebugStr();
    }

    FileSys::Path path;                                 ///< Path of the directory
    std::unique_ptr<FileSys::DirectoryBackend> backend; ///< File backend interface

protected:
    void Read(Kernel::HLERequestContext& ctx);
    void Close(Kernel::HLERequestContext& ctx);

private:
    Directory();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Service::FS

BOOST_CLASS_EXPORT_KEY(Service::FS::Directory)
