# CMake >=3.12 required for 20 to be a valid value for CXX_STANDARD,
# and >=3.25 required to make LTO work on Android.
cmake_minimum_required(VERSION 3.25)

# Don't override the warning flags in MSVC:
cmake_policy(SET CMP0092 NEW)
# Enforce new LTO setting
cmake_policy(SET CMP0069 NEW)
# Honor visibility properties for all targets
# Set the default so subdirectory cmake_minimum_required calls won't unset the policy.
cmake_policy(SET CMP0063 NEW)
# Allow conditions to be used in cmake_dependent_option
cmake_policy(SET CMP0127 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)

# Prefer building bundled dependencies as static instead of shared
set(BUILD_SHARED_LIBS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")
include(DownloadExternals)
include(CMakeDependentOption)

project(citra LANGUAGES C CXX ASM)
# must be invoked after project() command when using CMAKE_TOOLCHAIN_FILE
include(FindPkgConfig)
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJC OBJCXX)
endif()

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux" AND MINGW)
    string(TOLOWER ${LIBTYPE} LIBTYPE_LOWER)
    set(CMAKE_AR x86_64-w64-mingw32.${LIBTYPE_LOWER}-gcc-ar)
endif()

if (BSD STREQUAL "OpenBSD")
    add_link_options(-z wxneeded)
endif()

option(ENABLE_LIBRETRO "Build as a LibRetro core" OFF)

# Some submodules like to pick their own default build type if not specified.
# Make sure we default to Release build type always, unless the generator has custom types.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

