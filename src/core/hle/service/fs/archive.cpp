// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <memory>
#include <system_error>
#include <type_traits>
#include <utility>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/archive_other_savedata.h"
#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/archive_sdmcwriteonly.h"
#include "core/file_sys/archive_selfncch.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/archive.h"

namespace Service::FS {

MediaType GetMediaTypeFromPath(std::string_view path) {
    if (path.rfind(FileUtil::GetUserPath(FileUtil::UserPath::NANDDir), 0) == 0) {
        return MediaType::NAND;
    }
    if (path.rfind(FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), 0) == 0) {
        return MediaType::SDMC;
    }
    return MediaType::GameCard;
}

ArchiveBackend* ArchiveManager::GetArchive(ArchiveHandle handle) {
    auto itr = handle_map.find(handle);
    return (itr == handle_map.end()) ? nullptr : itr->second.get();
}

ResultVal<ArchiveHandle> ArchiveManager::OpenArchive(ArchiveIdCode id_code,
                                                     const FileSys::Path& archive_path,
                                                     u64 program_id) {
    LOG_TRACE(Service_FS, "Opening archive with id code 0x{:08X}", id_code);

    auto itr = id_code_map.find(id_code);
    if (itr == id_code_map.end()) {
        return FileSys::ResultNotFound;
    }

    CASCADE_RESULT(std::unique_ptr<ArchiveBackend> res,
                   itr->second->Open(archive_path, program_id));

    // This should never even happen in the first place with 64-bit handles,
    while (handle_map.count(next_handle) != 0) {
        ++next_handle;
    }
    handle_map.emplace(next_handle, std::move(res));
    return next_handle++;
}

Result ArchiveManager::CloseArchive(ArchiveHandle handle) {
    auto itr = handle_map.find(handle);
    if (itr != handle_map.end()) {
        itr->second->Close();
    } else {
        return FileSys::ResultInvalidArchiveHandle;
    }
    handle_map.erase(itr);
    return ResultSuccess;
}

Result ArchiveManager::ControlArchive(ArchiveHandle handle, u32 action, u8* input,
                                      size_t input_size, u8* output, size_t output_size) {
    auto itr = handle_map.find(handle);
    if (itr != handle_map.end()) {
        return itr->second->Control(action, input, input_size, output, output_size);
    } else {
        return FileSys::ResultInvalidArchiveHandle;
    }
}

// TODO(yuriks): This might be what the fs:REG service is for. See the Register/Unregister calls in
// http://3dbrew.org/wiki/Filesystem_services#ProgramRegistry_service_.22fs:REG.22
Result ArchiveManager::RegisterArchiveType(std::unique_ptr<FileSys::ArchiveFactory>&& factory,
                                           ArchiveIdCode id_code) {
    auto result = id_code_map.emplace(id_code, std::move(factory));

    bool inserted = result.second;
    ASSERT_MSG(inserted, "Tried to register more than one archive with same id code");

    auto& archive = result.first->second;
    LOG_DEBUG(Service_FS, "Registered archive {} with id code 0x{:08X}", archive->GetName(),
              id_code);
    return ResultSuccess;
}

std::pair<ResultVal<std::shared_ptr<File>>, std::chrono::nanoseconds>
ArchiveManager::OpenFileFromArchive(ArchiveHandle archive_handle, const FileSys::Path& path,
                                    const FileSys::Mode mode, u32 attributes) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr) {
        return std::make_pair(FileSys::ResultInvalidArchiveHandle, std::chrono::nanoseconds{0});
    }

    const std::chrono::nanoseconds open_timeout_ns{archive->GetOpenDelayNs()};
    auto backend = archive->OpenFile(path, mode, attributes);
    if (backend.Failed()) {
        return std::make_pair(backend.Code(), open_timeout_ns);
    }

    auto file = std::make_shared<File>(system.Kernel(), std::move(backend).Unwrap(), path);
    return std::make_pair(std::move(file), open_timeout_ns);
}

Result ArchiveManager::DeleteFileFromArchive(ArchiveHandle archive_handle,
                                             const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ResultInvalidArchiveHandle;

    return archive->DeleteFile(path);
}

