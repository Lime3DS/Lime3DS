// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include "common/archives.h"
#include "common/error.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/path_parser.h"

SERIALIZE_EXPORT_IMPL(FileSys::SDMCArchive)
SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_SDMC)

namespace FileSys {

class SDMCDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // This is the delay measured on O3DS and O2DS with
        // https://gist.github.com/B3n30/ac40eac20603f519ff106107f4ac9182
        // from the results the average of each length was taken.
        static constexpr u64 slope(183);
        static constexpr u64 offset(524879);
        static constexpr u64 minimum(631826);
        u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return IPCDelayNanoseconds;
    }

    u64 GetOpenDelayNs() override {
        // This is the delay measured on O3DS and O2DS with
        // https://gist.github.com/FearlessTobi/c37e143c314789251f98f2c45cd706d2
        // from the results the average of each length was taken.
        static constexpr u64 IPCDelayNanoseconds(269082);
        return IPCDelayNanoseconds;
    }

    SERIALIZE_DELAY_GENERATOR
};

ResultVal<std::unique_ptr<FileBackend>> SDMCArchive::OpenFile(const Path& path, const Mode& mode,
                                                              u32 attributes) {
    Mode modified_mode;
    modified_mode.hex = mode.hex;

    // SDMC archive always opens a file with at least read permission
    modified_mode.read_flag.Assign(1);

    return OpenFileBase(path, modified_mode);
}

ResultVal<std::unique_ptr<FileBackend>> SDMCArchive::OpenFileBase(const Path& path,
                                                                  const Mode& mode) const {
    LOG_DEBUG(Service_FS, "called path={} mode={:01X}", path.DebugStr(), mode.hex);

    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    if (mode.hex == 0) {
        LOG_ERROR(Service_FS, "Empty open mode");
        return ResultInvalidOpenFlags;
    }

    if (mode.create_flag && !mode.write_flag) {
        LOG_ERROR(Service_FS, "Create flag set but write flag not set");
        return ResultInvalidOpenFlags;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultNotFound;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultNotFound;
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "{} is not a file", full_path);
        return ResultUnexpectedFileOrDirectorySdmc;
    case PathParser::NotFound:
        if (!mode.create_flag) {
            LOG_ERROR(Service_FS, "Non-existing file {} can't be open without mode create.",
                      full_path);
            return ResultNotFound;
        } else {
            // Create the file
            FileUtil::CreateEmptyFile(full_path);
        }
        break;
    case PathParser::FileFound:
        break; // Expected 'success' case
    }

    FileUtil::IOFile file(full_path, mode.write_flag ? "r+b" : "rb");
    if (!file.IsOpen()) {
        LOG_CRITICAL(Service_FS, "Error opening {}: {}", full_path, Common::GetLastErrorMsg());
        return ResultNotFound;
    }

    std::unique_ptr<DelayGenerator> delay_generator = std::make_unique<SDMCDelayGenerator>();
    return std::make_unique<DiskFile>(std::move(file), mode, std::move(delay_generator));
}

Result SDMCArchive::DeleteFile(const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultNotFound;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
    case PathParser::NotFound:
        LOG_DEBUG(Service_FS, "{} not found", full_path);
        return ResultNotFound;
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "{} is not a file", full_path);
        return ResultUnexpectedFileOrDirectorySdmc;
    case PathParser::FileFound:
        break; // Expected 'success' case
    }

    if (FileUtil::Delete(full_path)) {
        return ResultSuccess;
    }

    LOG_CRITICAL(Service_FS, "(unreachable) Unknown error deleting {}", full_path);
    return ResultNotFound;
}

Result SDMCArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
    const PathParser path_parser_src(src_path);

    // TODO: Verify these return codes with HW
    if (!path_parser_src.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid src path {}", src_path.DebugStr());
        return ResultInvalidPath;
    }

    const PathParser path_parser_dest(dest_path);

    if (!path_parser_dest.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid dest path {}", dest_path.DebugStr());
        return ResultInvalidPath;
    }

    const auto src_path_full = path_parser_src.BuildHostPath(mount_point);
    const auto dest_path_full = path_parser_dest.BuildHostPath(mount_point);

    if (FileUtil::Rename(src_path_full, dest_path_full)) {
        return ResultSuccess;
    }

    // TODO(yuriks): This code probably isn't right, it'll return a Status even if the file didn't
    // exist or similar. Verify.
    return Result(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                  ErrorSummary::NothingHappened, ErrorLevel::Status);
}

template <typename T>
static Result DeleteDirectoryHelper(const Path& path, const std::string& mount_point, T deleter) {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    if (path_parser.IsRootDirectory())
        return ResultNotFound;

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultNotFound;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultNotFound;
    case PathParser::FileInPath:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "Unexpected file in path {}", full_path);
        return ResultUnexpectedFileOrDirectorySdmc;
    case PathParser::DirectoryFound:
        break; // Expected 'success' case
    }

    if (deleter(full_path)) {
        return ResultSuccess;
    }

    LOG_ERROR(Service_FS, "Directory not empty {}", full_path);
    return ResultUnexpectedFileOrDirectorySdmc;
}

