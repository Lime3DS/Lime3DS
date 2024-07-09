// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/file_sys/archive_artic.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/savedata_archive.h"
#include "core/hle/service/fs/archive.h"

SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_SystemSaveData)

namespace FileSys {

std::string GetSystemSaveDataPath(std::string_view mount_point, const Path& path) {
    const std::vector<u8> vec_data = path.AsBinary();
    u32 save_low;
    u32 save_high;
    std::memcpy(&save_low, &vec_data[4], sizeof(u32));
    std::memcpy(&save_high, &vec_data[0], sizeof(u32));
    return fmt::format("{}{:08X}/{:08X}/", mount_point, save_low, save_high);
}

std::string GetSystemSaveDataContainerPath(std::string_view mount_point) {
    return fmt::format("{}data/{}/sysdata/", mount_point, SYSTEM_ID);
}

Path ConstructSystemSaveDataBinaryPath(u32 high, u32 low) {
    std::vector<u8> binary_path;
    binary_path.reserve(8);

    // Append each word byte by byte

    // First is the high word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((high >> (8 * i)) & 0xFF);

    // Next is the low word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((low >> (8 * i)) & 0xFF);

    return {std::move(binary_path)};
}

ArchiveFactory_SystemSaveData::ArchiveFactory_SystemSaveData(const std::string& nand_path)
    : base_path(GetSystemSaveDataContainerPath(nand_path)) {}

static bool AllowArticSystemSaveData(const Path& path) {
    constexpr u32 APP_SYSTEM_SAVE_DATA_MASK = 0x00020000;
    if (path.GetType() == FileSys::LowPathType::Binary) {
        std::vector<u8> path_data = path.AsBinary();
        return path_data.size() == 8 &&
               (*reinterpret_cast<u32*>(path_data.data() + 4) & APP_SYSTEM_SAVE_DATA_MASK) != 0;
    }
    return false;
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SystemSaveData::Open(const Path& path,
                                                                               u64 program_id) {
    if (IsUsingArtic() && AllowArticSystemSaveData(path)) {
        EnsureCacheCreated();
        return ArticArchive::Open(artic_client, Service::FS::ArchiveIdCode::SystemSaveData, path,
                                  Core::PerfStats::PerfArticEventBits::ARTIC_SYSTEM_SAVE_DATA,
                                  *this, false);
    } else {
        std::string fullpath = GetSystemSaveDataPath(base_path, path);
        if (!FileUtil::Exists(fullpath)) {
            // TODO(Subv): Check error code, this one is probably wrong
            return ResultNotFound;
        }
        return std::make_unique<SaveDataArchive>(fullpath);
    }
}

Result ArchiveFactory_SystemSaveData::Format(const Path& path,
                                             const FileSys::ArchiveFormatInfo& format_info,
                                             u64 program_id, u32 directory_buckets,
                                             u32 file_buckets) {
    const std::vector<u8> vec_data = path.AsBinary();
    u32 save_low;
    u32 save_high;
    std::memcpy(&save_low, &vec_data[4], sizeof(u32));
    std::memcpy(&save_high, &vec_data[0], sizeof(u32));
    return FormatAsSysData(save_high, save_low, format_info.total_size, 0x1000,
                           format_info.number_directories, format_info.number_files,
                           directory_buckets, file_buckets, format_info.duplicate_data);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SystemSaveData::GetFormatInfo(const Path& path,
                                                                          u64 program_id) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive {}", GetName());
    return ResultUnknown;
}

Result ArchiveFactory_SystemSaveData::FormatAsSysData(u32 high, u32 low, u32 total_size,
                                                      u32 block_size, u32 number_directories,
                                                      u32 number_files,
                                                      u32 number_directory_buckets,
                                                      u32 number_file_buckets, u8 duplicate_data) {
    if (IsUsingArtic() &&
        AllowArticSystemSaveData(FileSys::ConstructSystemSaveDataBinaryPath(high, low))) {
        auto req = artic_client->NewRequest("FSUSER_CreateSysSaveData");

        req.AddParameterU32(high);
        req.AddParameterU32(low);
        req.AddParameterU32(total_size);
        req.AddParameterU32(block_size);
        req.AddParameterU32(number_directories);
        req.AddParameterU32(number_files);
        req.AddParameterU32(number_directory_buckets);
        req.AddParameterU32(number_file_buckets);
        req.AddParameterU8(duplicate_data);

        auto resp = artic_client->Send(req);
        if (!resp.has_value() || !resp->Succeeded()) {
            return ResultUnknown;
        }

        Result res(static_cast<u32>(resp->GetMethodResult()));
        return res;

    } else {
        // Construct the binary path to the archive first
        const FileSys::Path path = FileSys::ConstructSystemSaveDataBinaryPath(high, low);

        const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
        const std::string base_path = FileSys::GetSystemSaveDataContainerPath(nand_directory);
        const std::string systemsavedata_path = FileSys::GetSystemSaveDataPath(base_path, path);
        if (!FileUtil::CreateFullPath(systemsavedata_path)) {
            return ResultUnknown; // TODO(Subv): Find the right error code
        }
        return ResultSuccess;
    }
}

} // namespace FileSys
