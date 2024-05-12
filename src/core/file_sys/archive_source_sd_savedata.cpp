// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "common/archives.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/archive_artic.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/savedata_archive.h"
#include "core/hle/service/fs/archive.h"

SERIALIZE_EXPORT_IMPL(FileSys::ArchiveSource_SDSaveData)

namespace FileSys {

namespace {

std::string GetSaveDataContainerPath(const std::string& sdmc_directory) {
    return fmt::format("{}Nintendo 3DS/{}/{}/title/", sdmc_directory, SYSTEM_ID, SDCARD_ID);
}

std::string GetSaveDataPath(const std::string& mount_location, u64 program_id) {
    u32 high = static_cast<u32>(program_id >> 32);
    u32 low = static_cast<u32>(program_id & 0xFFFFFFFF);
    return fmt::format("{}{:08x}/{:08x}/data/00000001/", mount_location, high, low);
}

std::string GetSaveDataMetadataPath(const std::string& mount_location, u64 program_id) {
    u32 high = static_cast<u32>(program_id >> 32);
    u32 low = static_cast<u32>(program_id & 0xFFFFFFFF);
    return fmt::format("{}{:08x}/{:08x}/data/00000001.metadata", mount_location, high, low);
}

} // namespace

ArchiveSource_SDSaveData::ArchiveSource_SDSaveData(const std::string& sdmc_directory)
    : mount_point(GetSaveDataContainerPath(sdmc_directory)) {
    LOG_DEBUG(Service_FS, "Directory {} set as SaveData.", mount_point);
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveSource_SDSaveData::Open(
    Service::FS::ArchiveIdCode archive_id, const Path& path, u64 program_id) {
    if (IsUsingArtic()) {
        EnsureCacheCreated();
        return ArticArchive::Open(artic_client, archive_id, path,
                                  Core::PerfStats::PerfArticEventBits::ARTIC_SAVE_DATA, *this,
                                  archive_id != Service::FS::ArchiveIdCode::SaveData);
    } else {
        std::string concrete_mount_point = GetSaveDataPath(mount_point, program_id);
        if (!FileUtil::Exists(concrete_mount_point)) {
            // When a SaveData archive is created for the first time, it is not yet formatted and
            // the save file/directory structure expected by the game has not yet been initialized.
            // Returning the NotFormatted error code will signal the game to provision the SaveData
            // archive with the files and folders that it expects.
            return ResultNotFormatted;
        }

        return std::make_unique<SaveDataArchive>(std::move(concrete_mount_point));
    }
}

Result ArchiveSource_SDSaveData::Format(u64 program_id,
                                        const FileSys::ArchiveFormatInfo& format_info,
                                        Service::FS::ArchiveIdCode archive_id, const Path& path,
                                        u32 directory_buckets, u32 file_buckets) {
    if (IsUsingArtic()) {
        ClearAllCache();
        auto req = artic_client->NewRequest("FSUSER_FormatSaveData");

        req.AddParameterS32(static_cast<u32>(archive_id));
        auto artic_path = ArticArchive::BuildFSPath(path);
        req.AddParameterBuffer(artic_path.data(), artic_path.size());
        req.AddParameterU32(format_info.total_size / 512);
        req.AddParameterU32(format_info.number_directories);
        req.AddParameterU32(format_info.number_files);
        req.AddParameterU32(directory_buckets);
        req.AddParameterU32(file_buckets);
        req.AddParameterU8(format_info.duplicate_data);

        auto resp = artic_client->Send(req);
        return ArticArchive::RespResult(resp);
    } else {
        std::string concrete_mount_point = GetSaveDataPath(mount_point, program_id);
        FileUtil::DeleteDirRecursively(concrete_mount_point);
        FileUtil::CreateFullPath(concrete_mount_point);

        // Write the format metadata
        std::string metadata_path = GetSaveDataMetadataPath(mount_point, program_id);
        FileUtil::IOFile file(metadata_path, "wb");

        if (file.IsOpen()) {
            file.WriteBytes(&format_info, sizeof(format_info));
            return ResultSuccess;
        }
        return ResultSuccess;
    }
}

ResultVal<ArchiveFormatInfo> ArchiveSource_SDSaveData::GetFormatInfo(
    u64 program_id, Service::FS::ArchiveIdCode archive_id, const Path& path) const {
    if (IsUsingArtic()) {
        auto req = artic_client->NewRequest("FSUSER_GetFormatInfo");

        req.AddParameterS32(static_cast<u32>(archive_id));
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
        std::string metadata_path = GetSaveDataMetadataPath(mount_point, program_id);
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

std::string ArchiveSource_SDSaveData::GetSaveDataPathFor(const std::string& mount_point,
                                                         u64 program_id) {
    return GetSaveDataPath(GetSaveDataContainerPath(mount_point), program_id);
}

} // namespace FileSys
