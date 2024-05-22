// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/file_util.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/path_parser.h"
#include "core/file_sys/savedata_archive.h"

namespace FileSys {

class SaveDataDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // The delay was measured on O3DS and O2DS with
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

ResultVal<std::unique_ptr<FileBackend>> SaveDataArchive::OpenFile(const Path& path,
                                                                  const Mode& mode,
                                                                  u32 attributes) {
    LOG_DEBUG(Service_FS, "called path={} mode={:01X}", path.DebugStr(), mode.hex);

    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    if (mode.hex == 0) {
        LOG_ERROR(Service_FS, "Empty open mode");
        return ResultUnsupportedOpenFlags;
    }

    if (mode.create_flag && !mode.write_flag) {
        LOG_ERROR(Service_FS, "Create flag set but write flag not set");
        return ResultUnsupportedOpenFlags;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultFileNotFound;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultPathNotFound;
    case PathParser::FileInPath:
    case PathParser::DirectoryFound:
        LOG_ERROR(Service_FS, "Unexpected file or directory in {}", full_path);
        return ResultUnexpectedFileOrDirectory;
    case PathParser::NotFound:
        if (!mode.create_flag) {
            LOG_ERROR(Service_FS, "Non-existing file {} can't be open without mode create.",
                      full_path);
            return ResultFileNotFound;
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
        LOG_CRITICAL(Service_FS, "(unreachable) Unknown error opening {}", full_path);
        return ResultFileNotFound;
    }

    std::unique_ptr<DelayGenerator> delay_generator = std::make_unique<SaveDataDelayGenerator>();
    return std::make_unique<DiskFile>(std::move(file), mode, std::move(delay_generator));
}

Result SaveDataArchive::DeleteFile(const Path& path) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultFileNotFound;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultPathNotFound;
    case PathParser::FileInPath:
    case PathParser::DirectoryFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "File not found {}", full_path);
        return ResultFileNotFound;
    case PathParser::FileFound:
        break; // Expected 'success' case
    }

    if (FileUtil::Delete(full_path)) {
        return ResultSuccess;
    }

    LOG_CRITICAL(Service_FS, "(unreachable) Unknown error deleting {}", full_path);
    return ResultFileNotFound;
}

Result SaveDataArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
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
        return ResultDirectoryNotEmpty;

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultPathNotFound;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultPathNotFound;
    case PathParser::FileInPath:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "Unexpected file or directory {}", full_path);
        return ResultUnexpectedFileOrDirectory;
    case PathParser::DirectoryFound:
        break; // Expected 'success' case
    }

    if (deleter(full_path)) {
        return ResultSuccess;
    }

    LOG_ERROR(Service_FS, "Directory not empty {}", full_path);
    return ResultDirectoryNotEmpty;
}

Result SaveDataArchive::DeleteDirectory(const Path& path) const {
    return DeleteDirectoryHelper(path, mount_point, FileUtil::DeleteDir);
}

Result SaveDataArchive::DeleteDirectoryRecursively(const Path& path) const {
    return DeleteDirectoryHelper(
        path, mount_point, [](const std::string& p) { return FileUtil::DeleteDirRecursively(p); });
}

Result SaveDataArchive::CreateFile(const FileSys::Path& path, u64 size, u32 attributes) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultFileNotFound;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultPathNotFound;
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Unexpected file in path {}", full_path);
        return ResultUnexpectedFileOrDirectory;
    case PathParser::DirectoryFound:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "{} already exists", full_path);
        return ResultFileAlreadyExists;
    case PathParser::NotFound:
        break; // Expected 'success' case
    }

    if (size == 0) {
        if (allow_zero_size_create) {
            FileUtil::CreateEmptyFile(full_path);
            return ResultSuccess;
        } else {
            LOG_DEBUG(Service_FS, "Zero-size file is not supported");
            return ResultUnsupportedOpenFlags;
        }
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

Result SaveDataArchive::CreateDirectory(const Path& path, u32 attributes) const {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultFileNotFound;
    case PathParser::PathNotFound:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultPathNotFound;
    case PathParser::FileInPath:
        LOG_ERROR(Service_FS, "Unexpected file in path {}", full_path);
        return ResultUnexpectedFileOrDirectory;
    case PathParser::DirectoryFound:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "{} already exists", full_path);
        return ResultDirectoryAlreadyExists;
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

Result SaveDataArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
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

ResultVal<std::unique_ptr<DirectoryBackend>> SaveDataArchive::OpenDirectory(const Path& path) {
    const PathParser path_parser(path);

    if (!path_parser.IsValid()) {
        LOG_ERROR(Service_FS, "Invalid path {}", path.DebugStr());
        return ResultInvalidPath;
    }

    const auto full_path = path_parser.BuildHostPath(mount_point);

    switch (path_parser.GetHostStatus(mount_point)) {
    case PathParser::InvalidMountPoint:
        LOG_CRITICAL(Service_FS, "(unreachable) Invalid mount point {}", mount_point);
        return ResultFileNotFound;
    case PathParser::PathNotFound:
    case PathParser::NotFound:
        LOG_ERROR(Service_FS, "Path not found {}", full_path);
        return ResultPathNotFound;
    case PathParser::FileInPath:
    case PathParser::FileFound:
        LOG_ERROR(Service_FS, "Unexpected file in path {}", full_path);
        return ResultUnexpectedFileOrDirectory;
    case PathParser::DirectoryFound:
        break; // Expected 'success' case
    }

    return std::make_unique<DiskDirectory>(full_path);
}

u64 SaveDataArchive::GetFreeBytes() const {
    // TODO: Stubbed to return 32MiB
    return 1024 * 1024 * 32;
}

} // namespace FileSys

SERIALIZE_EXPORT_IMPL(FileSys::SaveDataArchive)
SERIALIZE_EXPORT_IMPL(FileSys::SaveDataDelayGenerator)
