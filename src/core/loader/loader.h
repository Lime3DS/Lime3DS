// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/file_sys/romfs_reader.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {
struct AddressMapping;
class Process;
} // namespace Kernel

namespace Loader {

/// File types supported by CTR
enum class FileType {
    Error,
    Unknown,
    CCI,
    CXI,
    CIA,
    ELF,
    THREEDSX, // 3DSX
    ARTIC,
};

/**
 * Identifies the type of a bootable file based on the magic value in its header.
 * @param file open file
 * @return FileType of file
 */
FileType IdentifyFile(FileUtil::IOFile& file);

/**
 * Identifies the type of a bootable file based on the magic value in its header.
 * @param file_name path to file
 * @return FileType of file. Note: this will return FileType::Unknown if it is unable to determine
 * a filetype, and will never return FileType::Error.
 */
FileType IdentifyFile(const std::string& file_name);

/**
 * Guess the type of a bootable file from its extension
 * @param extension String extension of bootable file
 * @return FileType of file. Note: this will return FileType::Unknown if it is unable to determine
 * a filetype, and will never return FileType::Error.
 */
FileType GuessFromExtension(const std::string& extension);

/**
 * Convert a FileType into a string which can be displayed to the user.
 */
const char* GetFileTypeString(FileType type);

/// Return type for functions in Loader namespace
enum class ResultStatus {
    Success,
    Error,
    ErrorInvalidFormat,
    ErrorNotImplemented,
    ErrorNotLoaded,
    ErrorNotUsed,
    ErrorAlreadyLoaded,
    ErrorMemoryAllocationFailed,
    ErrorEncrypted,
    ErrorGbaTitle,
    ErrorArtic,
};

constexpr u32 MakeMagic(char a, char b, char c, char d) {
    return a | b << 8 | c << 16 | d << 24;
}

/// Interface for loading an application
class AppLoader : NonCopyable {
public:
    explicit AppLoader(Core::System& system_, FileUtil::IOFile&& file)
        : system(system_), file(std::move(file)) {}
    virtual ~AppLoader() {}

    /**
     * Returns the type of this file
     * @return FileType corresponding to the loaded file
     */
    virtual FileType GetFileType() = 0;

    /**
     * Returns the preferred region codes of this file
     * @return A vector of the preferred region codes
     */
    [[nodiscard]] virtual std::span<const u32> GetPreferredRegions() const {
        return {};
    }

    /**
     * Load the application and return the created Process instance
     * @param process The newly created process.
     * @return The status result of the operation.
     */
    virtual ResultStatus Load(std::shared_ptr<Kernel::Process>& process) = 0;

    /**
     * Loads the core version (FIRM title ID low) that this application needs.
     * This function defaults to 0x2 (NATIVE_FIRM) if it can't read the
     * information.
     * @returns A pair with the optional core version, and the status.
     */
    virtual std::pair<std::optional<u32>, ResultStatus> LoadCoreVersion() {
        return std::make_pair(0x2, ResultStatus::Success);
    }

    /**
     * Forces the application memory mode to the specified value,
     * overriding the memory mode specified in the metadata.
     */
    void SetKernelMemoryModeOverride(Kernel::MemoryMode mem_override) {
        memory_mode_override = mem_override;
    }

    /**
     * Loads the memory mode that this application needs.
     * This function defaults to Dev1 (96MB allocated to the application) if it can't read the
     * information.
     * @returns A pair with the optional memory mode, and the status.
     */
    virtual std::pair<std::optional<Kernel::MemoryMode>, ResultStatus> LoadKernelMemoryMode() {
        if (memory_mode_override.has_value()) {
            return std::make_pair(*memory_mode_override, ResultStatus::Success);
        }
        // 96MB allocated to the application.
        return std::make_pair(Kernel::MemoryMode::Dev1, ResultStatus::Success);
    }

    /**
     * Loads the N3DS hardware capabilities that this application uses.
     * It defaults to all disabled (O3DS) if it can't read the information.
     * @returns A pair with the optional N3DS hardware capabilities, and the status.
     */
    virtual std::pair<std::optional<Kernel::New3dsHwCapabilities>, ResultStatus>
    LoadNew3dsHwCapabilities() {
        return std::make_pair(
            Kernel::New3dsHwCapabilities{false, false, Kernel::New3dsMemoryMode::Legacy},
            ResultStatus::Success);
    }

    /**
     * Get whether this application is executable.
     * @param out_executable Reference to store the executable flag into.
     * @return ResultStatus result of function
     */
    virtual ResultStatus IsExecutable(bool& out_executable) {
        out_executable = true;
        return ResultStatus::Success;
    }

    /**
     * Get the code (typically .code section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadCode([[maybe_unused]] std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the icon (typically icon section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadIcon([[maybe_unused]] std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the banner (typically banner section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadBanner([[maybe_unused]] std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the logo (typically logo section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadLogo([[maybe_unused]] std::vector<u8>& buffer) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the program id of the application
     * @param out_program_id Reference to store program id into
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadProgramId([[maybe_unused]] u64& out_program_id) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the extdata id for the application
     * @param out_extdata_id Reference to store extdata id into
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadExtdataId([[maybe_unused]] u64& out_extdata_id) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the RomFS of the application
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadRomFS(
        [[maybe_unused]] std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Dump the RomFS of the applciation
     * @param target_path The target path to dump to
     * @return ResultStatus result of function
     */
    virtual ResultStatus DumpRomFS([[maybe_unused]] const std::string& target_path) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the update RomFS of the application
     * Since the RomFS can be huge, we return a file reference instead of copying to a buffer
     * @param romfs_file The file containing the RomFS
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadUpdateRomFS(
        [[maybe_unused]] std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Dump the update RomFS of the applciation
     * @param target_path The target path to dump to
     * @return ResultStatus result of function
     */
    virtual ResultStatus DumpUpdateRomFS([[maybe_unused]] const std::string& target_path) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the short title of the application
     * @param title Reference to store the application title into
     * @return ResultStatus result of function
     */
    virtual ResultStatus ReadTitle([[maybe_unused]] std::string& title) {
        return ResultStatus::ErrorNotImplemented;
    }

    /**
     * Get the long title of the application
     * @param title Referencec to store the application title into
     * @param ResultStatus result of function
     */
    virtual ResultStatus ReadTitleLong([[maybe_unused]] std::string& title) {
        return ResultStatus::ErrorNotImplemented;
    }

    virtual bool SupportsSaveStates() {
        return true;
    }

    virtual bool SupportsMultipleInstancesForSameFile() {
        return true;
    }

protected:
    Core::System& system;
    FileUtil::IOFile file;
    bool is_loaded = false;
    std::optional<Kernel::MemoryMode> memory_mode_override = std::nullopt;
};

/**
 * Identifies a bootable file and return a suitable loader
 * @param filename String filename of bootable file
 * @return best loader for this file
 */
std::unique_ptr<AppLoader> GetLoader(const std::string& filename);

} // namespace Loader
