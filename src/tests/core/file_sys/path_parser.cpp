// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "common/file_util.h"
#include "core/file_sys/path_parser.h"

namespace FileSys {

TEST_CASE("PathParser", "[core][file_sys]") {
    REQUIRE(!PathParser(Path(std::vector<u8>{})).IsValid());
    REQUIRE(!PathParser(Path("a")).IsValid());
    REQUIRE(!PathParser(Path("/|")).IsValid());
    REQUIRE(PathParser(Path("/a")).IsValid());
    REQUIRE(!PathParser(Path("/a/b/../../c/../../d")).IsValid());
    REQUIRE(PathParser(Path("/a/b/../c/../../d")).IsValid());
    REQUIRE(PathParser(Path("/")).IsRootDirectory());
    REQUIRE(!PathParser(Path("/a")).IsRootDirectory());
    REQUIRE(PathParser(Path("/a/..")).IsRootDirectory());
}

TEST_CASE("PathParser - Host file system", "[core][file_sys]") {
    std::string test_dir = "./test";
    FileUtil::CreateDir(test_dir);
    FileUtil::CreateDir(test_dir + "/z");
    FileUtil::CreateEmptyFile(test_dir + "/a");

    REQUIRE(PathParser(Path("/a")).GetHostStatus(test_dir) == PathParser::FileFound);
    REQUIRE(PathParser(Path("/b")).GetHostStatus(test_dir) == PathParser::NotFound);
    REQUIRE(PathParser(Path("/z")).GetHostStatus(test_dir) == PathParser::DirectoryFound);
    REQUIRE(PathParser(Path("/a/c")).GetHostStatus(test_dir) == PathParser::FileInPath);
    REQUIRE(PathParser(Path("/b/c")).GetHostStatus(test_dir) == PathParser::PathNotFound);

    FileUtil::DeleteDirRecursively(test_dir);
}

} // namespace FileSys
