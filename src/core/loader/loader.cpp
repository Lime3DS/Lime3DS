// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <string>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/loader/3dsx.h"
#include "core/loader/artic.h"
#include "core/loader/elf.h"
#include "core/loader/ncch.h"

namespace Loader {

FileType IdentifyFile(FileUtil::IOFile& file) {
    FileType type;

#define CHECK_TYPE(loader)                                                                         \
    type = AppLoader_##loader::IdentifyType(file);                                                 \
    if (FileType::Error != type)                                                                   \
        return type;

    CHECK_TYPE(THREEDSX)
    CHECK_TYPE(ELF)
    CHECK_TYPE(NCCH)

#undef CHECK_TYPE

    return FileType::Unknown;
}

FileType IdentifyFile(const std::string& file_name) {
    FileUtil::IOFile file(file_name, "rb");
    if (!file.IsOpen()) {
        LOG_ERROR(Loader, "Failed to load file {}", file_name);
        return FileType::Unknown;
    }

    return IdentifyFile(file);
}

FileType GuessFromExtension(const std::string& extension_) {
    std::string extension = Common::ToLower(extension_);

    if (extension == ".elf" || extension == ".axf")
        return FileType::ELF;

    if (extension == ".cci" || extension == ".3ds")
        return FileType::CCI;

    if (extension == ".cxi" || extension == ".app")
        return FileType::CXI;

    if (extension == ".3dsx")
        return FileType::THREEDSX;

    if (extension == ".cia")
        return FileType::CIA;

    return FileType::Unknown;
}

const char* GetFileTypeString(FileType type) {
    switch (type) {
    case FileType::CCI:
        return "NCSD";
    case FileType::CXI:
        return "NCCH";
    case FileType::CIA:
        return "CIA";
    case FileType::ELF:
        return "ELF";
    case FileType::THREEDSX:
        return "3DSX";
    case FileType::ARTIC:
        return "ARTIC";
    case FileType::Error:
    case FileType::Unknown:
        break;
    }

    return "unknown";
}

/**
 * Get a loader for a file with a specific type
 * @param file The file to load
 * @param type The type of the file
 * @param filename the file name (without path)
 * @param filepath the file full path (with name)
 * @return std::unique_ptr<AppLoader> a pointer to a loader object;  nullptr for unsupported type
 */
static std::unique_ptr<AppLoader> GetFileLoader(Core::System& system, FileUtil::IOFile&& file,
                                                FileType type, const std::string& filename,
                                                const std::string& filepath) {
    switch (type) {

    // 3DSX file format.
    case FileType::THREEDSX:
        return std::make_unique<AppLoader_THREEDSX>(system, std::move(file), filename, filepath);

    // Standard ELF file format.
    case FileType::ELF:
        return std::make_unique<AppLoader_ELF>(system, std::move(file), filename);

    // NCCH/NCSD container formats.
    case FileType::CXI:
    case FileType::CCI:
        return std::make_unique<AppLoader_NCCH>(system, std::move(file), filepath);

    case FileType::ARTIC: {
        auto strToUInt = [](const std::string& str) -> int {
            char* pEnd = NULL;
            unsigned long ul = ::strtoul(str.c_str(), &pEnd, 10);
            if (*pEnd)
                return -1;
            return static_cast<int>(ul);
        };

        u16 port = 5543;
        std::string server_addr = filename;
        auto pos = server_addr.find(":");
        if (pos != server_addr.npos) {
            int newVal = strToUInt(server_addr.substr(pos + 1));
            if (newVal >= 0 && newVal <= 0xFFFF) {
                port = static_cast<u16>(newVal);
                server_addr = server_addr.substr(0, pos);
            }
        }
        return std::make_unique<Apploader_Artic>(system, server_addr, port);
    }

    default:
        return nullptr;
    }
}

std::unique_ptr<AppLoader> GetLoader(const std::string& filename) {
    if (filename.starts_with("articbase://")) {
        return GetFileLoader(Core::System::GetInstance(), FileUtil::IOFile(), FileType::ARTIC,
                             filename.substr(12), "");
    }

    FileUtil::IOFile file(filename, "rb");
    if (!file.IsOpen()) {
        LOG_ERROR(Loader, "Failed to load file {}", filename);
        return nullptr;
    }

    std::string filename_filename, filename_extension;
    Common::SplitPath(filename, nullptr, &filename_filename, &filename_extension);

    FileType type = IdentifyFile(file);
    FileType filename_type = GuessFromExtension(filename_extension);

    if (type != filename_type) {
        LOG_WARNING(Loader, "File {} has a different type than its extension.", filename);
        if (FileType::Unknown == type)
            type = filename_type;
    }

    LOG_DEBUG(Loader, "Loading file {} as {}...", filename, GetFileTypeString(type));

    auto& system = Core::System::GetInstance();
    return GetFileLoader(system, std::move(file), type, filename_filename, filename);
}

} // namespace Loader
