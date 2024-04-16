// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/romfs_reader.h"

namespace FileSys {

struct RomFSHeader {
    struct Descriptor {
        u32_le offset;
        u32_le length;
    };
    u32_le header_length;
    Descriptor directory_hash_table;
    Descriptor directory_metadata_table;
    Descriptor file_hash_table;
    Descriptor file_metadata_table;
    u32_le file_data_offset;
};
static_assert(sizeof(RomFSHeader) == 0x28, "Size of RomFSHeader is not correct");

/**
 * LayeredFS implementation. This basically adds a layer to another RomFSReader.
 *
 * patch_path: Path for RomFS replacements. Files present in this path replace or create
 * corresponding files in RomFS.
 * patch_ext_path: Path for RomFS extensions. Files present in this path:
 *  - When with an extension of ".stub", remove the corresponding file in the RomFS.
 *  - When with an extension of ".ips" or ".bps", patch the file in the RomFS.
 */
class LayeredFS : public RomFSReader {
public:
    explicit LayeredFS(std::shared_ptr<RomFSReader> romfs, std::string patch_path,
                       std::string patch_ext_path, bool load_relocations = true);
    ~LayeredFS() override;

    std::size_t GetSize() const override;
    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) override;

    bool DumpRomFS(const std::string& target_path);

    bool AllowsCachedReads() const override {
        return false;
    }

    bool CacheReady(std::size_t file_offset, std::size_t length) override {
        return false;
    }

private:
    struct File;
    struct Directory {
        std::string name;
        std::string path; // with trailing '/'
        std::vector<std::unique_ptr<File>> files;
        std::vector<std::unique_ptr<Directory>> directories;
        Directory* parent;
    };

    std::string ReadName(u32 offset, u32 name_length);

    // Loads the current directory, then its children.
    // Returns offset of the next sibling directory to load (0xFFFFFFFF if the last directory)
    u32 LoadDirectory(Directory& current, u32 offset);

    // Load the file at offset.
    // Returns offset of the next sibling file to load (0xFFFFFFFF if the last file)
    u32 LoadFile(Directory& parent, u32 offset);

    // Load replace/create relocations
    void LoadRelocations();

    // Load patch/remove relocations
    void LoadExtRelocations();

    // Calculate the offset of a single directory add it to the map and list of directories
    void PrepareBuildDirectory(Directory& current);

    // Calculate the offset of a single file add it to the map and list of files
    void PrepareBuildFile(File& current);

    // Recursively generate a sequence of files and directories and their offsets for all
    // children of current. (The current directory itself is not handled.)
    void PrepareBuild(Directory& current);

    void BuildDirectories();
    void BuildFiles();

    // Recursively extract a directory and all its contents to target_path
    // target_path should be without trailing '/'.
    bool ExtractDirectory(Directory& current, const std::string& target_path);

    void RebuildMetadata();

    void Load();

    std::shared_ptr<RomFSReader> romfs;
    std::string patch_path;
    std::string patch_ext_path;
    bool load_relocations;

    RomFSHeader header;
    Directory root;
    std::unordered_map<std::string, File*> file_path_map;
    std::unordered_map<std::string, Directory*> directory_path_map;
    std::map<u64, File*> data_offset_map; // assigned data offset -> file
    std::vector<u8> metadata;             // Includes header, hash table and metadata

    // Used for rebuilding header
    std::vector<u32_le> directory_hash_table;
    std::vector<u32_le> file_hash_table;

    std::unordered_map<Directory*, u32>
        directory_metadata_offset_map;        // directory -> metadata offset
    std::vector<Directory*> directory_list;   // sequence of directories to be written to metadata
    u64 current_directory_offset{};           // current directory metadata offset
    std::vector<u8> directory_metadata_table; // rebuilt directory metadata table

    std::unordered_map<File*, u32> file_metadata_offset_map; // file -> metadata offset
    std::vector<File*> file_list;        // sequence of files to be written to metadata
    u64 current_file_offset{};           // current file metadata offset
    std::vector<u8> file_metadata_table; // rebuilt file metadata table
    u64 current_data_offset{};           // current assigned data offset

    LayeredFS();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<RomFSReader>(*this);
        ar & romfs;
        ar & patch_path;
        ar & patch_ext_path;
        ar & load_relocations;
        if (Archive::is_loading::value) {
            Load();
        }
        // NOTE: Everything else is essentially cached, updated when we call Load
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::LayeredFS)
