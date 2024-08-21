// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace NetSettings {

struct Values {
    // WebService
    std::string web_api_url;
    std::string lime3ds_username;
    std::string lime3ds_token;
} extern values;

} // namespace NetSettings
