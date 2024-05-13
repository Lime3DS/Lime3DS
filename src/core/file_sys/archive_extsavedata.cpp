// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/archive_artic.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/path_parser.h"
#include "core/file_sys/savedata_archive.h"
#include "core/hle/service/fs/archive.h"

SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_ExtSaveData)

namespace FileSys {

/**
 * A modified version of DiskFile for fixed-size file used by ExtSaveData
 * The file size can't be changed by SetSize or Write.
 */
class FixSizeDiskFile : public DiskFile {
public:
    FixSizeDiskFile(FileUtil::IOFile&& file, const Mode& mode,
                    std::unique_ptr<DelayGenerator> delay_generator_)
        : DiskFile(std::move(file), mode, std::move(delay_generator_)) {
        size = GetSize();
    }

    bool SetSize(u64 size) const override {
        return false;
    }

    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush, bool update_timestamp,
                                 const u8* buffer) override {
        if (offset > size) {
            return ResultWriteBeyondEnd;
        } else if (offset == size) {
            return 0ULL;
        }

        if (offset + length > size) {
            length = size - offset;
        }

        return DiskFile::Write(offset, length, flush, update_timestamp, buffer);
    }

private:
    u64 size{};
};

class ExtSaveDataDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // This is the delay measured for a savedata read,
        // not for extsaveData
        // For now we will take that
        static constexpr u64 slope(183);
        static constexpr u64 offset(524879);
        static constexpr u64 minimum(631826);
        u64 ipc_delay_nanoseconds =
            std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return ipc_delay_nanoseconds;
    }

    u64 GetOpenDelayNs() override {
        // This is the delay measured on N3DS with
        // https://gist.github.com/FearlessTobi/929b68489f4abb2c6cf81d56970a20b4
        // from the results the average of each length was taken.
        static constexpr u64 IPCDelayNanoseconds(3085068);
        return IPCDelayNanoseconds;
    }

    SERIALIZE_DELAY_GENERATOR
};

/**
 * Archive backend for general extsave data archive type.
 * The behaviour of ExtSaveDataArchive is almost the same as SaveDataArchive, except for
 *  - file size can't be changed once created (thus creating zero-size file and openning with create
 *    flag are prohibited);
 *  - always open a file with read+write permission.
 */
class ExtSaveDataArchive : public SaveDataArchive {
public:
    explicit ExtSaveDataArchive(const std::string& mount_point,
                                std::unique_ptr<DelayGenerator> delay_generator_)
        : SaveDataArchive(mount_point, false) {
        delay_generator = std::move(delay_generator_);
    }

    std::string GetName() const override {
        return "ExtSaveDataArchive: " + mount_point;
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path, const Mode& mode,
                                                     u32 attributes) override {
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

        if (mode.create_flag) {
            LOG_ERROR(Service_FS, "Create flag is not supported");
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
            LOG_ERROR(Service_FS, "{} not found", full_path);
            return ResultFileNotFound;
        case PathParser::FileFound:
            break; // Expected 'success' case
        }

        FileUtil::IOFile file(full_path, "r+b");
        if (!file.IsOpen()) {
            LOG_CRITICAL(Service_FS, "(unreachable) Unknown error opening {}", full_path);
            return ResultFileNotFound;
        }

        Mode rwmode;
        rwmode.write_flag.Assign(1);
        rwmode.read_flag.Assign(1);
        auto delay_generator = std::make_unique<ExtSaveDataDelayGenerator>();
        return std::make_unique<FixSizeDiskFile>(std::move(file), rwmode,
                                                 std::move(delay_generator));
    }

private:
    ExtSaveDataArchive() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<SaveDataArchive>(*this);
    }
    friend class boost::serialization::access;
};

struct ExtSaveDataArchivePath {
    u32_le media_type;
    u32_le save_low;
    u32_le save_high;
};

static_assert(sizeof(ExtSaveDataArchivePath) == 12, "Incorrect path size");

std::string GetExtSaveDataPath(std::string_view mount_point, const Path& path) {
    std::vector<u8> vec_data = path.AsBinary();

    ExtSaveDataArchivePath path_data;
    std::memcpy(&path_data, vec_data.data(), sizeof(path_data));

    return fmt::format("{}{:08X}/{:08X}/", mount_point, path_data.save_high, path_data.save_low);
}

std::string GetExtDataContainerPath(std::string_view mount_point, bool shared) {
    if (shared) {
        return fmt::format("{}data/{}/extdata/", mount_point, SYSTEM_ID);
    }
    return fmt::format("{}Nintendo 3DS/{}/{}/extdata/", mount_point, SYSTEM_ID, SDCARD_ID);
}

std::string GetExtDataPathFromId(std::string_view mount_point, u64 extdata_id) {
    const u32 high = static_cast<u32>(extdata_id >> 32);
    const u32 low = static_cast<u32>(extdata_id & 0xFFFFFFFF);

    return fmt::format("{}{:08x}/{:08x}/", GetExtDataContainerPath(mount_point, false), high, low);
}