Result ArchiveManager::RenameFileBetweenArchives(ArchiveHandle src_archive_handle,
                                                 const FileSys::Path& src_path,
                                                 ArchiveHandle dest_archive_handle,
                                                 const FileSys::Path& dest_path) {
    ArchiveBackend* src_archive = GetArchive(src_archive_handle);
    ArchiveBackend* dest_archive = GetArchive(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return FileSys::ResultInvalidArchiveHandle;

    if (src_archive == dest_archive) {
        return src_archive->RenameFile(src_path, dest_path);
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }
}

Result ArchiveManager::DeleteDirectoryFromArchive(ArchiveHandle archive_handle,
                                                  const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ResultInvalidArchiveHandle;

    return archive->DeleteDirectory(path);
}

Result ArchiveManager::DeleteDirectoryRecursivelyFromArchive(ArchiveHandle archive_handle,
                                                             const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ResultInvalidArchiveHandle;

    return archive->DeleteDirectoryRecursively(path);
}

Result ArchiveManager::CreateFileInArchive(ArchiveHandle archive_handle, const FileSys::Path& path,
                                           u64 file_size, u32 attributes) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ResultInvalidArchiveHandle;

    return archive->CreateFile(path, file_size, attributes);
}

Result ArchiveManager::CreateDirectoryFromArchive(ArchiveHandle archive_handle,
                                                  const FileSys::Path& path, u32 attributes) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr)
        return FileSys::ResultInvalidArchiveHandle;

    return archive->CreateDirectory(path, attributes);
}

Result ArchiveManager::RenameDirectoryBetweenArchives(ArchiveHandle src_archive_handle,
                                                      const FileSys::Path& src_path,
                                                      ArchiveHandle dest_archive_handle,
                                                      const FileSys::Path& dest_path) {
    ArchiveBackend* src_archive = GetArchive(src_archive_handle);
    ArchiveBackend* dest_archive = GetArchive(dest_archive_handle);
    if (src_archive == nullptr || dest_archive == nullptr)
        return FileSys::ResultInvalidArchiveHandle;

    if (src_archive == dest_archive) {
        return src_archive->RenameDirectory(src_path, dest_path);
    } else {
        // TODO: Implement renaming across archives
        return UnimplementedFunction(ErrorModule::FS);
    }
}

ResultVal<std::shared_ptr<Directory>> ArchiveManager::OpenDirectoryFromArchive(
    ArchiveHandle archive_handle, const FileSys::Path& path) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr) {
        return FileSys::ResultInvalidArchiveHandle;
    }

    auto backend = archive->OpenDirectory(path);
    if (backend.Failed()) {
        return backend.Code();
    }

    return std::make_shared<Directory>(std::move(backend).Unwrap(), path);
}

ResultVal<u64> ArchiveManager::GetFreeBytesInArchive(ArchiveHandle archive_handle) {
    const ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr) {
        return FileSys::ResultInvalidArchiveHandle;
    }
    return archive->GetFreeBytes();
}

