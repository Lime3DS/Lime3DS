// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "common/archives.h"
#include "core/core.h"
#include "core/file_sys/archive_savedata.h"
#include "core/hle/kernel/process.h"

SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_SaveData)

namespace FileSys {

ArchiveFactory_SaveData::ArchiveFactory_SaveData(
    std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata)
    : sd_savedata_source(std::move(sd_savedata)) {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SaveData::Open(const Path& path,
                                                                         u64 program_id) {
    return sd_savedata_source->Open(Service::FS::ArchiveIdCode::SaveData, path, program_id);
}

Result ArchiveFactory_SaveData::Format(const Path& path,
                                       const FileSys::ArchiveFormatInfo& format_info,
                                       u64 program_id, u32 directory_buckets, u32 file_buckets) {
    return sd_savedata_source->Format(program_id, format_info, Service::FS::ArchiveIdCode::SaveData,
                                      path, directory_buckets, file_buckets);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SaveData::GetFormatInfo(const Path& path,
                                                                    u64 program_id) const {
    return sd_savedata_source->GetFormatInfo(program_id, Service::FS::ArchiveIdCode::SaveData,
                                             path);
}

} // namespace FileSys