if (APPLE AND NOT ENABLE_LIBRETRO)
    # Silence warnings on empty objects, for example when platform-specific code is #ifdef'd out.
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")

    if (IOS)
        # Minimum iOS 14
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0")

        # Enable searching CMAKE_PREFIX_PATH for bundled dependencies.
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
    else()
        # Minimum macOS 13
        set(CMAKE_OSX_DEPLOYMENT_TARGET "13.4")

        # Catch compiler issue on AppleClang versions below 15.0
        # TODO: Remove this check when we drop macOS 13 Ventura
        if (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND
            CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
            message(FATAL_ERROR "AppleClang 15.0 or later is required due to a compiler bug in earlier versions.\n"
                                "Current version: ${CMAKE_CXX_COMPILER_VERSION}\n"
                                "After updating, delete 'CMakeCache.txt' in the build directory.")
        endif()
    endif()
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    set(IS_DEBUG_BUILD ON)
    set(IS_RELEASE_BUILD OFF)
else()
    set(IS_DEBUG_BUILD OFF)
    set(IS_RELEASE_BUILD ON)
endif()

if (MSVC)
    add_compile_options(
        /wd4711 # Suppresses `function 'xxxxx' selected for automatic inline expansion` messages
        /wd5045 # Suppresses `Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified` messages
    )
endif()

# LTO takes too much memory and time using MSVC.
if (NOT MSVC AND IS_RELEASE_BUILD)
    set(DEFAULT_ENABLE_LTO ON)
else()
    set(DEFAULT_ENABLE_LTO OFF)
endif()

# Disable OpenGL by default on Linux aarch64.
# Some aarch64 devices running Linux don't support OpenGL, and users may encounter issues.
if (LINUX AND CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(DEFAULT_ENABLE_OPENGL OFF)
else()
    set(DEFAULT_ENABLE_OPENGL ON)
endif()

# Track which options were explicitly set by the user (for libretro conflict detection)
set(_LIBRETRO_INCOMPATIBLE_OPTIONS
    ENABLE_SDL2 ENABLE_QT ENABLE_WEB_SERVICE ENABLE_SCRIPTING ENABLE_GDBSTUB
    ENABLE_OPENAL ENABLE_ROOM ENABLE_ROOM_STANDALONE ENABLE_CUBEB ENABLE_LIBUSB)
set(_USER_SET_OPTIONS "")
foreach(_opt IN LISTS _LIBRETRO_INCOMPATIBLE_OPTIONS)
    if(DEFINED ${_opt})
        list(APPEND _USER_SET_OPTIONS ${_opt})
    endif()
endforeach()

option(ENABLE_SDL2 "Enable using SDL2" ON)
option(USE_SYSTEM_SDL2 "Use the system SDL2 lib (instead of the bundled one)" OFF)

# Set bundled qt as dependent options.
option(ENABLE_QT "Enable the Qt frontend" ON)
option(ENABLE_QT_TRANSLATION "Enable translations for the Qt frontend" ON)
option(ENABLE_QT_UPDATE_CHECKER "Enable built-in update checker for the Qt frontend" OFF)

CMAKE_DEPENDENT_OPTION(ENABLE_TESTS "Enable generating tests executable" ON "NOT IOS" OFF)
CMAKE_DEPENDENT_OPTION(ENABLE_ROOM "Enable dedicated room functionality" ON "NOT ANDROID AND NOT IOS" OFF)
CMAKE_DEPENDENT_OPTION(ENABLE_ROOM_STANDALONE "Enable generating a standalone dedicated room executable" ON "ENABLE_ROOM" OFF)

option(ENABLE_WEB_SERVICE "Enable web services (telemetry, etc.)" ON)
option(ENABLE_SCRIPTING "Enable RPC server for scripting" ON)
option(ENABLE_GDBSTUB "Enable GDB stub for emulated applications" ON)

CMAKE_DEPENDENT_OPTION(ENABLE_CUBEB "Enables the cubeb audio backend" ON "NOT IOS" OFF)
option(ENABLE_OPENAL "Enables the OpenAL audio backend" ON)

CMAKE_DEPENDENT_OPTION(ENABLE_LIBUSB "Enable libusb for GameCube Adapter support" ON "NOT IOS" OFF)

CMAKE_DEPENDENT_OPTION(ENABLE_SOFTWARE_RENDERER "Enables the software renderer" ON "NOT ANDROID" OFF)
CMAKE_DEPENDENT_OPTION(ENABLE_OPENGL "Enables the OpenGL renderer" ${DEFAULT_ENABLE_OPENGL} "NOT APPLE" OFF)
# NetBSD doesn't support Vulkan yet, remove this check when it does.
CMAKE_DEPENDENT_OPTION(ENABLE_VULKAN "Enables the Vulkan renderer" ON "NOT (BSD MATCHES \"NetBSD\")" OFF)

option(ENABLE_DISCORD_RPC "Enables Discord Rich Presence" OFF)

option(ENABLE_MICROPROFILE "Enables microprofile capabilities" OFF)

option(ENABLE_SSE42 "Enable SSE4.2 optimizations on x86_64" ON)

option(ENABLE_DEVELOPER_OPTIONS "Enable functionality targeted at emulator developers" OFF)

option(ENABLE_BUILTIN_KEYBLOB "Enable the inclusion of the default crypto keys blob" ON)

# Compile options
CMAKE_DEPENDENT_OPTION(COMPILE_WITH_DWARF "Add DWARF debugging information" ${IS_DEBUG_BUILD} "MINGW" OFF)
option(ENABLE_LTO "Enable link time optimization" ${DEFAULT_ENABLE_LTO})
option(ENABLE_NATIVE_OPTIMIZATION "Enables processor-specific optimizations via -march=native" OFF)
option(CITRA_USE_PRECOMPILED_HEADERS "Use precompiled headers" ON)
option(CITRA_WARNINGS_AS_ERRORS "Enable warnings as errors" ON)
option(CITRA_ADDRESS_SANITIZE "Enable address sanitizer" OFF)

# Handle incompatible options for libretro builds
if(ENABLE_LIBRETRO)
    # Check for explicitly-set conflicting options
    set(_CONFLICTS "")
    foreach(_opt IN LISTS _LIBRETRO_INCOMPATIBLE_OPTIONS)
        list(FIND _USER_SET_OPTIONS ${_opt} _idx)
        if(NOT _idx EQUAL -1 AND ${_opt})
            list(APPEND _CONFLICTS ${_opt})
        endif()
    endforeach()

    if(_CONFLICTS)
        string(REPLACE ";" ", " _CONFLICTS_STR "${_CONFLICTS}")
        message(FATAL_ERROR
            "ENABLE_LIBRETRO is incompatible with: ${_CONFLICTS_STR}\n"
            "These options were explicitly enabled but are not supported for libretro builds.\n"
            "Remove these options or set them to OFF.")
    endif()

    # Force disable incompatible options (handles defaulted-on options)
    foreach(_opt IN LISTS _LIBRETRO_INCOMPATIBLE_OPTIONS)
        set(${_opt} OFF CACHE BOOL "Disabled for libretro" FORCE)
    endforeach()
endif()

# Pass the following values to C++ land
if (ENABLE_QT)
    add_definitions(-DENABLE_QT)
endif()
if (ENABLE_QT_TRANSLATION)
    add_definitions(-DENABLE_QT_TRANSLATION)
endif()
if (ENABLE_ROOM)
    add_definitions(-DENABLE_ROOM)
endif()
if (ENABLE_SDL2)
    add_definitions(-DENABLE_SDL2)
endif()

if(ENABLE_SSE42 AND (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64"))
    message(STATUS "SSE4.2 enabled for x86_64")
    if(MSVC)
        SET(SSE42_COMPILE_OPTION /arch:SSE4.2)
    else()
        SET(SSE42_COMPILE_OPTION -msse4.1 -msse4.2)
    endif()
    add_compile_options(${SSE42_COMPILE_OPTION})
    add_compile_definitions(CITRA_HAS_SSE42)
endif()

include(CitraHandleSystemLibs)

if (CITRA_USE_PRECOMPILED_HEADERS)
    message(STATUS "Using Precompiled Headers.")
    set(CMAKE_PCH_INSTANTIATE_TEMPLATES ON)

    # This ensures that pre-compiled headers won't invalidate build caches for every fresh checkout.
    if(NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND CMAKE_CXX_COMPILE_OPTIONS_CREATE_PCH -Xclang -fno-pch-timestamp)
    endif()
endif()

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/.git/hooks/pre-commit)
    message(STATUS "Copying pre-commit hook")
    file(COPY hooks/pre-commit
        DESTINATION ${PROJECT_SOURCE_DIR}/.git/hooks)
endif()

# Use ccache for android if available
# =======================================================================
if (NOT $ENV{NDK_CCACHE} EQUAL "")
    set(CCACHE_EXE $ENV{NDK_CCACHE})
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_EXE})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_EXE})
endif()

