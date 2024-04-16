// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/file_sys/archive_sdmc.h"

namespace FileSys {

/**
 * Archive backend for SDMC write-only archive.
 * The behaviour of SDMCWriteOnlyArchive is almost the same as SDMCArchive, except for
 *  - OpenDirectory is unsupported;
 *  - OpenFile with read flag is unsupported.
 */
class SDMCWriteOnlyArchive : public SDMCArchive {
public:
    explicit SDMCWriteOnlyArchive(const std::string& mount_point,
                                  std::unique_ptr<DelayGenerator> delay_generator_)
        : SDMCArchive(mount_point, std::move(delay_generator_)) {}

    std::string GetName() const override {
        return "SDMCWriteOnlyArchive: " + mount_point;
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                     const Mode& mode) const override;

    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) const override;

private:
    SDMCWriteOnlyArchive() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<SDMCArchive>(*this);
    }
    friend class boost::serialization::access;
};

/// File system interface to the SDMC write-only archive
class ArchiveFactory_SDMCWriteOnly final : public ArchiveFactory {
public:
    explicit ArchiveFactory_SDMCWriteOnly(const std::string& mount_point);

    /**
     * Initialize the archive.
     * @return true if it initialized successfully
     */
    bool Initialize();

    std::string GetName() const override {
        return "SDMCWriteOnly";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path, u64 program_id) override;
    Result Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info,
                  u64 program_id) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path, u64 program_id) const override;

private:
    std::string sdmc_directory;

    ArchiveFactory_SDMCWriteOnly() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveFactory>(*this);
        ar & sdmc_directory;
    }
    friend class boost::serialization::access;
};

class SDMCWriteOnlyDelayGenerator;

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::SDMCWriteOnlyArchive)
BOOST_CLASS_EXPORT_KEY(FileSys::ArchiveFactory_SDMCWriteOnly)
BOOST_CLASS_EXPORT_KEY(FileSys::SDMCWriteOnlyDelayGenerator)
