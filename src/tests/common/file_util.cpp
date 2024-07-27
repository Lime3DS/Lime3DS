// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "common/file_util.h"
#include "common/string_util.h"

TEST_CASE("SplitFilename83 Sanity", "[common]") {
    std::string filename = "long_ass_file_name.3ds";
    std::array<char, 9> short_name;
    std::array<char, 4> extension;

    FileUtil::SplitFilename83(filename, short_name, extension);

    filename = Common::ToUpper(filename);
    std::string expected_short_name = filename.substr(0, 6).append("~1");
    std::string expected_extension = filename.substr(filename.find('.') + 1, 3);

    REQUIRE(std::memcmp(short_name.data(), expected_short_name.data(), short_name.size()) == 0);
    REQUIRE(std::memcmp(extension.data(), expected_extension.data(), extension.size()) == 0);
}
