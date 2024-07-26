// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include "common/alignment.h"
#include "common/archives.h"
#include "common/assert.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/file_sys/layered_fs.h"
#include "core/file_sys/patch.h"

SERIALIZE_EXPORT_IMPL(FileSys::LayeredFS)

namespace FileSys {

struct FileRelocationInfo {
    int type;                      // 0 - none, 1 - replaced / created, 2 - patched, 3 - removed
    u64 original_offset;           // Type 0. Offset is absolute
    std::string replace_file_path; // Type 1
    std::vector<u8> patched_file;  // Type 2
    u64 size;                      // Relocated file size
};
struct LayeredFS::File {
    std::string name;
    std::string path;
    FileRelocationInfo relocation{};
    Directory* parent;
};

struct DirectoryMetadata {
    u32_le parent_directory_offset;
    u32_le next_sibling_offset;
    u32_le first_child_directory_offset;
    u32_le first_file_offset;
    u32_le hash_bucket_next;
    u32_le name_length;
    // Followed by a name of name length (aligned up to 4)
};
static_assert(sizeof(DirectoryMetadata) == 0x18, "Size of DirectoryMetadata is not correct");

struct FileMetadata {
    u32_le parent_directory_offset;
    u32_le next_sibling_offset;
    u64_le file_data_offset;
    u64_le file_data_length;
    u32_le hash_bucket_next;
    u32_le name_length;
    // Followed by a name of name length (aligned up to 4)
};
static_assert(sizeof(FileMetadata) == 0x20, "Size of FileMetadata is not correct");

LayeredFS::LayeredFS() = default;

LayeredFS::LayeredFS(std::shared_ptr<RomFSReader> romfs_, std::string patch_path_,
                     std::string patch_ext_path_, bool load_relocations_)
    : romfs(std::move(romfs_)), patch_path(std::move(patch_path_)),
      patch_ext_path(std::move(patch_ext_path_)), load_relocations(load_relocations_) {
    Load();
}

void LayeredFS::Load() {
    romfs->ReadFile(0, sizeof(header), reinterpret_cast<u8*>(&header));

    ASSERT_MSG(header.header_length == sizeof(header), "Header size is incorrect");

    // TODO: is root always the first directory in table?
    root.parent = &root;
    LoadDirectory(root, 0);

    if (load_relocations) {
        LoadRelocations();
        LoadExtRelocations();
    }

    RebuildMetadata();
}

LayeredFS::~LayeredFS() = default;

u32 LayeredFS::LoadDirectory(Directory& current, u32 offset) {
    DirectoryMetadata metadata;
    romfs->ReadFile(header.directory_metadata_table.offset + offset, sizeof(metadata),
                    reinterpret_cast<u8*>(&metadata));

    current.name = ReadName(header.directory_metadata_table.offset + offset + sizeof(metadata),
                            metadata.name_length);
    current.path = current.parent->path + current.name + DIR_SEP;
    directory_path_map.emplace(current.path, &current);

    u32 file_offset = metadata.first_file_offset;
    while (file_offset != 0xFFFFFFFF) {
        file_offset = LoadFile(current, file_offset);
    }

    u32 child_directory_offset = metadata.first_child_directory_offset;
    while (child_directory_offset != 0xFFFFFFFF) {
        auto child = std::make_unique<Directory>();
        auto& directory = *child;
        directory.parent = &current;
        current.directories.emplace_back(std::move(child));
        child_directory_offset = LoadDirectory(directory, child_directory_offset);
    }

    return metadata.next_sibling_offset;
}

u32 LayeredFS::LoadFile(Directory& parent, u32 offset) {
    FileMetadata metadata;
    romfs->ReadFile(header.file_metadata_table.offset + offset, sizeof(metadata),
                    reinterpret_cast<u8*>(&metadata));

    auto file = std::make_unique<File>();
    file->name = ReadName(header.file_metadata_table.offset + offset + sizeof(metadata),
                          metadata.name_length);
    file->path = parent.path + file->name;
    file->relocation.original_offset = header.file_data_offset + metadata.file_data_offset;
    file->relocation.size = metadata.file_data_length;
    file->parent = &parent;

    file_path_map.emplace(file->path, file.get());
    parent.files.emplace_back(std::move(file));

    return metadata.next_sibling_offset;
}

std::string LayeredFS::ReadName(u32 offset, u32 name_length) {
    std::vector<u16_le> buffer(name_length / sizeof(u16_le));
    romfs->ReadFile(offset, name_length, reinterpret_cast<u8*>(buffer.data()));

    std::u16string name(buffer.size(), 0);
    std::transform(buffer.begin(), buffer.end(), name.begin(), [](u16_le character) {
        return static_cast<char16_t>(static_cast<u16>(character));
    });
    return Common::UTF16ToUTF8(name);
}

void LayeredFS::LoadRelocations() {
    if (!FileUtil::Exists(patch_path)) {
        return;
    }

    const FileUtil::DirectoryEntryCallable callback =
        [this, &callback]([[maybe_unused]] u64* num_entries_out, const std::string& directory,
                          const std::string& virtual_name) {
            auto* parent = directory_path_map.at(directory.substr(patch_path.size() - 1));

            if (FileUtil::IsDirectory(directory + virtual_name + DIR_SEP)) {
                const auto path =
                    (directory + virtual_name + DIR_SEP).substr(patch_path.size() - 1);
                if (!directory_path_map.count(path)) { // Add this directory
                    auto child_dir = std::make_unique<Directory>();
                    child_dir->name = virtual_name;
                    child_dir->path = path;
                    child_dir->parent = parent;
                    directory_path_map.emplace(path, child_dir.get());
                    parent->directories.emplace_back(std::move(child_dir));
                    LOG_INFO(Service_FS, "LayeredFS created directory {}", path);
                }
                return FileUtil::ForeachDirectoryEntry(nullptr, directory + virtual_name + DIR_SEP,
                                                       callback);
            }

            const auto path = (directory + virtual_name).substr(patch_path.size() - 1);
            if (!file_path_map.count(path)) { // Newly created file
                auto file = std::make_unique<File>();
                file->name = virtual_name;
                file->path = path;
                file->parent = parent;
                file_path_map.emplace(path, file.get());
                parent->files.emplace_back(std::move(file));
                LOG_INFO(Service_FS, "LayeredFS created file {}", path);
            }

            auto* file = file_path_map.at(path);
            file->relocation.type = 1;
            file->relocation.replace_file_path = directory + virtual_name;
            file->relocation.size = FileUtil::GetSize(directory + virtual_name);
            LOG_INFO(Service_FS, "LayeredFS replacement file in use for {}", path);
            return true;
        };

    FileUtil::ForeachDirectoryEntry(nullptr, patch_path, callback);
}

void LayeredFS::LoadExtRelocations() {
    if (!FileUtil::Exists(patch_ext_path)) {
        return;
    }

    if (patch_ext_path.back() == '/' || patch_ext_path.back() == '\\') {
        // ScanDirectoryTree expects a path without trailing '/'
        patch_ext_path.erase(patch_ext_path.size() - 1, 1);
    }

    FileUtil::FSTEntry result;
    FileUtil::ScanDirectoryTree(patch_ext_path, result, 256);

    for (const auto& entry : result.children) {
        if (FileUtil::IsDirectory(entry.physicalName)) {
            continue;
        }

        const auto path = entry.physicalName.substr(patch_ext_path.size());
        if (path.size() >= 5 && path.substr(path.size() - 5) == ".stub") {
            // Remove the corresponding file if exists
            const auto file_path = path.substr(0, path.size() - 5);
            if (file_path_map.count(file_path)) {
                auto& file = *file_path_map[file_path];
                file.relocation.type = 3;
                file.relocation.size = 0;
                file_path_map.erase(file_path);
                LOG_INFO(Service_FS, "LayeredFS removed file {}", file_path);
            } else {
                LOG_WARNING(Service_FS, "LayeredFS file for stub {} not found", path);
            }
        } else if (path.size() >= 4) {
            const auto extension = path.substr(path.size() - 4);
            if (extension != ".ips" && extension != ".bps") {
                LOG_WARNING(Service_FS, "LayeredFS unknown ext file {}", path);
            }

            const auto file_path = path.substr(0, path.size() - 4);
            if (!file_path_map.count(file_path)) {
                LOG_WARNING(Service_FS, "LayeredFS original file for patch {} not found", path);
                continue;
            }

            FileUtil::IOFile patch_file(entry.physicalName, "rb");
            if (!patch_file) {
                LOG_ERROR(Service_FS, "LayeredFS Could not open file {}", entry.physicalName);
                continue;
            }

            const auto size = patch_file.GetSize();
            std::vector<u8> patch(size);
            if (patch_file.ReadBytes(patch.data(), size) != size) {
                LOG_ERROR(Service_FS, "LayeredFS Could not read file {}", entry.physicalName);
                continue;
            }

            auto& file = *file_path_map[file_path];
            std::vector<u8> buffer(file.relocation.size); // Original size
            romfs->ReadFile(file.relocation.original_offset, buffer.size(), buffer.data());

            bool ret = false;
            if (extension == ".ips") {
                ret = Patch::ApplyIpsPatch(patch, buffer);
            } else {
                ret = Patch::ApplyBpsPatch(patch, buffer);
            }

            if (ret) {
                LOG_INFO(Service_FS, "LayeredFS patched file {}", file_path);

                file.relocation.type = 2;
                file.relocation.size = buffer.size();
                file.relocation.patched_file = std::move(buffer);
            } else {
                LOG_ERROR(Service_FS, "LayeredFS failed to patch file {}", file_path);
            }
        } else {
            LOG_WARNING(Service_FS, "LayeredFS unknown ext file {}", path);
        }
    }
}

static std::size_t GetNameSize(const std::string& name) {
    std::u16string u16name = Common::UTF8ToUTF16(name);
    return Common::AlignUp(u16name.size() * 2, 4);
}

void LayeredFS::PrepareBuildDirectory(Directory& current) {
    directory_metadata_offset_map.emplace(&current, static_cast<u32>(current_directory_offset));
    directory_list.emplace_back(&current);
    current_directory_offset += sizeof(DirectoryMetadata) + GetNameSize(current.name);
}

void LayeredFS::PrepareBuildFile(File& current) {
    if (current.relocation.type == 3) { // Deleted files are not counted
        return;
    }
    file_metadata_offset_map.emplace(&current, static_cast<u32>(current_file_offset));
    file_list.emplace_back(&current);
    current_file_offset += sizeof(FileMetadata) + GetNameSize(current.name);
}

void LayeredFS::PrepareBuild(Directory& current) {
    for (const auto& child : current.files) {
        PrepareBuildFile(*child);
    }

    for (const auto& child : current.directories) {
        PrepareBuildDirectory(*child);
    }

    for (const auto& child : current.directories) {
        PrepareBuild(*child);
    }
}

// Implementation from 3dbrew
static u32 CalcHash(const std::string& name, u32 parent_offset) {
    u32 hash = parent_offset ^ 123456789;
    std::u16string u16name = Common::UTF8ToUTF16(name);
    for (char16_t c : u16name) {
        hash = (hash >> 5) | (hash << 27);
        hash ^= static_cast<u16>(c);
    }
    return hash;
}

static std::size_t WriteName(u8* dest, std::u16string name) {
    const auto buffer_size = Common::AlignUp(name.size() * 2, 4);
    std::vector<u16_le> buffer(buffer_size / 2);
    std::transform(name.begin(), name.end(), buffer.begin(), [](char16_t character) {
        return static_cast<u16_le>(static_cast<u16>(character));
    });
    std::memcpy(dest, buffer.data(), buffer_size);

    return buffer_size;
}

void LayeredFS::BuildDirectories() {
    directory_metadata_table.resize(current_directory_offset, 0xFF);

    std::size_t written = 0;
    for (const auto& directory : directory_list) {
        DirectoryMetadata metadata;
        std::memset(&metadata, 0xFF, sizeof(metadata));
        metadata.parent_directory_offset = directory_metadata_offset_map.at(directory->parent);

        if (directory->parent != directory) {
            bool flag = false;
            for (const auto& sibling : directory->parent->directories) {
                if (flag) {
                    metadata.next_sibling_offset = directory_metadata_offset_map.at(sibling.get());
                    break;
                } else if (sibling.get() == directory) {
                    flag = true;
                }
            }
        }

        if (!directory->directories.empty()) {
            metadata.first_child_directory_offset =
                directory_metadata_offset_map.at(directory->directories.front().get());
        }

        if (!directory->files.empty()) {
            metadata.first_file_offset =
                file_metadata_offset_map.at(directory->files.front().get());
        }

        const auto bucket = CalcHash(directory->name, metadata.parent_directory_offset) %
                            directory_hash_table.size();
        metadata.hash_bucket_next = directory_hash_table[bucket];
        directory_hash_table[bucket] = directory_metadata_offset_map.at(directory);

        // Write metadata and name
        std::u16string u16name = Common::UTF8ToUTF16(directory->name);
        metadata.name_length = static_cast<u32_le>(u16name.size() * 2);

        std::memcpy(directory_metadata_table.data() + written, &metadata, sizeof(metadata));
        written += sizeof(metadata);

        written += WriteName(directory_metadata_table.data() + written, u16name);
    }

    ASSERT_MSG(written == directory_metadata_table.size(),
               "Calculated size for directory metadata table is wrong");
}

void LayeredFS::BuildFiles() {
    file_metadata_table.resize(current_file_offset, 0xFF);

    std::size_t written = 0;
    for (const auto& file : file_list) {
        FileMetadata metadata;
        std::memset(&metadata, 0xFF, sizeof(metadata));

        metadata.parent_directory_offset = directory_metadata_offset_map.at(file->parent);

        bool flag = false;
        for (const auto& sibling : file->parent->files) {
            if (sibling->relocation.type == 3) { // removed file
                continue;
            }
            if (flag) {
                metadata.next_sibling_offset = file_metadata_offset_map.at(sibling.get());
                break;
            } else if (sibling.get() == file) {
                flag = true;
            }
        }

        metadata.file_data_offset = current_data_offset;
        metadata.file_data_length = file->relocation.size;
        current_data_offset += Common::AlignUp(metadata.file_data_length, 16);
        if (metadata.file_data_length != 0) {
            data_offset_map.emplace(metadata.file_data_offset, file);
        }

        const auto bucket =
            CalcHash(file->name, metadata.parent_directory_offset) % file_hash_table.size();
        metadata.hash_bucket_next = file_hash_table[bucket];
        file_hash_table[bucket] = file_metadata_offset_map.at(file);

        // Write metadata and name
        std::u16string u16name = Common::UTF8ToUTF16(file->name);
        metadata.name_length = static_cast<u32_le>(u16name.size() * 2);

        std::memcpy(file_metadata_table.data() + written, &metadata, sizeof(metadata));
        written += sizeof(metadata);

        written += WriteName(file_metadata_table.data() + written, u16name);
    }

    ASSERT_MSG(written == file_metadata_table.size(),
               "Calculated size for file metadata table is wrong");
}

// Implementation from 3dbrew
static std::size_t GetHashTableSize(std::size_t entry_count) {
    if (entry_count < 3) {
        return 3;
    } else if (entry_count < 19) {
        return entry_count | 1;
    } else {
        std::size_t count = entry_count;
        while (count % 2 == 0 || count % 3 == 0 || count % 5 == 0 || count % 7 == 0 ||
               count % 11 == 0 || count % 13 == 0 || count % 17 == 0) {
            count++;
        }
        return count;
    }
}

void LayeredFS::RebuildMetadata() {
    PrepareBuildDirectory(root);
    PrepareBuild(root);

    directory_hash_table.resize(GetHashTableSize(directory_list.size()), 0xFFFFFFFF);
    file_hash_table.resize(GetHashTableSize(file_list.size()), 0xFFFFFFFF);

    BuildDirectories();
    BuildFiles();

    // Create header
    RomFSHeader header;
    header.header_length = sizeof(header);
    header.directory_hash_table = {
        /*offset*/ sizeof(header),
        /*length*/ static_cast<u32_le>(directory_hash_table.size() * sizeof(u32_le))};
    header.directory_metadata_table = {
        /*offset*/
        header.directory_hash_table.offset + header.directory_hash_table.length,
        /*length*/ static_cast<u32_le>(directory_metadata_table.size())};
    header.file_hash_table = {
        /*offset*/
        header.directory_metadata_table.offset + header.directory_metadata_table.length,
        /*length*/ static_cast<u32_le>(file_hash_table.size() * sizeof(u32_le))};
    header.file_metadata_table = {/*offset*/ header.file_hash_table.offset +
                                      header.file_hash_table.length,
                                  /*length*/ static_cast<u32_le>(file_metadata_table.size())};
    header.file_data_offset =
        Common::AlignUp(header.file_metadata_table.offset + header.file_metadata_table.length, 16);

    // Write hash table and metadata table
    metadata.resize(header.file_data_offset);
    std::memcpy(metadata.data(), &header, header.header_length);
    std::memcpy(metadata.data() + header.directory_hash_table.offset, directory_hash_table.data(),
                header.directory_hash_table.length);
    std::memcpy(metadata.data() + header.directory_metadata_table.offset,
                directory_metadata_table.data(), header.directory_metadata_table.length);
    std::memcpy(metadata.data() + header.file_hash_table.offset, file_hash_table.data(),
                header.file_hash_table.length);
    std::memcpy(metadata.data() + header.file_metadata_table.offset, file_metadata_table.data(),
                header.file_metadata_table.length);
}

std::size_t LayeredFS::GetSize() const {
    return metadata.size() + current_data_offset;
}

std::size_t LayeredFS::ReadFile(std::size_t offset, std::size_t length, u8* buffer) {
    ASSERT_MSG(offset + length <= GetSize(), "Out of bound");

    std::size_t read_size = 0;
    if (offset < metadata.size()) {
        // First read the metadata
        const auto to_read = std::min(metadata.size() - offset, length);
        std::memcpy(buffer, metadata.data() + offset, to_read);
        read_size += to_read;
        offset = 0;
    } else {
        offset -= metadata.size();
    }

    // Read files
    auto current = (--data_offset_map.upper_bound(offset));
    while (read_size < length) {
        const auto relative_offset = offset - current->first;
        std::size_t to_read{};
        if (current->second->relocation.size > relative_offset) {
            to_read = std::min<std::size_t>(current->second->relocation.size - relative_offset,
                                            length - read_size);
        }
        const auto alignment =
            std::min<std::size_t>(Common::AlignUp(current->second->relocation.size, 16) -
                                      relative_offset,
                                  length - read_size) -
            to_read;

        // Read the file in different ways depending on relocation type
        auto& relocation = current->second->relocation;
        if (relocation.type == 0) { // none
            romfs->ReadFile(relocation.original_offset + relative_offset, to_read,
                            buffer + read_size);
        } else if (relocation.type == 1) { // replace
            FileUtil::IOFile replace_file(relocation.replace_file_path, "rb");
            if (replace_file) {
                replace_file.Seek(relative_offset, SEEK_SET);
                replace_file.ReadBytes(buffer + read_size, to_read);
            } else {
                LOG_ERROR(Service_FS, "Could not open replacement file for {}",
                          current->second->path);
            }
        } else if (relocation.type == 2) { // patch
            std::memcpy(buffer + read_size, relocation.patched_file.data() + relative_offset,
                        to_read);
        } else {
            UNREACHABLE();
        }

        std::memset(buffer + read_size + to_read, 0, alignment);

        read_size += to_read + alignment;
        offset += to_read + alignment;
        current++;
    }

    return read_size;
}

bool LayeredFS::ExtractDirectory(Directory& current, const std::string& target_path) {
    if (!FileUtil::CreateFullPath(target_path + current.path)) {
        LOG_ERROR(Service_FS, "Could not create path {}", target_path + current.path);
        return false;
    }

    constexpr std::size_t BufferSize = 0x10000;
    std::array<u8, BufferSize> buffer;
    for (const auto& file : current.files) {
        // Extract file
        const auto path = target_path + file->path;
        LOG_INFO(Service_FS, "Extracting {} to {}", file->path, path);

        FileUtil::IOFile target_file(path, "wb");
        if (!target_file) {
            LOG_ERROR(Service_FS, "Could not open file {}", path);
            return false;
        }

        std::size_t written = 0;
        while (written < file->relocation.size) {
            const auto to_read =
                std::min<std::size_t>(buffer.size(), file->relocation.size - written);
            if (romfs->ReadFile(file->relocation.original_offset + written, to_read,
                                buffer.data()) != to_read) {
                LOG_ERROR(Service_FS, "Could not read from RomFS");
                return false;
            }

            if (target_file.WriteBytes(buffer.data(), to_read) != to_read) {
                LOG_ERROR(Service_FS, "Could not write to file {}", path);
                return false;
            }

            written += to_read;
        }
    }

    for (const auto& directory : current.directories) {
        if (!ExtractDirectory(*directory, target_path)) {
            return false;
        }
    }

    return true;
}

bool LayeredFS::DumpRomFS(const std::string& target_path) {
    std::string path = target_path;
    if (path.back() == '/' || path.back() == '\\') {
        path.erase(path.size() - 1, 1);
    }

    return ExtractDirectory(root, path);
}

} // namespace FileSys