Path ConstructExtDataBinaryPath(u32 media_type, u32 high, u32 low) {
    ExtSaveDataArchivePath path;
    path.media_type = media_type;
    path.save_high = high;
    path.save_low = low;

    std::vector<u8> binary_path(sizeof(path));
    std::memcpy(binary_path.data(), &path, binary_path.size());

    return {binary_path};
}

ArchiveFactory_ExtSaveData::ArchiveFactory_ExtSaveData(const std::string& mount_location,
                                                       ExtSaveDataType type_)
    : type(type_),
      mount_point(GetExtDataContainerPath(mount_location, type_ == ExtSaveDataType::Shared)) {
    LOG_DEBUG(Service_FS, "Directory {} set as base for ExtSaveData.", mount_point);
}

Path ArchiveFactory_ExtSaveData::GetCorrectedPath(const Path& path) {
    if (type != ExtSaveDataType::Shared) {
        return path;
    }

    static constexpr u32 SharedExtDataHigh = 0x48000;

    ExtSaveDataArchivePath new_path;
    std::memcpy(&new_path, path.AsBinary().data(), sizeof(new_path));

    // The FS module overwrites the high value of the saveid when dealing with the SharedExtSaveData
    // archive.
    new_path.save_high = SharedExtDataHigh;

    std::vector<u8> binary_data(sizeof(new_path));
    std::memcpy(binary_data.data(), &new_path, binary_data.size());

    return {binary_data};
}

static Service::FS::ArchiveIdCode ExtSaveDataTypeToArchiveID(ExtSaveDataType type) {
    switch (type) {
    case FileSys::ExtSaveDataType::Normal:
        return Service::FS::ArchiveIdCode::ExtSaveData;
    case FileSys::ExtSaveDataType::Shared:
        return Service::FS::ArchiveIdCode::SharedExtSaveData;
    case FileSys::ExtSaveDataType::Boss:
        return Service::FS::ArchiveIdCode::BossExtSaveData;
    default:
        return Service::FS::ArchiveIdCode::ExtSaveData;
    }
}

