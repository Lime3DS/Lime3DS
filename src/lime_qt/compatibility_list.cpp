// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <fmt/format.h>
#include "lime_qt/compatibility_list.h"

CompatibilityList::const_iterator FindMatchingCompatibilityEntry(
    const CompatibilityList& compatibility_list, u64 program_id) {
    return std::find_if(compatibility_list.begin(), compatibility_list.end(),
                        [program_id](const auto& element) {
                            std::string pid = fmt::format("{:016X}", program_id);
                            return element.first == pid;
                        });
}
