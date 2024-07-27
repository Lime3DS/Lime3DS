// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <span>
#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

namespace Loader {

/**
 * Tests if data is a valid SMDH by its length and magic number.
 * @param smdh_data data buffer to test
 * @return bool test result
 */
bool IsValidSMDH(std::span<const u8> smdh_data);

/// SMDH data structure that contains titles, icons etc. See https://www.3dbrew.org/wiki/SMDH
struct SMDH {
    u32_le magic;
    u16_le version;
    INSERT_PADDING_BYTES(2);

    struct Title {
        std::array<char16_t, 0x40> short_title;
        std::array<char16_t, 0x80> long_title;
        std::array<char16_t, 0x40> publisher;
    };
    std::array<Title, 16> titles;

    std::array<u8, 16> ratings;
    u32_le region_lockout;
    u32_le match_maker_id;
    u64_le match_maker_bit_id;
    u32_le flags;
    u16_le eula_version;
    INSERT_PADDING_BYTES(2);
    float_le banner_animation_frame;
    u32_le cec_id;
    INSERT_PADDING_BYTES(8);

    std::array<u8, 0x480> small_icon;
    std::array<u8, 0x1200> large_icon;

    /// indicates the language used for each title entry
    enum class TitleLanguage {
        Japanese = 0,
        English = 1,
        French = 2,
        German = 3,
        Italian = 4,
        Spanish = 5,
        SimplifiedChinese = 6,
        Korean = 7,
        Dutch = 8,
        Portuguese = 9,
        Russian = 10,
        TraditionalChinese = 11
    };

    enum class GameRegion {
        Japan = 0,
        NorthAmerica = 1,
        Europe = 2,
        Australia = 3,
        China = 4,
        Korea = 5,
        Taiwan = 6,
    };

    enum Flags {
        Visible = 1 << 0,
    };

    /**
     * Gets game icon from SMDH
     * @param large If true, returns large icon (48x48), otherwise returns small icon (24x24)
     * @return vector of RGB565 data
     */
    std::vector<u16> GetIcon(bool large) const;

    /**
     * Gets the short game title from SMDH
     * @param language title language
     * @return UTF-16 array of the short title
     */
    std::array<char16_t, 0x40> GetShortTitle(Loader::SMDH::TitleLanguage language) const;

    /**
     * Gets the long game title from SMDH
     * @param language title language
     * @return UTF-16 array of the long title
     */
    std::array<char16_t, 0x80> GetLongTitle(Loader::SMDH::TitleLanguage language) const;

    std::vector<GameRegion> GetRegions() const;
};
static_assert(sizeof(SMDH) == 0x36C0, "SMDH structure size is wrong");

} // namespace Loader