# Check for LTO support
# =======================================================================
if (ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT supported OUTPUT error_msg)

    if (supported)
        message(STATUS "LTO enabled")
    else()
        message(STATUS "LTO enabled but is unavailable, disabling: ${error_msg}")
        set(ENABLE_LTO OFF)
    endif()
else()
    message(STATUS "LTO disabled")
endif()

# Address sanitizer
# =======================================================================
if (CITRA_ADDRESS_SANITIZE)
    if (MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Check for march=native support
# =======================================================================
if (ENABLE_NATIVE_OPTIMIZATION)
    include(CheckCCompilerFlag)
    include(CheckCXXCompilerFlag)
    CHECK_C_COMPILER_FLAG("-march=native" C_COMPILER_SUPPORTS_MARCH_NATIVE)
    CHECK_CXX_COMPILER_FLAG("-march=native" CXX_COMPILER_SUPPORTS_MARCH_NATIVE)

    if (C_COMPILER_SUPPORTS_MARCH_NATIVE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
    endif()
    if (CXX_COMPILER_SUPPORTS_MARCH_NATIVE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    endif()
endif()

# Sanity check : Check that all submodules are present
# =======================================================================

function(check_submodules_present)
    file(READ "${PROJECT_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})
    foreach(module ${gitmodules})
        string(REGEX REPLACE "path *= *" "" module ${module})
        if (NOT EXISTS "${PROJECT_SOURCE_DIR}/${module}/.git")
            message(SEND_ERROR "Git submodule ${module} not found.\n"
                    "Please run: git submodule update --init --recursive")
        endif()
    endforeach()
endfunction()
if (EXISTS "${PROJECT_SOURCE_DIR}/.git/objects")
    # only check submodules when source is obtained via Git
    check_submodules_present()
endif()

configure_file(${PROJECT_SOURCE_DIR}/dist/compatibility_list/compatibility_list.qrc
               ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.qrc
               COPYONLY)
if (EXISTS ${PROJECT_SOURCE_DIR}/dist/compatibility_list/compatibility_list.json)
    file(COPY "${PROJECT_SOURCE_DIR}/dist/compatibility_list/compatibility_list.json"
        DESTINATION "${PROJECT_BINARY_DIR}/dist/compatibility_list/")
endif()
if (NOT EXISTS ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json)
    file(WRITE ${PROJECT_BINARY_DIR}/dist/compatibility_list/compatibility_list.json "")
endif()

# Detect current compilation architecture and create standard definitions
# =======================================================================

include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

if (NOT ENABLE_GENERIC)
    if (CMAKE_OSX_ARCHITECTURES)
        set(ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
    elseif (MSVC)
        detect_architecture("_M_AMD64" x86_64)
        detect_architecture("_M_IX86" x86)
        detect_architecture("_M_ARM" arm)
        detect_architecture("_M_ARM64" arm64)
    else()
        detect_architecture("__x86_64__" x86_64)
        detect_architecture("__i386__" x86)
        detect_architecture("__arm__" arm)
        detect_architecture("__aarch64__" arm64)
    endif()
endif()
if (NOT DEFINED ARCHITECTURE)
    set(ARCHITECTURE "GENERIC")
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")


# Configure C++ standard
# ===========================

# boost asio's concept usage doesn't play nicely with some compilers yet.
add_definitions(-DBOOST_ASIO_DISABLE_CONCEPTS)
# boost can have issues compiling with C++17 and up on newer versions of Clang.
add_definitions(-DBOOST_NO_CXX98_FUNCTION_BASE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Apply consistent visibility settings.
set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN NO)

# set up output paths for executable binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)

if (ENABLE_LIBRETRO)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# System imported libraries
# ======================

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (ENABLE_QT)
    if (NOT USE_SYSTEM_QT)
        download_qt(6.10.3)
    endif()

    find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia Concurrent)

    if (UNIX AND NOT APPLE)
        find_package(Qt6 REQUIRED COMPONENTS DBus)
    endif()

    if (ENABLE_QT_TRANSLATION)
        find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
    endif()

    if (NOT DEFINED QT_TARGET_PATH)
        # Determine the location of the compile target's Qt.
        get_target_property(qtcore_path Qt6::Core LOCATION_Release)
        string(FIND "${qtcore_path}" "/bin/" qtcore_path_bin_pos REVERSE)
        string(FIND "${qtcore_path}" "/lib/" qtcore_path_lib_pos REVERSE)
        if (qtcore_path_bin_pos GREATER qtcore_path_lib_pos)
            string(SUBSTRING "${qtcore_path}" 0 ${qtcore_path_bin_pos} QT_TARGET_PATH)
        else()
            string(SUBSTRING "${qtcore_path}" 0 ${qtcore_path_lib_pos} QT_TARGET_PATH)
        endif()
    endif()

    if (NOT DEFINED QT_HOST_PATH)
        # Use the same for host Qt if none is defined.
        set(QT_HOST_PATH "${QT_TARGET_PATH}")
    endif()

    message(STATUS "Using target Qt at ${QT_TARGET_PATH}")
    message(STATUS "Using host Qt at ${QT_HOST_PATH}")
endif()

# Use system tsl::robin_map if available (otherwise we fallback to version bundled with dynarmic)
find_package(tsl-robin-map QUIET)

# Platform-specific library requirements
# ======================================

if (APPLE)
    if (NOT IOS)
        # Umbrella framework for everything GUI-related
        find_library(COCOA_LIBRARY Cocoa REQUIRED)
    endif()
    find_library(AVFOUNDATION_LIBRARY AVFoundation REQUIRED)
    find_library(IOSURFACE_LIBRARY IOSurface REQUIRED)
    set(PLATFORM_LIBRARIES ${COCOA_LIBRARY} ${AVFOUNDATION_LIBRARY} ${IOSURFACE_LIBRARY})

    if (ENABLE_VULKAN AND NOT ENABLE_LIBRETRO)
        if (USE_SYSTEM_MOLTENVK)
            find_library(MOLTENVK_LIBRARY MoltenVK REQUIRED)
        else()
            download_moltenvk()
            if (IOS)
                set(MOLTENVK_RELATIVE_LIBPATH "static/MoltenVK.xcframework/ios-arm64/libMoltenVK.a")
            else()
                set(MOLTENVK_RELATIVE_LIBPATH "dynamic/dylib/macOS/libMoltenVK.dylib")
            endif()
            set(MOLTENVK_LIBRARY "${CMAKE_BINARY_DIR}/externals/MoltenVK/MoltenVK/${MOLTENVK_RELATIVE_LIBPATH}")
        endif()

        message(STATUS "Using MoltenVK at ${MOLTENVK_LIBRARY}.")
        set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} ${MOLTENVK_LIBRARY})
    endif()
elseif (WIN32)
    set(PLATFORM_LIBRARIES winmm ws2_32)
    if (MINGW)
        # PSAPI is the Process Status API
        set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} psapi imm32 version)
    endif()
