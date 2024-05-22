// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include "common/common_types.h"

namespace FileSys {

// Structure of a directory entry, from http://3dbrew.org/wiki/FSDir:Read#Entry_format
const std::size_t FILENAME_LENGTH = 0x20C / 2;
struct Entry {
    char16_t filename[FILENAME_LENGTH]; // Entry name (UTF-16, null-terminated)
    std::array<char, 9> short_name; // 8.3 file name ('longfilename' -> 'LONGFI~1', null-terminated)
    char unknown1;                  // unknown (observed values: 0x0A, 0x70, 0xFD)
    std::array<char, 4>
        extension;     // 8.3 file extension (set to spaces for directories, null-terminated)
    char unknown2;     // unknown (always 0x01)
    char unknown3;     // unknown (0x00 or 0x08)
    char is_directory; // directory flag
    char is_hidden;    // hidden flag
    char is_archive;   // archive flag
    char is_read_only; // read-only flag
    u64 file_size;     // file size (for files only)
};
static_assert(sizeof(Entry) == 0x228, "Directory Entry struct isn't exactly 0x228 bytes long!");
static_assert(offsetof(Entry, short_name) == 0x20C, "Wrong offset for short_name in Entry.");
static_assert(offsetof(Entry, extension) == 0x216, "Wrong offset for extension in Entry.");
static_assert(offsetof(Entry, is_archive) == 0x21E, "Wrong offset for is_archive in Entry.");
static_assert(offsetof(Entry, file_size) == 0x220, "Wrong offset for file_size in Entry.");

class DirectoryBackend : NonCopyable {
public:
    DirectoryBackend() {}
    virtual ~DirectoryBackend() {}

    /**
     * List files contained in the directory
     * @param count Number of entries to return at once in entries
     * @param entries Buffer to read data into
     * @return Number of entries listed
     */
    virtual u32 Read(const u32 count, Entry* entries) = 0;

    /**
     * Close the directory
     * @return true if the directory closed correctly
     */
    virtual bool Close() = 0;

    virtual bool IsSlow() {
        return false;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;
};

} // namespace FileSys
