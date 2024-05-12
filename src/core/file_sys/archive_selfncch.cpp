// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "common/archives.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/archive_selfncch.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ivfc_archive.h"
#include "core/hle/kernel/process.h"

SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_SelfNCCH)

namespace FileSys {

enum class SelfNCCHFilePathType : u32 {
    RomFS = 0,
    Code = 1, // This is not supported by SelfNCCHArchive but by archive 0x2345678E
    ExeFS = 2,
    UpdateRomFS = 5, // This is presumably for accessing the RomFS of the update patch.
};

struct SelfNCCHFilePath {
    enum_le<SelfNCCHFilePathType> type;
    std::array<char, 8> exefs_filename;
};
static_assert(sizeof(SelfNCCHFilePath) == 12, "NCCHFilePath has wrong size!");

// A read-only file created from a block of data. It only allows you to read the entire file at
// once, in a single read operation.
class ExeFSSectionFile final : public FileBackend {
public:
    explicit ExeFSSectionFile(std::shared_ptr<std::vector<u8>> data_) : data(std::move(data_)) {}

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override {
        if (offset != 0) {
            LOG_ERROR(Service_FS, "offset must be zero!");
            return ResultUnsupportedOpenFlags;
        }

        if (length != data->size()) {
            LOG_ERROR(Service_FS, "size must match the file size!");
            return ResultIncorrectExefsReadSize;
        }

        std::memcpy(buffer, data->data(), data->size());
        return data->size();
    }

    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush, bool update_timestamp,
                                 const u8* buffer) override {
        LOG_ERROR(Service_FS, "The file is read-only!");
        return ResultUnsupportedOpenFlags;
    }

    u64 GetSize() const override {
        return data->size();
    }

    bool SetSize(u64 size) const override {
        return false;
    }

    bool Close() override {
        return true;
    }

    void Flush() const override {}

private:
    std::shared_ptr<std::vector<u8>> data;

    ExeFSSectionFile() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<FileBackend>(*this);
        ar & data;
    }
    friend class boost::serialization::access;
};

// SelfNCCHArchive represents the running application itself. From this archive the application can
// open RomFS and ExeFS, excluding the .code section.
class SelfNCCHArchive final : public ArchiveBackend {
public:
    explicit SelfNCCHArchive(const NCCHData& ncch_data_) : ncch_data(ncch_data_) {}

    std::string GetName() const override {
        return "SelfNCCHArchive";
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path, const Mode&,
                                                     u32 attributes) override {
        // Note: SelfNCCHArchive doesn't check the open mode.

        if (path.GetType() != LowPathType::Binary) {
            LOG_ERROR(Service_FS, "Path need to be Binary");
            return ResultInvalidPath;
        }

        std::vector<u8> binary = path.AsBinary();
        if (binary.size() != sizeof(SelfNCCHFilePath)) {
            LOG_ERROR(Service_FS, "Wrong path size {}", binary.size());
            return ResultInvalidPath;
        }

        SelfNCCHFilePath file_path;
        std::memcpy(&file_path, binary.data(), sizeof(SelfNCCHFilePath));

        switch (file_path.type) {
        case SelfNCCHFilePathType::UpdateRomFS:
            return OpenUpdateRomFS();

        case SelfNCCHFilePathType::RomFS:
            return OpenRomFS();

        case SelfNCCHFilePathType::Code:
            LOG_ERROR(Service_FS, "Reading the code section is not supported!");
            return ResultCommandNotAllowed;

        case SelfNCCHFilePathType::ExeFS: {
            const auto& raw = file_path.exefs_filename;
            auto end = std::find(raw.begin(), raw.end(), '\0');
            std::string filename(raw.begin(), end);
            return OpenExeFS(filename);
        }
        default:
            LOG_ERROR(Service_FS, "Unknown file type {}!", file_path.type);
            return ResultInvalidPath;
        }
    }