Result ArchiveManager::FormatArchive(ArchiveIdCode id_code,
                                     const FileSys::ArchiveFormatInfo& format_info,
                                     const FileSys::Path& path, u64 program_id,
                                     u32 directory_buckets, u32 file_buckets) {
    auto archive_itr = id_code_map.find(id_code);
    if (archive_itr == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    return archive_itr->second->Format(path, format_info, program_id, directory_buckets,
                                       file_buckets);
}

ResultVal<FileSys::ArchiveFormatInfo> ArchiveManager::GetArchiveFormatInfo(
    ArchiveIdCode id_code, const FileSys::Path& archive_path, u64 program_id) {
    auto archive = id_code_map.find(id_code);
    if (archive == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    return archive->second->GetFormatInfo(archive_path, program_id);
}

Result ArchiveManager::CreateExtSaveData(MediaType media_type, u8 unknown, u32 high, u32 low,
                                         std::span<const u8> smdh_icon,
                                         const FileSys::ArchiveFormatInfo& format_info,
                                         u64 program_id, u64 total_size) {
    // Construct the binary path to the archive first
    FileSys::Path path =
        FileSys::ConstructExtDataBinaryPath(static_cast<u32>(media_type), high, low);

    auto archive = id_code_map.find(media_type == MediaType::NAND ? ArchiveIdCode::SharedExtSaveData
                                                                  : ArchiveIdCode::ExtSaveData);

    if (archive == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    auto ext_savedata = static_cast<FileSys::ArchiveFactory_ExtSaveData*>(archive->second.get());

    Result result = ext_savedata->FormatAsExtData(path, format_info, unknown, program_id,
                                                  total_size, smdh_icon);
    if (result.IsError()) {
        return result;
    }

    return ResultSuccess;
}

Result ArchiveManager::DeleteExtSaveData(MediaType media_type, u8 unknown, u32 high, u32 low) {
    auto archive = id_code_map.find(media_type == MediaType::NAND ? ArchiveIdCode::SharedExtSaveData
                                                                  : ArchiveIdCode::ExtSaveData);

    if (archive == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    auto ext_savedata = static_cast<FileSys::ArchiveFactory_ExtSaveData*>(archive->second.get());

    return ext_savedata->DeleteExtData(media_type, unknown, high, low);
}

Result ArchiveManager::DeleteSystemSaveData(u32 high, u32 low) {
    // Construct the binary path to the archive first
    const FileSys::Path path = FileSys::ConstructSystemSaveDataBinaryPath(high, low);

    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    const std::string base_path = FileSys::GetSystemSaveDataContainerPath(nand_directory);
    const std::string systemsavedata_path = FileSys::GetSystemSaveDataPath(base_path, path);
    if (!FileUtil::DeleteDirRecursively(systemsavedata_path)) {
        return ResultUnknown; // TODO(Subv): Find the right error code
    }

    return ResultSuccess;
}

Result ArchiveManager::CreateSystemSaveData(u32 high, u32 low, u32 total_size, u32 block_size,
                                            u32 number_directories, u32 number_files,
                                            u32 number_directory_buckets, u32 number_file_buckets,
                                            u8 duplicate_data) {

    auto archive = id_code_map.find(ArchiveIdCode::SystemSaveData);

    if (archive == id_code_map.end()) {
        return UnimplementedFunction(ErrorModule::FS); // TODO(Subv): Find the right error
    }

    auto sys_savedata = static_cast<FileSys::ArchiveFactory_SystemSaveData*>(archive->second.get());

    return sys_savedata->FormatAsSysData(high, low, total_size, block_size, number_directories,
                                         number_files, number_directory_buckets,
                                         number_file_buckets, duplicate_data);
}

ResultVal<ArchiveResource> ArchiveManager::GetArchiveResource(MediaType media_type) const {
    // TODO(Subv): Implement querying the actual size information for these storages.
    ArchiveResource resource{};
    resource.sector_size_in_bytes = 512;
    resource.cluster_size_in_bytes = 16384;
    resource.partition_capacity_in_clusters = 0x80000; // 8GiB capacity
    resource.free_space_in_clusters = 0x80000;         // 8GiB free
    return resource;
}

Result ArchiveManager::SetSaveDataSecureValue(ArchiveHandle archive_handle, u32 secure_value_slot,
                                              u64 secure_value, bool flush) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr) {
        return FileSys::ResultInvalidArchiveHandle;
    }
    return archive->SetSaveDataSecureValue(secure_value_slot, secure_value, flush);
}

ResultVal<std::tuple<bool, bool, u64>> ArchiveManager::GetSaveDataSecureValue(
    ArchiveHandle archive_handle, u32 secure_value_slot) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr) {
        return FileSys::ResultInvalidArchiveHandle;
    }
    return archive->GetSaveDataSecureValue(secure_value_slot);
}

void ArchiveManager::RegisterArchiveTypes() {
    // TODO(Subv): Add the other archive types (see here for the known types:
    // http://3dbrew.org/wiki/FS:OpenArchive#Archive_idcodes).

    std::string sdmc_directory = FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir);
    std::string nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    auto sdmc_factory = std::make_unique<FileSys::ArchiveFactory_SDMC>(sdmc_directory);
    if (sdmc_factory->Initialize())
        RegisterArchiveType(std::move(sdmc_factory), ArchiveIdCode::SDMC);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMC archive with path {}", sdmc_directory);

    auto sdmcwo_factory = std::make_unique<FileSys::ArchiveFactory_SDMCWriteOnly>(sdmc_directory);
    if (sdmcwo_factory->Initialize())
        RegisterArchiveType(std::move(sdmcwo_factory), ArchiveIdCode::SDMCWriteOnly);
    else
        LOG_ERROR(Service_FS, "Can't instantiate SDMCWriteOnly archive with path {}",
                  sdmc_directory);

    // Create the SaveData archive
    sd_savedata_source = std::make_shared<FileSys::ArchiveSource_SDSaveData>(sdmc_directory);
    auto savedata_factory = std::make_unique<FileSys::ArchiveFactory_SaveData>(sd_savedata_source);
    RegisterArchiveType(std::move(savedata_factory), ArchiveIdCode::SaveData);
    auto other_savedata_permitted_factory =
        std::make_unique<FileSys::ArchiveFactory_OtherSaveDataPermitted>(sd_savedata_source);
    RegisterArchiveType(std::move(other_savedata_permitted_factory),
                        ArchiveIdCode::OtherSaveDataPermitted);
    auto other_savedata_general_factory =
        std::make_unique<FileSys::ArchiveFactory_OtherSaveDataGeneral>(sd_savedata_source);
    RegisterArchiveType(std::move(other_savedata_general_factory),
                        ArchiveIdCode::OtherSaveDataGeneral);

    auto extsavedata_factory = std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(
        sdmc_directory, FileSys::ExtSaveDataType::Normal);
    RegisterArchiveType(std::move(extsavedata_factory), ArchiveIdCode::ExtSaveData);

    auto sharedextsavedata_factory = std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(
        nand_directory, FileSys::ExtSaveDataType::Shared);
    RegisterArchiveType(std::move(sharedextsavedata_factory), ArchiveIdCode::SharedExtSaveData);

    auto bossextsavedata_factory = std::make_unique<FileSys::ArchiveFactory_ExtSaveData>(
        sdmc_directory, FileSys::ExtSaveDataType::Boss);
    RegisterArchiveType(std::move(bossextsavedata_factory), ArchiveIdCode::BossExtSaveData);

    // Create the NCCH archive, basically a small variation of the RomFS archive
    auto savedatacheck_factory = std::make_unique<FileSys::ArchiveFactory_NCCH>();
    RegisterArchiveType(std::move(savedatacheck_factory), ArchiveIdCode::NCCH);

    auto systemsavedata_factory =
        std::make_unique<FileSys::ArchiveFactory_SystemSaveData>(nand_directory);
    RegisterArchiveType(std::move(systemsavedata_factory), ArchiveIdCode::SystemSaveData);

    auto selfncch_factory = std::make_unique<FileSys::ArchiveFactory_SelfNCCH>();
    RegisterArchiveType(std::move(selfncch_factory), ArchiveIdCode::SelfNCCH);
}

