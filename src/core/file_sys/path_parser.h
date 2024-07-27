// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "core/file_sys/archive_backend.h"

namespace FileSys {

/**
 * A helper class parsing and verifying a string-type Path.
 * Every archives with a sub file system should use this class to parse the path argument and check
 * the status of the file / directory in question on the host file system.
 */
class PathParser {
public:
    explicit PathParser(const Path& path);

    /**
     * Checks if the Path is valid.
     * This function should be called once a PathParser is constructed.
     * A Path is valid if:
     *  - it is a string path (with type LowPathType::Char or LowPathType::Wchar),
     *  - it starts with "/" (this seems a hard requirement in real 3DS),
     *  - it doesn't contain invalid characters, and
     *  - it doesn't go out of the root directory using "..".
     */
    bool IsValid() const {
        return is_valid;
    }

    /// Checks if the Path represents the root directory.
    bool IsRootDirectory() const {
        return is_root;
    }

    enum HostStatus {
        InvalidMountPoint,
        PathNotFound,   // "/a/b/c" when "a" doesn't exist
        FileInPath,     // "/a/b/c" when "a" is a file
        FileFound,      // "/a/b/c" when "c" is a file
        DirectoryFound, // "/a/b/c" when "c" is a directory
        NotFound        // "/a/b/c" when "a/b/" exists but "c" doesn't exist
    };

    /// Checks the status of the specified file / directory by the Path on the host file system.
    HostStatus GetHostStatus(std::string_view mount_point) const;

    /// Builds a full path on the host file system.
    std::string BuildHostPath(std::string_view mount_point) const;

private:
    std::vector<std::string> path_sequence;
    bool is_valid{};
    bool is_root{};
};

} // namespace FileSys
