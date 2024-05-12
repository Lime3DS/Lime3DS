// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include <utility>
#include "common/archives.h"
#include "core/file_sys/archive_other_savedata.h"
#include "core/file_sys/errors.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/fs/archive.h"

SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_OtherSaveDataPermitted)
SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_OtherSaveDataGeneral)

namespace FileSys {

// TODO(wwylele): The storage info in exheader should be checked before accessing these archives

using Service::FS::MediaType;

namespace {

template <typename T>
ResultVal<std::tuple<MediaType, u64>> ParsePath(const Path& path, T program_id_reader) {
    if (path.GetType() != LowPathType::Binary) {
        LOG_ERROR(Service_FS, "Wrong path type {}", path.GetType());
        return ResultInvalidPath;
    }

    std::vector<u8> vec_data = path.AsBinary();

    if (vec_data.size() != 12) {
        LOG_ERROR(Service_FS, "Wrong path length {}", vec_data.size());
        return ResultInvalidPath;
    }

    const u32* data = reinterpret_cast<const u32*>(vec_data.data());
    auto media_type = static_cast<MediaType>(data[0]);

    if (media_type != MediaType::SDMC && media_type != MediaType::GameCard) {
        LOG_ERROR(Service_FS, "Unsupported media type {}", media_type);

        // Note: this is strange, but the error code was verified with a real 3DS
        return ResultUnsupportedOpenFlags;
    }

    return std::make_tuple(media_type, program_id_reader(data));
}

ResultVal<std::tuple<MediaType, u64>> ParsePathPermitted(const Path& path) {
    return ParsePath(path,
                     [](const u32* data) -> u64 { return (data[1] << 8) | 0x0004000000000000ULL; });
}

ResultVal<std::tuple<MediaType, u64>> ParsePathGeneral(const Path& path) {
    return ParsePath(
        path, [](const u32* data) -> u64 { return data[1] | (static_cast<u64>(data[2]) << 32); });
}

} // namespace

ArchiveFactory_OtherSaveDataPermitted::ArchiveFactory_OtherSaveDataPermitted(
    std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata)
    : sd_savedata_source(std::move(sd_savedata)) {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_OtherSaveDataPermitted::Open(
    const Path& path, u64 /*client_program_id*/) {
    MediaType media_type;
    u64 program_id;
    CASCADE_RESULT(std::tie(media_type, program_id), ParsePathPermitted(path));

    if (media_type == MediaType::GameCard) {
        LOG_WARNING(Service_FS, "(stubbed) Unimplemented media type GameCard");
        return ResultGamecardNotInserted;
    }

    return sd_savedata_source->Open(Service::FS::ArchiveIdCode::OtherSaveDataPermitted, path,
                                    program_id);
}

Result ArchiveFactory_OtherSaveDataPermitted::Format(const Path& path,
                                                     const FileSys::ArchiveFormatInfo& format_info,
                                                     u64 program_id, u32 directory_buckets,
                                                     u32 file_buckets) {
    LOG_ERROR(Service_FS, "Attempted to format a OtherSaveDataPermitted archive.");
    return ResultInvalidPath;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_OtherSaveDataPermitted::GetFormatInfo(
    const Path& path, u64 /*client_program_id*/) const {
    MediaType media_type;
    u64 program_id;
    CASCADE_RESULT(std::tie(media_type, program_id), ParsePathPermitted(path));

    if (media_type == MediaType::GameCard) {
        LOG_WARNING(Service_FS, "(stubbed) Unimplemented media type GameCard");
        return ResultGamecardNotInserted;
    }

    return sd_savedata_source->GetFormatInfo(
        program_id, Service::FS::ArchiveIdCode::OtherSaveDataPermitted, path);
}

ArchiveFactory_OtherSaveDataGeneral::ArchiveFactory_OtherSaveDataGeneral(
    std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata)
    : sd_savedata_source(std::move(sd_savedata)) {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_OtherSaveDataGeneral::Open(
    const Path& path, u64 /*client_program_id*/) {
    MediaType media_type;
    u64 program_id;
    CASCADE_RESULT(std::tie(media_type, program_id), ParsePathGeneral(path));

    if (media_type == MediaType::GameCard) {
        LOG_WARNING(Service_FS, "(stubbed) Unimplemented media type GameCard");
        return ResultGamecardNotInserted;
    }

    return sd_savedata_source->Open(Service::FS::ArchiveIdCode::OtherSaveDataGeneral, path,
                                    program_id);
}

Result ArchiveFactory_OtherSaveDataGeneral::Format(const Path& path,
                                                   const FileSys::ArchiveFormatInfo& format_info,
                                                   u64 /*client_program_id*/, u32 directory_buckets,
                                                   u32 file_buckets) {
    MediaType media_type;
    u64 program_id;
    CASCADE_RESULT(std::tie(media_type, program_id), ParsePathGeneral(path));

    if (media_type == MediaType::GameCard) {
        LOG_WARNING(Service_FS, "(stubbed) Unimplemented media type GameCard");
        return ResultGamecardNotInserted;
    }

    return sd_savedata_source->Format(program_id, format_info,
                                      Service::FS::ArchiveIdCode::OtherSaveDataPermitted, path,
                                      directory_buckets, file_buckets);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_OtherSaveDataGeneral::GetFormatInfo(
    const Path& path, u64 /*client_program_id*/) const {
    MediaType media_type;
    u64 program_id;
    CASCADE_RESULT(std::tie(media_type, program_id), ParsePathGeneral(path));

    if (media_type == MediaType::GameCard) {
        LOG_WARNING(Service_FS, "(stubbed) Unimplemented media type GameCard");
        return ResultGamecardNotInserted;
    }

    return sd_savedata_source->GetFormatInfo(
        program_id, Service::FS::ArchiveIdCode::OtherSaveDataPermitted, path);
}

} // namespace FileSys
