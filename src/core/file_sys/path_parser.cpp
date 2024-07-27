// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <set>
#include "common/file_util.h"
#include "common/string_util.h"
#include "core/file_sys/path_parser.h"

namespace FileSys {

PathParser::PathParser(const Path& path) {
    if (path.GetType() != LowPathType::Char && path.GetType() != LowPathType::Wchar) {
        is_valid = false;
        return;
    }

    auto path_string = path.AsString();
    if (path_string.size() == 0 || path_string[0] != '/') {
        is_valid = false;
        return;
    }

    // Filter out invalid characters for the host system.
    // Although some of these characters are valid on 3DS, they are unlikely to be used by games.
    if (std::find_if(path_string.begin(), path_string.end(), [](char c) {
            static const std::set<char> invalid_chars{'<', '>', '\\', '|', ':', '\"', '*', '?'};
            return invalid_chars.find(c) != invalid_chars.end();
        }) != path_string.end()) {
        is_valid = false;
        return;
    }

    path_sequence = Common::SplitString(path_string, '/');

    auto begin = path_sequence.begin();
    auto end = path_sequence.end();
    end = std::remove_if(begin, end, [](std::string& str) { return str == "" || str == "."; });
    path_sequence = std::vector<std::string>(begin, end);

    // checks if the path is out of bounds.
    int level = 0;
    for (auto& node : path_sequence) {
        if (node == "..") {
            --level;
            if (level < 0) {
                is_valid = false;
                return;
            }
        } else {
            ++level;
        }
    }

    is_valid = true;
    is_root = level == 0;
}

PathParser::HostStatus PathParser::GetHostStatus(std::string_view mount_point) const {
    std::string path{mount_point};
    if (!FileUtil::IsDirectory(path))
        return InvalidMountPoint;
    if (path_sequence.empty()) {
        return DirectoryFound;
    }

    for (auto iter = path_sequence.begin(); iter != path_sequence.end() - 1; iter++) {
        if (path.back() != '/')
            path += '/';
        path += *iter;

        if (!FileUtil::Exists(path))
            return PathNotFound;
        if (FileUtil::IsDirectory(path))
            continue;
        return FileInPath;
    }

    path += "/" + path_sequence.back();
    if (!FileUtil::Exists(path))
        return NotFound;
    if (FileUtil::IsDirectory(path))
        return DirectoryFound;
    return FileFound;
}

std::string PathParser::BuildHostPath(std::string_view mount_point) const {
    std::string path{mount_point};
    for (auto& node : path_sequence) {
        if (path.back() != '/')
            path += '/';
        path += node;
    }
    return path;
}

} // namespace FileSys
