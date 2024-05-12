// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <utility>
#include "common/archives.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/file_sys/ivfc_archive.h"

SERIALIZE_EXPORT_IMPL(FileSys::IVFCFile)
SERIALIZE_EXPORT_IMPL(FileSys::IVFCFileInMemory)
SERIALIZE_EXPORT_IMPL(FileSys::IVFCDelayGenerator)
SERIALIZE_EXPORT_IMPL(FileSys::RomFSDelayGenerator)
SERIALIZE_EXPORT_IMPL(FileSys::ExeFSDelayGenerator)

namespace FileSys {

IVFCArchive::IVFCArchive(std::shared_ptr<RomFSReader> file,
                         std::unique_ptr<DelayGenerator> delay_generator_)
    : romfs_file(std::move(file)) {
    delay_generator = std::move(delay_generator_);
}

std::string IVFCArchive::GetName() const {
    return "IVFC";
}

ResultVal<std::unique_ptr<FileBackend>> IVFCArchive::OpenFile(const Path& path, const Mode& mode,
                                                              u32 attributes) {
    std::unique_ptr<DelayGenerator> delay_generator = std::make_unique<IVFCDelayGenerator>();
    return std::make_unique<IVFCFile>(romfs_file, std::move(delay_generator));
}

Result IVFCArchive::DeleteFile(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a file from an IVFC archive ({}).", GetName());
    // TODO(Subv): Verify error code
    return Result(ErrorDescription::NoData, ErrorModule::FS, ErrorSummary::Canceled,
                  ErrorLevel::Status);
}

Result IVFCArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an IVFC archive ({}).", GetName());
    // TODO(wwylele): Use correct error code
    return ResultUnknown;
}

Result IVFCArchive::DeleteDirectory(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a directory from an IVFC archive ({}).",
                 GetName());
    // TODO(wwylele): Use correct error code
    return ResultUnknown;
}

Result IVFCArchive::DeleteDirectoryRecursively(const Path& path) const {
    LOG_CRITICAL(Service_FS, "Attempted to delete a directory from an IVFC archive ({}).",
                 GetName());
    // TODO(wwylele): Use correct error code
    return ResultUnknown;
}

Result IVFCArchive::CreateFile(const Path& path, u64 size, u32 attributes) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a file in an IVFC archive ({}).", GetName());
    // TODO: Verify error code
    return Result(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported,
                  ErrorLevel::Permanent);
}

Result IVFCArchive::CreateDirectory(const Path& path, u32 attributes) const {
    LOG_CRITICAL(Service_FS, "Attempted to create a directory in an IVFC archive ({}).", GetName());
    // TODO(wwylele): Use correct error code
    return ResultUnknown;
}

Result IVFCArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    LOG_CRITICAL(Service_FS, "Attempted to rename a file within an IVFC archive ({}).", GetName());
    // TODO(wwylele): Use correct error code
    return ResultUnknown;
}

ResultVal<std::unique_ptr<DirectoryBackend>> IVFCArchive::OpenDirectory(const Path& path) {
    return std::make_unique<IVFCDirectory>();
}

u64 IVFCArchive::GetFreeBytes() const {
    LOG_WARNING(Service_FS, "Attempted to get the free space in an IVFC archive");
    return 0;
}

IVFCFile::IVFCFile(std::shared_ptr<RomFSReader> file,
                   std::unique_ptr<DelayGenerator> delay_generator_)
    : romfs_file(std::move(file)) {
    delay_generator = std::move(delay_generator_);
}

ResultVal<std::size_t> IVFCFile::Read(const u64 offset, const std::size_t length,
                                      u8* buffer) const {
    LOG_TRACE(Service_FS, "called offset={}, length={}", offset, length);
    return romfs_file->ReadFile(offset, length, buffer);
}

ResultVal<std::size_t> IVFCFile::Write(const u64 offset, const std::size_t length, const bool flush,
                                       const bool update_timestamp, const u8* buffer) {
    LOG_ERROR(Service_FS, "Attempted to write to IVFC file");
    // TODO(Subv): Find error code
    return 0ULL;
}

u64 IVFCFile::GetSize() const {
    return romfs_file->GetSize();
}

bool IVFCFile::SetSize(const u64 size) const {
    LOG_ERROR(Service_FS, "Attempted to set the size of an IVFC file");
    return false;
}

IVFCFileInMemory::IVFCFileInMemory(std::vector<u8> bytes, u64 offset, u64 size,
                                   std::unique_ptr<DelayGenerator> delay_generator_)
    : romfs_file(std::move(bytes)), data_offset(offset), data_size(size) {
    delay_generator = std::move(delay_generator_);
}

ResultVal<std::size_t> IVFCFileInMemory::Read(const u64 offset, const std::size_t length,
                                              u8* buffer) const {
    LOG_TRACE(Service_FS, "called offset={}, length={}", offset, length);
    std::size_t read_length = (std::size_t)std::min((u64)length, data_size - offset);

    std::memcpy(buffer, romfs_file.data() + data_offset + offset, read_length);
    return read_length;
}

ResultVal<std::size_t> IVFCFileInMemory::Write(const u64 offset, const std::size_t length,
                                               const bool flush, const bool update_timestamp,
                                               const u8* buffer) {
    LOG_ERROR(Service_FS, "Attempted to write to IVFC file");
    // TODO(Subv): Find error code
    return 0ULL;
}

u64 IVFCFileInMemory::GetSize() const {
    return data_size;
}

bool IVFCFileInMemory::SetSize(const u64 size) const {
    LOG_ERROR(Service_FS, "Attempted to set the size of an IVFC file");
    return false;
}

} // namespace FileSys