elseif (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
endif()

# Setup a custom clang-format target (if clang-format can be found) that will run
# against all the src files. This should be used before making a pull request.
# =======================================================================

set(CLANG_FORMAT_POSTFIX "-15")
find_program(CLANG_FORMAT
    NAMES clang-format${CLANG_FORMAT_POSTFIX}
          clang-format
    PATHS ${PROJECT_BINARY_DIR}/externals)
# if find_program doesn't find it, try to download from externals
if (NOT CLANG_FORMAT)
    if (WIN32)
        message(STATUS "Clang format not found! Downloading...")
        set(CLANG_FORMAT "${PROJECT_BINARY_DIR}/externals/clang-format${CLANG_FORMAT_POSTFIX}.exe")
        file(DOWNLOAD
            https://github.com/citra-emu/ext-windows-bin/raw/master/clang-format${CLANG_FORMAT_POSTFIX}.exe
            "${CLANG_FORMAT}" SHOW_PROGRESS
            STATUS DOWNLOAD_SUCCESS)
        if (NOT DOWNLOAD_SUCCESS EQUAL 0)
            message(WARNING "Could not download clang format! Disabling the clang format target")
            file(REMOVE ${CLANG_FORMAT})
            unset(CLANG_FORMAT)
        endif()
    else()
        message(WARNING "Clang format not found! Disabling the clang format target")
    endif()
endif()

if (CLANG_FORMAT)
    set(SRCS ${PROJECT_SOURCE_DIR}/src)
    set(CCOMMENT "Running clang format against all the .h and .cpp files in src/")
    if (WIN32)
        if(MINGW)
            add_custom_target(clang-format
                COMMAND find `cygpath -u ${SRCS}` -iname *.h -o -iname *.cpp -o -iname *.mm | xargs `cygpath -u ${CLANG_FORMAT}` -i
                COMMENT ${CCOMMENT})
        else()
            add_custom_target(clang-format
                COMMAND powershell.exe -Command "Get-ChildItem '${SRCS}/*' -Include *.cpp,*.h,*.mm -Recurse | Foreach {&'${CLANG_FORMAT}' -i $_.fullname}"
                COMMENT ${CCOMMENT})
        endif()
    else()
        add_custom_target(clang-format
            COMMAND find ${SRCS} -iname *.h -o -iname *.cpp -o -iname *.mm | xargs ${CLANG_FORMAT} -i
            COMMENT ${CCOMMENT})
    endif()
    unset(SRCS)
    unset(CCOMMENT)
endif()

# Include source code
# ===================

# This function should be passed a list of all files in a target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(create_target_directory_groups target_name)
    # Place any files that aren't in the source list in a separate group so that they don't get in
    # the way.
    source_group("Other Files" REGULAR_EXPRESSION ".")

    get_target_property(target_sources "${target_name}" SOURCES)

    foreach(file_name IN LISTS target_sources)
        get_filename_component(dir_name "${file_name}" PATH)
        # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
        string(REPLACE "/" "\\" group_name "${dir_name}")
        source_group("${group_name}" FILES "${file_name}")
    endforeach()
endfunction()

# Gets a UTC timstamp and sets the provided variable to it
function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

# generate git/build information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REF_SPEC GIT_REV)
git_describe(GIT_DESC --always --long --dirty)
git_branch_name(GIT_BRANCH)
get_timestamp(BUILD_DATE)

# Boost
# Prevent boost from linking against libs when building
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY
    -DBOOST_SYSTEM_NO_LIB
    -DBOOST_DATE_TIME_NO_LIB
    -DBOOST_REGEX_NO_LIB
)
if (USE_SYSTEM_BOOST)
    find_package(Boost 1.70.0 COMPONENTS container locale serialization iostreams REQUIRED)