Result SDMCArchive::DeleteDirectory(const Path& path) const {
    return DeleteDirectoryHelper(path, mount_point, FileUtil::DeleteDir);
}

Result SDMCArchive::DeleteDirectoryRecursively(const Path& path) const {
    return DeleteDirectoryHelper(
        path, mount_point, [](const std::string& p) { return FileUtil::DeleteDirRecursively(p); });
}

Result SDMCArchive::CreateFile(const FileSys::Path& path, u64 size, u32 attributes) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultNotFound;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultNotFound;
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "{} already exists", full_path);
        return ResultUnexpectedFileOrDirectorySdmc;
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "{} already exists", full_path);
        return ResultAlreadyExists;
    case PathParser::NotFound:
        break; // Expected 'success' case
    }

    if (size == 0) {
        FileUtil::CreateEmptyFile(full_path);
        return ResultSuccess;
    }

    FileUtil::IOFile file(full_path, "wb");
    // Creates a sparse file (or a normal file on filesystems without the concept of sparse files)
    // We do this by seeking to the right size, then writing a single null byte.
    if (file.Seek(size - 1, SEEK_SET) && file.WriteBytes("", 1) == 1) {
        return ResultSuccess;
    }

    LOG_ERROR(Service_FS, "Too large file");
    return Result(ErrorDescription::TooLarge, ErrorModule::FS, ErrorSummary::OutOfResource,
                  ErrorLevel::Info);
}

Result SDMCArchive::CreateDirectory(const Path& path, u32 attributes) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultNotFound;
    case PathParser::PathNotFound:
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultNotFound;
    case PathParser::DirectoryFound:
    case PathParser::FileFound:
        LOG_DEBUG(Service_FS, "{} already exists", full_path);
        return ResultAlreadyExists;
    case PathParser::NotFound:
        break; // Expected 'success' case
    }

    if (FileUtil::CreateDir(mount_point + path.AsString())) {
        return ResultSuccess;
    }

    LOG_CRITICAL(Service_FS, "(unreachable) Unknown error creating {}", mount_point);
    return Result(ErrorDescription::NoData, ErrorModule::FS, ErrorSummary::Canceled,
                  ErrorLevel::Status);
}

Result SDMCArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    const PathParser path_parser_src(src_path);

    // TODO: Verify these return codes with HW
    if (!path_parser_src.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid src path {}", src_path.DebugStr());
        return ResultInvalidPath;
    }

    const PathParser path_parser_dest(dest_path);

    if (!path_parser_dest.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid dest path {}", dest_path.DebugStr());
        return ResultInvalidPath;
    }

    const auto src_path_full = path_parser_src.BuildHostPath(mount_point);
    const auto dest_path_full = path_parser_dest.BuildHostPath(mount_point);

    if (FileUtil::Rename(src_path_full, dest_path_full)) {
        return ResultSuccess;
    }

    // TODO(yuriks): This code probably isn't right, it'll return a Status even if the file didn't
    // exist or similar. Verify.
    return Result(ErrorDescription::NoData, ErrorModule::FS, // TODO: verify description
                  ErrorSummary::NothingHappened, ErrorLevel::Status);
}

ResultVal<std::unique_ptr<DirectoryBackend>> SDMCArchive::OpenDirectory(const Path& path) {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultNotFound;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "{} not found", full_path);
        return ResultNotFound;
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Unexpected file in path {}", full_path);
        return ResultUnexpectedFileOrDirectorySdmc;
    case PathParser::DirectoryFound:
        break; // Expected 'success' case
    }

    return std::make_unique<DiskDirectory>(full_path);
}

u64 SDMCArchive::GetFreeBytes() const {
    // TODO: Stubbed to return 1GiB
    return 1024 * 1024 * 1024;
}

ArchiveFactory_SDMC::ArchiveFactory_SDMC(const std::string& sdmc_directory)
    : sdmc_directory(sdmc_directory) {

    LOG_DEBUG(Service_FS, "Directory {} set as SDMC.", sdmc_directory);
}

bool ArchiveFactory_SDMC::Initialize() {
    if (!Settings::values.use_virtual_sd) {
        LOG_WARNING(Service_FS, "SDMC disabled by config.");
        return false;
    }

    if (!FileUtil::CreateFullPath(sdmc_directory)) {
        LOG_ERROR(Service_FS, "Unable to create SDMC path.");
        return false;
    }

    return true;
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SDMC::Open(const Path& path,
                                                                     u64 program_id) {
    std::unique_ptr<DelayGenerator> delay_generator = std::make_unique<SDMCDelayGenerator>();
    return std::make_unique<SDMCArchive>(sdmc_directory, std::move(delay_generator));
}

Result ArchiveFactory_SDMC::Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info,
                                   u64 program_id, u32 directory_buckets, u32 file_buckets) {
    // This is kind of an undesirable operation, so let's just ignore it. :)
    return ResultSuccess;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SDMC::GetFormatInfo(const Path& path,
                                                                u64 program_id) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive {}", GetName());
    return ResultUnknown;
}
} // namespace FileSys

SERIALIZE_EXPORT_IMPL(FileSys::SDMCDelayGenerator)
