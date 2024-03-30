// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include <QString>
#include "common/common_types.h"

using CompatibilityList = std::unordered_map<std::string, std::pair<QString, QString>>;

CompatibilityList::const_iterator FindMatchingCompatibilityEntry(
    const CompatibilityList& compatibility_list, u64 program_id);
