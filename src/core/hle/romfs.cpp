// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/swap.h"
#include "core/hle/romfs.h"

namespace RomFS {

struct Header {
    u32_le header_length;
    u32_le dir_hash_table_offset;
    u32_le dir_hash_table_length;
    u32_le dir_table_offset;
    u32_le dir_table_length;
    u32_le file_hash_table_offset;
    u32_le file_hash_table_length;
    u32_le file_table_offset;
    u32_le file_table_length;
    u32_le data_offset;
};

static_assert(sizeof(Header) == 0x28, "Header has incorrect size");

struct DirectoryMetadata {
    u32_le parent_dir_offset;
    u32_le next_dir_offset;
    u32_le first_child_dir_offset;
    u32_le first_file_offset;
    u32_le same_hash_next_dir_offset;
    u32_le name_length; // in bytes
    // followed by directory name
};

static_assert(sizeof(DirectoryMetadata) == 0x18, "DirectoryMetadata has incorrect size");

struct FileMetadata {
    u32_le parent_dir_offset;
    u32_le next_file_offset;
    u64_le data_offset;
    u64_le data_length;
    u32_le same_hash_next_file_offset;
    u32_le name_length; // in bytes
    // followed by file name
};

static_assert(sizeof(FileMetadata) == 0x20, "FileMetadata has incorrect size");

static bool MatchName(const u8* buffer, u32 name_length, const std::u16string& name) {
    std::vector<char16_t> name_buffer(name_length / sizeof(char16_t));
    std::memcpy(name_buffer.data(), buffer, name_length);
    return name == std::u16string(name_buffer.begin(), name_buffer.end());
}

RomFSFile::RomFSFile(const u8* data, u64 length) : data(data), length(length) {}

const u8* RomFSFile::Data() const {
    return data;
}

u64 RomFSFile::Length() const {
    return length;
}

const RomFSFile GetFile(const u8* romfs, const std::vector<std::u16string>& path) {
    constexpr u32 INVALID_FIELD = 0xFFFFFFFF;

    // Split path into directory names and file name
    std::vector<std::u16string> dir_names = path;
    dir_names.pop_back();
    const std::u16string& file_name = path.back();

    Header header;
    std::memcpy(&header, romfs, sizeof(header));

    // Find directories of each level
    DirectoryMetadata dir;
    const u8* current_dir = romfs + header.dir_table_offset;
    std::memcpy(&dir, current_dir, sizeof(dir));
    for (const std::u16string& dir_name : dir_names) {
        u32 child_dir_offset;
        child_dir_offset = dir.first_child_dir_offset;
        while (true) {
            if (child_dir_offset == INVALID_FIELD) {
                return RomFSFile();
            }
            const u8* current_child_dir = romfs + header.dir_table_offset + child_dir_offset;
            std::memcpy(&dir, current_child_dir, sizeof(dir));
            if (MatchName(current_child_dir + sizeof(dir), dir.name_length, dir_name)) {
                current_dir = current_child_dir;
                break;
            }
            child_dir_offset = dir.next_dir_offset;
        }
    }

    // Find the file
    FileMetadata file;
    u32 file_offset = dir.first_file_offset;
    while (file_offset != INVALID_FIELD) {
        const u8* current_file = romfs + header.file_table_offset + file_offset;
        std::memcpy(&file, current_file, sizeof(file));
        if (MatchName(current_file + sizeof(file), file.name_length, file_name)) {
            return RomFSFile(romfs + header.data_offset + file.data_offset, file.data_length);
        }
        file_offset = file.next_file_offset;
    }
    return RomFSFile();
}

} // namespace RomFS
