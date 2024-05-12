// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/delay_generator.h"
#include "core/hle/result.h"

namespace FileSys {

class FileBackend;
class DirectoryBackend;

// Path string type
enum class LowPathType : u32 {
    Invalid = 0,
    Empty = 1,
    Binary = 2,
    Char = 3,
    Wchar = 4,
};

union Mode {
    u32 hex = 0;
    BitField<0, 1, u32> read_flag;
    BitField<1, 1, u32> write_flag;
    BitField<2, 1, u32> create_flag;
};

class Path {
public:
    Path() : type(LowPathType::Invalid) {}
    Path(const char* path) : type(LowPathType::Char), string(path) {}
    Path(std::string path) : type(LowPathType::Char), string(std::move(path)) {}
    Path(std::vector<u8> binary_data) : type(LowPathType::Binary), binary(std::move(binary_data)) {}
    template <std::size_t size>
    Path(const std::array<u8, size>& binary_data)
        : type(LowPathType::Binary), binary(binary_data.begin(), binary_data.end()) {}
    Path(LowPathType type, std::vector<u8> data);

    LowPathType GetType() const {
        return type;
    }

    /**
     * Gets the string representation of the path for debugging
     * @return String representation of the path for debugging
     */
    std::string DebugStr() const;

    std::string AsString() const;
    std::u16string AsU16Str() const;
    std::vector<u8> AsBinary() const;

private:
    LowPathType type;
    std::vector<u8> binary;
    std::string string;
    std::u16string u16str;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & type;
        switch (type) {
        case LowPathType::Binary:
            ar & binary;
            break;
        case LowPathType::Char:
            ar & string;
            break;
        case LowPathType::Wchar: {
            std::vector<char16_t> data;
            if (Archive::is_saving::value) {
                std::copy(u16str.begin(), u16str.end(), std::back_inserter(data));
            }
            ar & data;
            if (Archive::is_loading::value) {
                u16str = std::u16string(data.data(), data.size());
            }
        } break;
        default:
            break;
        }
    }
    friend class boost::serialization::access;
};

/// Parameters of the archive, as specified in the Create or Format call.
struct ArchiveFormatInfo {
    u32_le total_size;         ///< The pre-defined size of the archive.
    u32_le number_directories; ///< The pre-defined number of directories in the archive.
    u32_le number_files;       ///< The pre-defined number of files in the archive.
    u8 duplicate_data;         ///< Whether the archive should duplicate the data.
};
static_assert(std::is_trivial_v<ArchiveFormatInfo>, "ArchiveFormatInfo is not POD");
static_assert(sizeof(ArchiveFormatInfo) == 16, "Invalid ArchiveFormatInfo size");

class ArchiveBackend : NonCopyable {
public:
    virtual ~ArchiveBackend() {}

    /**
     * Get a descriptive name for the archive (e.g. "RomFS", "SaveData", etc.)
     */
    virtual std::string GetName() const = 0;

    /**
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or error code
     */
    virtual ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path, const Mode& mode,
                                                             u32 attributes = 0) = 0;

    /**
     * Delete a file specified by its path
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    virtual Result DeleteFile(const Path& path) const = 0;

    /**
     * Rename a File specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Result of the operation
     */
    virtual Result RenameFile(const Path& src_path, const Path& dest_path) const = 0;

    /**
     * Delete a directory specified by its path
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    virtual Result DeleteDirectory(const Path& path) const = 0;

    /**
     * Delete a directory specified by its path and anything under it
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    virtual Result DeleteDirectoryRecursively(const Path& path) const = 0;

    /**
     * Create a file specified by its path
     * @param path Path relative to the Archive
     * @param size The size of the new file, filled with zeroes
     * @return Result of the operation
     */
    virtual Result CreateFile(const Path& path, u64 size, u32 attributes = 0) const = 0;

    /**
     * Create a directory specified by its path
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    virtual Result CreateDirectory(const Path& path, u32 attributes = 0) const = 0;

    /**
     * Rename a Directory specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Result of the operation
     */
    virtual Result RenameDirectory(const Path& src_path, const Path& dest_path) const = 0;

    /**
     * Open a directory specified by its path
     * @param path Path relative to the archive
     * @return Opened directory, or error code
     */
    virtual ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) = 0;

    /**
     * Get the free space
     * @return The number of free bytes in the archive
     */
    virtual u64 GetFreeBytes() const = 0;

    /**
     * Close the archive
     */
    virtual void Close() {}

    virtual Result Control(u32 action, u8* input, size_t input_size, u8* output,
                           size_t output_size) {
        LOG_WARNING(Service_FS,
                    "(STUBBED) called, archive={}, action={:08X}, input_size={:08X}, "
                    "output_size={:08X}",
                    GetName(), action, input_size, output_size);
        return ResultSuccess;
    }

    u64 GetOpenDelayNs() {
        if (delay_generator != nullptr) {
            return delay_generator->GetOpenDelayNs();
        }
        LOG_ERROR(Service_FS, "Delay generator was not initalized. Using default");
        delay_generator = std::make_unique<DefaultDelayGenerator>();
        return delay_generator->GetOpenDelayNs();
    }

    virtual Result SetSaveDataSecureValue(u32 secure_value_slot, u64 secure_value, bool flush) {

        // TODO: Generate and Save the Secure Value

        LOG_WARNING(Service_FS,
                    "(STUBBED) called, value=0x{:016x} secure_value_slot=0x{:04X} "
                    "flush={}",
                    secure_value, secure_value_slot, flush);

        return ResultSuccess;
    }

    virtual ResultVal<std::tuple<bool, bool, u64>> GetSaveDataSecureValue(u32 secure_value_slot) {

        // TODO: Implement Secure Value Lookup & Generation

        LOG_WARNING(Service_FS, "(STUBBED) called secure_value_slot=0x{:08X}", secure_value_slot);

        return std::make_tuple<bool, bool, u64>(false, true, 0);
    }

    virtual bool IsSlow() {
        return false;
    }

protected:
    std::unique_ptr<DelayGenerator> delay_generator;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & delay_generator;
    }
    friend class boost::serialization::access;
};

class ArchiveFactory : NonCopyable {
public:
    virtual ~ArchiveFactory() {}

    /**
     * Get a descriptive name for the archive (e.g. "RomFS", "SaveData", etc.)
     */
    virtual std::string GetName() const = 0;

    /**
     * Tries to open the archive of this type with the specified path
     * @param path Path to the archive
     * @param program_id the program ID of the client that requests the operation
     * @return An ArchiveBackend corresponding operating specified archive path.
     */
    virtual ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path, u64 program_id) = 0;

    /**
     * Deletes the archive contents and then re-creates the base folder
     * @param path Path to the archive
     * @param format_info Format information for the new archive
     * @param program_id the program ID of the client that requests the operation
     * @return Result of the operation, 0 on success
     */
    virtual Result Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info,
                          u64 program_id, u32 directory_buckets, u32 file_buckets) = 0;

    /**
     * Retrieves the format info about the archive with the specified path
     * @param path Path to the archive
     * @param program_id the program ID of the client that requests the operation
     * @return Format information about the archive or error code
     */
    virtual ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path, u64 program_id) const = 0;

    virtual bool IsSlow() {
        return false;
    }

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;
};

} // namespace FileSys
