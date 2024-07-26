// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <unordered_map>
#include "common/common_types.h"
#include "core/hle/service/cfg/cfg.h"

namespace Service::CFG {

/// The default parameters of a config block.
struct ConfigBlockDefaults {
    AccessFlag access_flags;
    std::span<const u8> data;

    ConfigBlockDefaults(AccessFlag access_flags_, const void* data_, std::size_t size_)
        : access_flags(access_flags_), data(reinterpret_cast<const u8*>(data_), size_) {}
};

/// Gets the default EULA version.
const EULAVersion& GetDefaultEULAVersion();

/// Retrieves a map of known config block IDs to their defaults.
const std::unordered_map<ConfigBlockID, ConfigBlockDefaults>& GetDefaultConfigBlocks();

/// Checks whether a particular config block has defaults defined.
bool HasDefaultConfigBlock(ConfigBlockID block_id);

/// Gets the defined defaults for a particular config block.
const ConfigBlockDefaults& GetDefaultConfigBlock(ConfigBlockID block_id);

} // namespace Service::CFG
