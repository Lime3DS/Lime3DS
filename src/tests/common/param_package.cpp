// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include "common/logging/backend.h"
#include "common/param_package.h"

namespace Common {

TEST_CASE("ParamPackage", "[common]") {
    Common::Log::DisableLoggingInTests();
    ParamPackage original{
        {"abc", "xyz"},
        {"def", "42"},
        {"jkl", "$$:1:$2$,3"},
    };
    original.Set("ghi", 3.14f);
    ParamPackage copy(original.Serialize());
    REQUIRE(copy.Get("abc", "") == "xyz");
    REQUIRE(copy.Get("def", 0) == 42);
    REQUIRE(std::abs(copy.Get("ghi", 0.0f) - 3.14f) < 0.01f);
    REQUIRE(copy.Get("jkl", "") == "$$:1:$2$,3");
    REQUIRE(copy.Get("mno", "uvw") == "uvw");
    REQUIRE(copy.Get("abc", 42) == 42);
}

} // namespace Common
