// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "uisettings.h"

namespace UISettings {

const Themes themes{{
    {"Light", "default"},
    {"Light Colorful", "colorful"},
    {"Dark", "qdarkstyle"},
    {"Dark Colorful", "colorful_dark"},
    {"Midnight Blue", "qdarkstyle_midnight_blue"},
    {"Midnight Blue Colorful", "colorful_midnight_blue"},
}};

Values values = {};
} // namespace UISettings
