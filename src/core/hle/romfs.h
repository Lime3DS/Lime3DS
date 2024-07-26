// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "common/common_types.h"

namespace RomFS {

class RomFSFile {
public:
    RomFSFile() = default;
    RomFSFile(const u8* data, u64 length);
    const u8* Data() const;
    u64 Length() const;

private:
    const u8* data = nullptr;
    u64 length = 0;
};

/**
 * Gets a RomFSFile class to a file in a RomFS image.
 * @param romfs The pointer to the RomFS image
 * @param path A vector containing the directory names and file name of the path to the file
 * @return the RomFSFile to the file
 * @todo reimplement this with a full RomFS manager
 */
const RomFSFile GetFile(const u8* romfs, const std::vector<std::u16string>& path);

} // namespace RomFS