bool ArchiveManager::ArchiveIsSlow(ArchiveIdCode archive_id) {
    auto itr = id_code_map.find(archive_id);
    if (itr == id_code_map.end() || itr->second.get() == nullptr) {
        return false;
    }

    return itr->second->IsSlow();
}

bool ArchiveManager::ArchiveIsSlow(ArchiveHandle archive_handle) {
    ArchiveBackend* archive = GetArchive(archive_handle);
    if (archive == nullptr) {
        return false;
    }
    return archive->IsSlow();
}

void ArchiveManager::RegisterSelfNCCH(Loader::AppLoader& app_loader) {
    auto itr = id_code_map.find(ArchiveIdCode::SelfNCCH);
    if (itr == id_code_map.end()) {
        LOG_ERROR(Service_FS,
                  "Could not register a new NCCH because the SelfNCCH archive hasn't been created");
        return;
    }

    auto* factory = static_cast<FileSys::ArchiveFactory_SelfNCCH*>(itr->second.get());
    factory->Register(app_loader);
}

void ArchiveManager::RegisterArticSaveDataSource(
    std::shared_ptr<Network::ArticBase::Client>& client) {
    if (!sd_savedata_source.get()) {
        LOG_ERROR(Service_FS, "Could not register artic save data source.");
        return;
    }
    sd_savedata_source->RegisterArtic(client);
}

void ArchiveManager::RegisterArticExtData(std::shared_ptr<Network::ArticBase::Client>& client) {
    for (auto it : {ArchiveIdCode::ExtSaveData, ArchiveIdCode::SharedExtSaveData,
                    ArchiveIdCode::BossExtSaveData}) {
        auto itr = id_code_map.find(it);
        if (itr == id_code_map.end() || itr->second.get() == nullptr) {
            continue;
        }
        reinterpret_cast<FileSys::ArchiveFactory_ExtSaveData*>(itr->second.get())
            ->RegisterArtic(client);
    }
}

void ArchiveManager::RegisterArticNCCH(std::shared_ptr<Network::ArticBase::Client>& client) {
    auto itr = id_code_map.find(ArchiveIdCode::NCCH);
    if (itr == id_code_map.end() || itr->second.get() == nullptr) {
        return;
    }
    reinterpret_cast<FileSys::ArchiveFactory_NCCH*>(itr->second.get())->RegisterArtic(client);
}

void ArchiveManager::RegisterArticSystemSaveData(
    std::shared_ptr<Network::ArticBase::Client>& client) {
    auto itr = id_code_map.find(ArchiveIdCode::SystemSaveData);
    if (itr == id_code_map.end() || itr->second.get() == nullptr) {
        return;
    }
    reinterpret_cast<FileSys::ArchiveFactory_SystemSaveData*>(itr->second.get())
        ->RegisterArtic(client);
}

ArchiveManager::ArchiveManager(Core::System& system) : system(system) {
    RegisterArchiveTypes();
}

} // namespace Service::FS
