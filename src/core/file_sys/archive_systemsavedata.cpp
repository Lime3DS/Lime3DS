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

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SystemSaveData::Open(const Path& path,
                                                                               u64 program_id) {
    std::string fullpath = GetSystemSaveDataPath(base_path, path);
    if (!FileUtil::Exists(fullpath)) {
        // TODO(Subv): Check error code, this one is probably wrong
        return ResultNotFound;
    }
    return std::make_unique<SaveDataArchive>(fullpath);
}

Result ArchiveFactory_SystemSaveData::Format(const Path& path,
                                             const FileSys::ArchiveFormatInfo& format_info,
                                             u64 program_id, u32 directory_buckets,
                                             u32 file_buckets) {
    std::string fullpath = GetSystemSaveDataPath(base_path, path);
    FileUtil::DeleteDirRecursively(fullpath);
    FileUtil::CreateFullPath(fullpath);
    return ResultSuccess;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SystemSaveData::GetFormatInfo(const Path& path,
                                                                          u64 program_id) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive {}", GetName());
    return ResultUnknown;
}

} // namespace FileSys