endif()

enable_testing()
add_subdirectory(externals)

# Boost (bundled)
if (NOT USE_SYSTEM_BOOST)
    add_definitions( -DBOOST_ALL_NO_LIB )
    add_library(Boost::boost ALIAS boost)
    add_library(Boost::serialization ALIAS boost_serialization)
    add_library(Boost::iostreams ALIAS boost_iostreams)
endif()

# SDL2
if (ENABLE_SDL2 AND USE_SYSTEM_SDL2)
    find_package(SDL2 REQUIRED)
    add_library(SDL2 INTERFACE)
    target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARY}")
    target_include_directories(SDL2 INTERFACE "${SDL2_INCLUDE_DIR}")
    add_library(SDL2::SDL2 ALIAS SDL2)
endif()

if (ENABLE_LIBUSB AND USE_SYSTEM_LIBUSB)
    find_package(LibUSB)
endif()

if (USE_SYSTEM_SOUNDTOUCH)
    find_package(SoundTouch REQUIRED)
    add_library(SoundTouch INTERFACE)
    target_link_libraries(SoundTouch INTERFACE "${SOUNDTOUCH_LIBRARIES}")
    target_include_directories(SoundTouch INTERFACE "${SOUNDTOUCH_INCLUDE_DIRS}")
endif()

add_subdirectory(src)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT citra_meta)

# Create target for outputting distributable bundles.
# Not supported for mobile platforms as distributables are built differently.
if (NOT ANDROID AND NOT IOS)
    include(BundleTarget)
    if (ENABLE_QT)
        qt_bundle_target(citra_meta)
    endif()
    if (ENABLE_ROOM_STANDALONE)
        bundle_target(citra_room_standalone)
    endif()
endif()

# Installation instructions
# =========================

# Install freedesktop.org metadata files, following those specifications:
# http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
# http://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
# http://standards.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html
if(ENABLE_QT AND UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/dist/azahar.desktop"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications"
            RENAME "org.azahar_emu.Azahar.desktop")
    install(FILES "${PROJECT_SOURCE_DIR}/dist/azahar.svg"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/scalable/apps"
            RENAME "org.azahar_emu.Azahar.svg")
    install(FILES "${PROJECT_SOURCE_DIR}/dist/azahar.png"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/icons/hicolor/512x512/apps"
            RENAME "org.azahar_emu.Azahar.png")
    install(FILES "${PROJECT_SOURCE_DIR}/dist/org.azahar_emu.Azahar.xml"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/mime/packages")
endif()
