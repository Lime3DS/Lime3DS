// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <boost/serialization/base_object.hpp>
#include "core/file_sys/archive_backend.h"
#include "core/global.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::FS {

struct FileSessionSlot : public Kernel::SessionRequestHandler::SessionDataBase {
    u32 priority; ///< Priority of the file. TODO(Subv): Find out what this means
    u64 offset;   ///< Offset that this session will start reading from.
    u64 size;     ///< Max size of the file that this session is allowed to access
    bool subfile; ///< Whether this file was opened via OpenSubFile or not.

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(
            *this);
        ar & priority;
        ar & offset;
        ar & size;
        ar & subfile;
    }
    friend class boost::serialization::access;
};

// TODO: File is not a real service, but it can still utilize ServiceFramework::RegisterHandlers.
// Consider splitting ServiceFramework interface.
class File final : public ServiceFramework<File, FileSessionSlot> {
public:
    File(Kernel::KernelSystem& kernel, std::unique_ptr<FileSys::FileBackend>&& backend,
         const FileSys::Path& path);
    ~File() = default;

    std::string GetName() const {
        return "Path: " + path.DebugStr();
    }

    FileSys::Path path;                            ///< Path of the file
    std::unique_ptr<FileSys::FileBackend> backend; ///< File backend interface

    /// Creates a new session to this File and returns the ClientSession part of the connection.
    std::shared_ptr<Kernel::ClientSession> Connect();

    // Returns the start offset of an open file represented by the input session, opened with
    // OpenSubFile.
    std::size_t GetSessionFileOffset(std::shared_ptr<Kernel::ServerSession> session);

    // Returns the size of an open file represented by the input session, opened with
    // OpenSubFile.
    std::size_t GetSessionFileSize(std::shared_ptr<Kernel::ServerSession> session);

private:
    void Read(Kernel::HLERequestContext& ctx);
    void Write(Kernel::HLERequestContext& ctx);
    void GetSize(Kernel::HLERequestContext& ctx);
    void SetSize(Kernel::HLERequestContext& ctx);
    void Close(Kernel::HLERequestContext& ctx);
    void Flush(Kernel::HLERequestContext& ctx);
    void SetPriority(Kernel::HLERequestContext& ctx);
    void GetPriority(Kernel::HLERequestContext& ctx);
    void OpenLinkFile(Kernel::HLERequestContext& ctx);
    void OpenSubFile(Kernel::HLERequestContext& ctx);

    Kernel::KernelSystem& kernel;

    File(Kernel::KernelSystem& kernel);
    File();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Service::FS

BOOST_CLASS_EXPORT_KEY(Service::FS::FileSessionSlot)
BOOST_CLASS_EXPORT_KEY(Service::FS::File)