    Result DeleteFile(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    Result RenameFile(const Path& src_path, const Path& dest_path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    Result DeleteDirectory(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    Result DeleteDirectoryRecursively(const Path& path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    Result CreateFile(const Path& path, u64 size, u32 attributes) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    Result CreateDirectory(const Path& path, u32 attributes) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    Result RenameDirectory(const Path& src_path, const Path& dest_path) const override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) override {
        LOG_ERROR(Service_FS, "Unsupported");
        return ResultUnsupportedOpenFlags;
    }

    u64 GetFreeBytes() const override {
        return 0;
    }

private:
    ResultVal<std::unique_ptr<FileBackend>> OpenRomFS() const {
        if (ncch_data.romfs_file) {
            std::unique_ptr<DelayGenerator> delay_generator =
                std::make_unique<RomFSDelayGenerator>();
            return std::make_unique<IVFCFile>(ncch_data.romfs_file, std::move(delay_generator));
        } else {
            LOG_INFO(Service_FS, "Unable to read RomFS");
            return ResultRomfsNotFound;
        }
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenUpdateRomFS() const {
        if (ncch_data.update_romfs_file) {
            std::unique_ptr<DelayGenerator> delay_generator =
                std::make_unique<RomFSDelayGenerator>();
            return std::make_unique<IVFCFile>(ncch_data.update_romfs_file,
                                              std::move(delay_generator));
        } else {
            LOG_INFO(Service_FS, "Unable to read update RomFS");
            return ResultRomfsNotFound;
        }
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenExeFS(const std::string& filename) const {
        if (filename == "icon") {
            if (ncch_data.icon) {
                return std::make_unique<ExeFSSectionFile>(ncch_data.icon);
            }

            LOG_WARNING(Service_FS, "Unable to read icon");
            return ResultExefsSectionNotFound;
        }

        if (filename == "logo") {
            if (ncch_data.logo) {
                return std::make_unique<ExeFSSectionFile>(ncch_data.logo);
            }

            LOG_WARNING(Service_FS, "Unable to read logo");
            return ResultExefsSectionNotFound;
        }

        if (filename == "banner") {
            if (ncch_data.banner) {
                return std::make_unique<ExeFSSectionFile>(ncch_data.banner);
            }

            LOG_WARNING(Service_FS, "Unable to read banner");
            return ResultExefsSectionNotFound;
        }

        LOG_ERROR(Service_FS, "Unknown ExeFS section {}!", filename);
        return ResultInvalidPath;
    }

    NCCHData ncch_data;

    SelfNCCHArchive() = default;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveBackend>(*this);
        ar & ncch_data;
    }
    friend class boost::serialization::access;
};

void ArchiveFactory_SelfNCCH::Register(Loader::AppLoader& app_loader) {
    u64 program_id = 0;
    if (app_loader.ReadProgramId(program_id) != Loader::ResultStatus::Success) {
        LOG_WARNING(
            Service_FS,
            "Could not read program id when registering with SelfNCCH, this might be a 3dsx file");
    }

    LOG_DEBUG(Service_FS, "Registering program {:016X} with the SelfNCCH archive factory",
              program_id);

    if (ncch_data.find(program_id) != ncch_data.end()) {
        LOG_WARNING(Service_FS,
                    "Registering program {:016X} with SelfNCCH will override existing mapping",
                    program_id);
    }

    NCCHData& data = ncch_data[program_id];

    std::shared_ptr<RomFSReader> romfs_file_;
    if (Loader::ResultStatus::Success == app_loader.ReadRomFS(romfs_file_)) {

        data.romfs_file = std::move(romfs_file_);
    }

    std::shared_ptr<RomFSReader> update_romfs_file;
    if (Loader::ResultStatus::Success == app_loader.ReadUpdateRomFS(update_romfs_file)) {

        data.update_romfs_file = std::move(update_romfs_file);
    }

    std::vector<u8> buffer;

    if (Loader::ResultStatus::Success == app_loader.ReadIcon(buffer))
        data.icon = std::make_shared<std::vector<u8>>(std::move(buffer));

    buffer.clear();
    if (Loader::ResultStatus::Success == app_loader.ReadLogo(buffer))
        data.logo = std::make_shared<std::vector<u8>>(std::move(buffer));

    buffer.clear();
    if (Loader::ResultStatus::Success == app_loader.ReadBanner(buffer))
        data.banner = std::make_shared<std::vector<u8>>(std::move(buffer));
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SelfNCCH::Open(const Path& path,
                                                                         u64 program_id) {
    return std::make_unique<SelfNCCHArchive>(ncch_data[program_id]);
}

Result ArchiveFactory_SelfNCCH::Format(const Path&, const FileSys::ArchiveFormatInfo&,
                                       u64 program_id, u32 directory_buckets, u32 file_buckets) {
    LOG_ERROR(Service_FS, "Attempted to format a SelfNCCH archive.");
    return ResultInvalidPath;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SelfNCCH::GetFormatInfo(const Path&,
                                                                    u64 program_id) const {
    LOG_ERROR(Service_FS, "Attempted to get format info of a SelfNCCH archive");
    return ResultInvalidPath;
}

} // namespace FileSys

SERIALIZE_EXPORT_IMPL(FileSys::ExeFSSectionFile)
SERIALIZE_EXPORT_IMPL(FileSys::SelfNCCHArchive)