static Core::PerfStats::PerfArticEventBits ExtSaveDataTypeToPerfArtic(ExtSaveDataType type) {
    switch (type) {
    case FileSys::ExtSaveDataType::Normal:
        return Core::PerfStats::PerfArticEventBits::ARTIC_EXT_DATA;
    case FileSys::ExtSaveDataType::Shared:
        return Core::PerfStats::PerfArticEventBits::ARTIC_SHARED_EXT_DATA;
    case FileSys::ExtSaveDataType::Boss:
        return Core::PerfStats::PerfArticEventBits::ARTIC_BOSS_EXT_DATA;
    default:
        return Core::PerfStats::PerfArticEventBits::ARTIC_EXT_DATA;
    }
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_ExtSaveData::Open(const Path& path,
                                                                            u64 program_id) {
    if (IsUsingArtic()) {
        EnsureCacheCreated();
        return ArticArchive::Open(artic_client, ExtSaveDataTypeToArchiveID(type), path,
                                  ExtSaveDataTypeToPerfArtic(type), *this,
                                  type != FileSys::ExtSaveDataType::Normal);
    } else {
        const auto directory = type == ExtSaveDataType::Boss ? "boss/" : "user/";
        const auto fullpath = GetExtSaveDataPath(mount_point, GetCorrectedPath(path)) + directory;
        if (!FileUtil::Exists(fullpath)) {
            // TODO(Subv): Verify the archive behavior of SharedExtSaveData compared to ExtSaveData.
            // ExtSaveData seems to return FS_NotFound (120) when the archive doesn't exist.
            if (type != ExtSaveDataType::Shared) {
                return ResultNotFoundInvalidState;
            } else {
                return ResultNotFormatted;
            }
        }
        std::unique_ptr<DelayGenerator> delay_generator =
            std::make_unique<ExtSaveDataDelayGenerator>();
        return std::make_unique<ExtSaveDataArchive>(fullpath, std::move(delay_generator));
    }
}

Result ArchiveFactory_ExtSaveData::FormatAsExtData(const Path& path,
                                                   const FileSys::ArchiveFormatInfo& format_info,
                                                   u8 unknown, u64 program_id, u64 total_size,
                                                   std::optional<std::span<const u8>> icon) {
    if (IsUsingArtic()) {
        if (!icon.has_value()) {
            LOG_ERROR(Service_FS, "No icon provided while using Artic Base");
            return ResultUnknown;
        }

        ExtSaveDataArchivePath path_data;
        std::memcpy(&path_data, path.AsBinary().data(), sizeof(path_data));

        Service::FS::ExtSaveDataInfo artic_extdata_path;

        artic_extdata_path.media_type = static_cast<u8>(path_data.media_type);
        artic_extdata_path.unknown = unknown;
        artic_extdata_path.save_id_low = path_data.save_low;
        artic_extdata_path.save_id_high = path_data.save_high;

        auto req = artic_client->NewRequest("FSUSER_CreateExtSaveData");

        req.AddParameterBuffer(&artic_extdata_path, sizeof(artic_extdata_path));
        req.AddParameterU32(format_info.number_directories);
        req.AddParameterU32(format_info.number_files);
        req.AddParameterU64(total_size);
        req.AddParameterBuffer(icon->data(), icon->size());

        return ArticArchive::RespResult(artic_client->Send(req));
    } else {
        auto corrected_path = GetCorrectedPath(path);

        // These folders are always created with the ExtSaveData
        std::string user_path = GetExtSaveDataPath(mount_point, corrected_path) + "user/";
        std::string boss_path = GetExtSaveDataPath(mount_point, corrected_path) + "boss/";
        FileUtil::CreateFullPath(user_path);
        FileUtil::CreateFullPath(boss_path);

        // Write the format metadata
        std::string metadata_path = GetExtSaveDataPath(mount_point, corrected_path) + "metadata";
        FileUtil::IOFile file(metadata_path, "wb");

        if (!file.IsOpen()) {
            // TODO(Subv): Find the correct error code
            return ResultUnknown;
        }

        file.WriteBytes(&format_info, sizeof(format_info));

        if (icon.has_value()) {
            FileUtil::IOFile icon_file(FileSys::GetExtSaveDataPath(GetMountPoint(), path) + "icon",
                                       "wb");
            icon_file.WriteBytes(icon->data(), icon->size());
        }
        return ResultSuccess;
    }
}

Result ArchiveFactory_ExtSaveData::DeleteExtData(Service::FS::MediaType media_type, u8 unknown,
                                                 u32 high, u32 low) {
    if (IsUsingArtic()) {
        Service::FS::ExtSaveDataInfo artic_extdata_path;

        artic_extdata_path.media_type = static_cast<u8>(media_type);
        artic_extdata_path.unknown = unknown;
        artic_extdata_path.save_id_low = low;
        artic_extdata_path.save_id_high = high;

        auto req = artic_client->NewRequest("FSUSER_DeleteExtSaveData");

        req.AddParameterBuffer(&artic_extdata_path, sizeof(artic_extdata_path));

        return ArticArchive::RespResult(artic_client->Send(req));
    } else {
        // Construct the binary path to the archive first
        FileSys::Path path =
            FileSys::ConstructExtDataBinaryPath(static_cast<u32>(media_type), high, low);

        std::string media_type_directory;
        if (media_type == Service::FS::MediaType::NAND) {
            media_type_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
        } else if (media_type == Service::FS::MediaType::SDMC) {
            media_type_directory = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
        } else {
            LOG_ERROR(Service_FS, "Unsupported media type {}", media_type);
            return ResultUnknown; // TODO(Subv): Find the right error code
        }

        // Delete all directories (/user, /boss) and the icon file.
        std::string base_path = FileSys::GetExtDataContainerPath(
            media_type_directory, media_type == Service::FS::MediaType::NAND);
        std::string extsavedata_path = FileSys::GetExtSaveDataPath(base_path, path);
        if (FileUtil::Exists(extsavedata_path) && !FileUtil::DeleteDirRecursively(extsavedata_path))
            return ResultUnknown; // TODO(Subv): Find the right error code
        return ResultSuccess;
    }
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_ExtSaveData::GetFormatInfo(const Path& path,
                                                                       u64 program_id) const {
    if (IsUsingArtic()) {
        auto req = artic_client->NewRequest("FSUSER_GetFormatInfo");

        req.AddParameterS32(static_cast<u32>(ExtSaveDataTypeToArchiveID(type)));
        auto path_artic = ArticArchive::BuildFSPath(path);
        req.AddParameterBuffer(path_artic.data(), path_artic.size());

        auto resp = artic_client->Send(req);
        Result res = ArticArchive::RespResult(resp);
        if (R_FAILED(res)) {
            return res;
        }

        auto info_buf = resp->GetResponseBuffer(0);
        if (!info_buf.has_value() || info_buf->second != sizeof(ArchiveFormatInfo)) {
            return ResultUnknown;
        }

        ArchiveFormatInfo info;
        memcpy(&info, info_buf->first, sizeof(info));
        return info;
    } else {
        std::string metadata_path = GetExtSaveDataPath(mount_point, path) + "metadata";
        FileUtil::IOFile file(metadata_path, "rb");

        if (!file.IsOpen()) {
            LOG_ERROR(Service_FS, "Could not open metadata information for archive");
            // TODO(Subv): Verify error code
            return ResultNotFormatted;
        }

        ArchiveFormatInfo info = {};
        file.ReadBytes(&info, sizeof(info));
        return info;
    }
}
} // namespace FileSys

SERIALIZE_EXPORT_IMPL(FileSys::ExtSaveDataDelayGenerator)
SERIALIZE_EXPORT_IMPL(FileSys::ExtSaveDataArchive)
