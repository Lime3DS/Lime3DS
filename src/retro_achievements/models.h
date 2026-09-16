// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
#include <string>

namespace RetroAchievements {

struct User {
    std::string display_name;
    std::string username;
    std::string token;
    uint32_t score;
    std::string avatar_url;
};

struct Game {
    std::string title;
    std::string badge_url;
};

struct Achievement {
    uint32_t id;
    std::string title;
    std::string description;
    uint32_t points;
    uint8_t state;
    uint8_t category;
    uint8_t type;
    uint8_t bucket;
    uint8_t unlocked;
    std::string measured_progress;
    float measured_percent;
    float rarity;
    float rarity_hardcore;
    std::string badge_url;
    std::string badge_locked_url;
};

} // namespace RetroAchievements
