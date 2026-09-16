# Definitions for all external bundled libraries

# Suppress warnings from external libraries
if (MSVC)
    add_compile_options(/W0)
else()
    add_compile_options(-w)
endif()

function(target_disable_warnings target)
    if (MSVC)
        target_compile_options(${target} INTERFACE /W0)
    else()
        target_compile_options(${target} INTERFACE -w)
    endif()
endfunction()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMakeModules)
include(DownloadExternals)
include(ExternalProject)

# Boost
if (USE_SYSTEM_BOOST)
    unset(BOOST_ROOT CACHE)
    unset(Boost_INCLUDE_DIR CACHE)
    set(Boost_NO_SYSTEM_PATHS OFF CACHE BOOL "" FORCE)
else()
    message(STATUS "Including vendored Boost library")
    set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/externals/boost" CACHE STRING "")
    set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/externals/boost" CACHE STRING "")
    set(Boost_NO_SYSTEM_PATHS ON CACHE BOOL "")
    add_library(boost INTERFACE)
    target_include_directories(boost INTERFACE ${Boost_INCLUDE_DIR})
    target_disable_warnings(boost)

    # Boost::serialization
    file(GLOB boost_serialization_SRC "${CMAKE_SOURCE_DIR}/externals/boost/libs/serialization/src/*.cpp")
    add_library(boost_serialization STATIC ${boost_serialization_SRC})
    target_link_libraries(boost_serialization PUBLIC boost)

    # Boost::iostreams
    add_library(
        boost_iostreams
        STATIC
        ${CMAKE_SOURCE_DIR}/externals/boost/libs/iostreams/src/file_descriptor.cpp
        ${CMAKE_SOURCE_DIR}/externals/boost/libs/iostreams/src/mapped_file.cpp
    )
    target_link_libraries(boost_iostreams PUBLIC boost)
    # Add additional boost libs here; remember to ALIAS them in the root CMakeLists!
endif()

# Catch2
if (ENABLE_TESTS)
    add_library(catch2 INTERFACE)
    if(USE_SYSTEM_CATCH2)
        find_package(Catch2 3.0.0 REQUIRED)
    else()
        set(CATCH_INSTALL_DOCS OFF CACHE BOOL "")
        set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "")
        add_subdirectory(catch2 EXCLUDE_FROM_ALL)
    endif()
    target_link_libraries(catch2 INTERFACE Catch2::Catch2WithMain)
    include(Catch)
endif()

# Crypto++
if(USE_SYSTEM_CRYPTOPP)
    find_package(cryptopp REQUIRED)
    add_library(cryptopp INTERFACE)
    target_link_libraries(cryptopp INTERFACE cryptopp::cryptopp)
else()
    set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "")
    set(CRYPTOPP_INSTALL OFF CACHE BOOL "")
    add_subdirectory(cryptopp EXCLUDE_FROM_ALL)
endif()

# dds-ktx
add_library(dds-ktx INTERFACE)
target_include_directories(dds-ktx INTERFACE ./dds-ktx)
target_disable_warnings(dds-ktx)

# fmt and Xbyak need to be added before dynarmic
# libfmt
if(USE_SYSTEM_FMT)
    add_library(fmt INTERFACE)
    find_package(fmt REQUIRED)
    target_link_libraries(fmt INTERFACE fmt::fmt)
else()
    option(FMT_INSTALL "" ON)
    add_subdirectory(fmt EXCLUDE_FROM_ALL)
endif()


# Xbyak
if ("x86_64" IN_LIST ARCHITECTURE)
    if(USE_SYSTEM_XBYAK)
        find_package(xbyak REQUIRED)
        add_library(xbyak INTERFACE)
        target_link_libraries(xbyak INTERFACE xbyak::xbyak)
    else()
        add_subdirectory(xbyak EXCLUDE_FROM_ALL)
    endif()
endif()

# Oaknut
if ("arm64" IN_LIST ARCHITECTURE)
    if(USE_SYSTEM_OAKNUT)
        find_package(oaknut REQUIRED)
        add_library(oaknut INTERFACE)
        target_link_libraries(oaknut INTERFACE merry::oaknut)
    else()
        add_subdirectory(oaknut EXCLUDE_FROM_ALL)
    endif()
endif()

# Dynarmic
if ("x86_64" IN_LIST ARCHITECTURE OR "arm64" IN_LIST ARCHITECTURE)
    if(USE_SYSTEM_DYNARMIC)
        find_package(dynarmic REQUIRED)
        add_library(dynarmic INTERFACE)
        target_link_libraries(dynarmic INTERFACE dynarmic::dynarmic)
        # The dynarmic package's cmake files are helpfully completely silent
        # so we have to inform the user of its status ourselves
        if(TARGET dynarmic::dynarmic)
            message(STATUS "Found dynarmic")
        endif()
    else()
        set(DYNARMIC_TESTS OFF CACHE BOOL "")
        set(DYNARMIC_FRONTENDS "A32" CACHE STRING "")
        set(DYNARMIC_USE_PRECOMPILED_HEADERS ${CITRA_USE_PRECOMPILED_HEADERS} CACHE BOOL "")
        add_subdirectory(dynarmic EXCLUDE_FROM_ALL)
    endif()
endif()

# getopt
if (MSVC)
    add_subdirectory(getopt EXCLUDE_FROM_ALL)
endif()

# inih
if(USE_SYSTEM_INIH)
    find_package(inih REQUIRED COMPONENTS inih inir)
    add_library(inih INTERFACE)
    target_link_libraries(inih INTERFACE inih::inih inih::inir)
else()
    add_subdirectory(inih EXCLUDE_FROM_ALL)
endif()

# MicroProfile
add_library(microprofile INTERFACE)
target_include_directories(microprofile INTERFACE ./microprofile)
target_disable_warnings(microprofile)
if (ENABLE_MICROPROFILE)
    target_compile_definitions(microprofile INTERFACE MICROPROFILE_ENABLED=1)
else()
    target_compile_definitions(microprofile INTERFACE MICROPROFILE_ENABLED=0)
endif()

# Nihstro
add_library(nihstro-headers INTERFACE)
target_include_directories(nihstro-headers INTERFACE ./nihstro/include)
target_disable_warnings(nihstro-headers)
if (NOT MSVC)
    # TODO: For some reason MSYS2 still applied this warnin even with -w
    target_compile_options(nihstro-headers INTERFACE -Wno-invalid-specialization)
endif()

# Open Source Archives
add_subdirectory(open_source_archives EXCLUDE_FROM_ALL)

# faad2
add_subdirectory(faad2 EXCLUDE_FROM_ALL)

# Dynamic library headers
add_library(library-headers INTERFACE)

if (USE_SYSTEM_FFMPEG_HEADERS)
    find_path(SYSTEM_FFMPEG_INCLUDES NAMES libavutil/avutil.h)
    if (SYSTEM_FFMPEG_INCLUDES STREQUAL "SYSTEM_FFMPEG_INCLUDES-NOTFOUND")
        message(WARNING "System FFmpeg headers not found. Falling back on bundled headers.")
    else()
        message(STATUS "Using system FFmpeg headers.")
        target_include_directories(library-headers SYSTEM INTERFACE ${SYSTEM_FFMPEG_INCLUDES})
        set(FOUND_FFMPEG_HEADERS ON)
    endif()
endif()
if (NOT FOUND_FFMPEG_HEADERS)
    message(STATUS "Using bundled ffmpeg headers.")
    target_include_directories(library-headers INTERFACE ./library-headers/ffmpeg/include)
    target_disable_warnings(library-headers)
endif()

# SoundTouch
if(NOT USE_SYSTEM_SOUNDTOUCH)
    set(INTEGER_SAMPLES ON CACHE BOOL "")
    set(SOUNDSTRETCH OFF CACHE BOOL "")
    set(SOUNDTOUCH_DLL OFF CACHE BOOL "")
    add_subdirectory(soundtouch EXCLUDE_FROM_ALL)
    target_compile_definitions(SoundTouch PUBLIC SOUNDTOUCH_INTEGER_SAMPLES)
endif()

# Teakra
set(BUILD_TESTING OFF CACHE BOOL "")
add_subdirectory(teakra EXCLUDE_FROM_ALL)

# SDL2
if (ENABLE_SDL2 AND NOT USE_SYSTEM_SDL2)
    add_subdirectory(sdl2 EXCLUDE_FROM_ALL)
endif()

# libusb
if (ENABLE_LIBUSB AND NOT USE_SYSTEM_LIBUSB)
    add_subdirectory(libusb EXCLUDE_FROM_ALL)
    set(LIBUSB_INCLUDE_DIR "" PARENT_SCOPE)
    set(LIBUSB_LIBRARIES usb PARENT_SCOPE)
endif()

# Zstandard
if(USE_SYSTEM_ZSTD)
    find_package(zstd REQUIRED)
    add_library(zstd INTERFACE)
    if(TARGET zstd::libzstd_shared)
        message(STATUS "Found system Zstandard")
    endif()
    target_link_libraries(zstd INTERFACE zstd::libzstd_shared)
else()
    set(ZSTD_LEGACY_SUPPORT OFF)
    set(ZSTD_BUILD_PROGRAMS OFF)
    set(ZSTD_BUILD_SHARED OFF)
    add_subdirectory(zstd/build/cmake EXCLUDE_FROM_ALL)

    target_include_directories(libzstd_static INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/externals/zstd/lib>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/externals/zstd/lib/common>
    )

    add_library(zstd_seekable STATIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zstd/contrib/seekable_format/zstdseek_compress.c>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zstd/contrib/seekable_format/zstdseek_decompress.c>
    )
    target_include_directories(zstd_seekable PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zstd/contrib/seekable_format>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zstd/lib/common>
    )
    target_link_libraries(zstd_seekable PUBLIC libzstd_static)

    add_library(zstd INTERFACE)
    target_link_libraries(zstd INTERFACE libzstd_static zstd_seekable)
endif()

# ENet
if(USE_SYSTEM_ENET)
    find_package(libenet REQUIRED)
    add_library(enet INTERFACE)
    target_link_libraries(enet INTERFACE libenet::libenet)
else()
    add_subdirectory(enet EXCLUDE_FROM_ALL)
    target_include_directories(enet INTERFACE ./enet/include)
endif()

# Cubeb
if (ENABLE_CUBEB)
    if(USE_SYSTEM_CUBEB)
        find_package(cubeb REQUIRED)
        add_library(cubeb INTERFACE)
        target_link_libraries(cubeb INTERFACE cubeb::cubeb)
        if(TARGET cubeb::cubeb)
            message(STATUS "Found system cubeb")
        endif()
    else()
        set(BUILD_TESTS OFF CACHE BOOL "")
        set(BUILD_TOOLS OFF CACHE BOOL "")
        set(BUNDLE_SPEEX ON CACHE BOOL "")
        add_subdirectory(cubeb EXCLUDE_FROM_ALL)
    endif()
endif()

# DiscordRPC
if (ENABLE_DISCORD_RPC)
    # rapidjson used by discord-rpc is old and doesn't correctly detect endianness for some platforms.
    include(TestBigEndian)
    test_big_endian(RAPIDJSON_BIG_ENDIAN)
    if(RAPIDJSON_BIG_ENDIAN)
        add_compile_definitions(RAPIDJSON_ENDIAN=1)
    else()
        add_compile_definitions(RAPIDJSON_ENDIAN=0)
    endif()

    # Apply a dummy CLANG_FORMAT_SUFFIX to disable discord-rpc's unnecessary automatic clang-format.
    set(CLANG_FORMAT_SUFFIX "dummy")

    add_subdirectory(discord-rpc EXCLUDE_FROM_ALL)
    target_include_directories(discord-rpc INTERFACE ./discord-rpc/include)
endif()

# LibRetro
if (ENABLE_LIBRETRO)
  add_library(libretro INTERFACE)
  target_include_directories(libretro INTERFACE ./libretro-common/libretro-common/include)
  if (ANDROID)
    add_subdirectory(libretro-common EXCLUDE_FROM_ALL)
  endif()
endif()

# RetroAchievements
if (ENABLE_RETRO_ACHIEVEMENTS)
  add_subdirectory(rcheevos EXCLUDE_FROM_ALL)
endif()

# JSON
add_library(json-headers INTERFACE)
if (USE_SYSTEM_JSON)
    find_package(nlohmann_json REQUIRED)
    target_link_libraries(json-headers INTERFACE nlohmann_json::nlohmann_json)
    get_target_property(NLOHMANN_PREFIX nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
    # The nlohmann-json3 package expects "#include <nlohmann/json.hpp>"
    # Citra uses "#include <json.hpp>" so we have to add this manually
    target_include_directories(json-headers SYSTEM INTERFACE "${NLOHMANN_PREFIX}/nlohmann")
else()
    target_include_directories(json-headers INTERFACE ./json)
    target_disable_warnings(json-headers)
endif()

# OpenSSL
if (USE_SYSTEM_OPENSSL)
    find_package(OpenSSL 1.1)
    set(OPENSSL_LIBRARIES OpenSSL::SSL OpenSSL::Crypto)
else()
    # LibreSSL
    set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "")
    set(OPENSSLDIR "/etc/ssl/")
    add_subdirectory(libressl EXCLUDE_FROM_ALL)
    target_include_directories(ssl INTERFACE ./libressl/include)
    target_disable_warnings(ssl)
    target_compile_definitions(ssl PRIVATE -DHAVE_INET_NTOP)
    get_directory_property(OPENSSL_LIBRARIES
        DIRECTORY libressl
        DEFINITION OPENSSL_LIBS)
endif()

# httplib
add_library(httplib INTERFACE)
if(USE_SYSTEM_CPP_HTTPLIB)
    find_package(CppHttp 0.14.1)
    # Detect if system cpphttplib is a shared library
    # this breaks building as Citra relies on functions that are moved
    # into the shared object.
    get_target_property(HTTP_LIBS httplib::httplib INTERFACE_LINK_LIBRARIES)
    if(HTTP_LIBS)
        message(WARNING "Shared cpp-http (${HTTP_LIBS}) not supported. Falling back to bundled...")
        target_include_directories(httplib INTERFACE ./httplib)
        target_disable_warnings(httplib)
    else()
        if(CppHttp_FOUND)
            target_link_libraries(httplib INTERFACE httplib::httplib)
        else()
            message(STATUS "Cpp-httplib not found or not suitable version! Falling back to bundled...")
            target_include_directories(httplib INTERFACE ./httplib)
            target_disable_warnings(httplib)
            endif()
    endif()
else()
    target_include_directories(httplib INTERFACE ./httplib)
    target_disable_warnings(httplib)
endif()
target_compile_options(httplib INTERFACE -DCPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(httplib INTERFACE ${OPENSSL_LIBRARIES})

if (UNIX AND NOT APPLE)
    add_subdirectory(gamemode EXCLUDE_FROM_ALL)
endif()

# cpp-jwt
if (ENABLE_WEB_SERVICE)
    if (USE_SYSTEM_CPP_JWT)
        find_package(cpp-jwt REQUIRED)
        add_library(cpp-jwt INTERFACE)
        target_link_libraries(cpp-jwt INTERFACE cpp-jwt::cpp-jwt)
    else()
        add_library(cpp-jwt INTERFACE)
        target_include_directories(cpp-jwt INTERFACE ./cpp-jwt/include)
        target_disable_warnings(cpp-jwt)
        target_compile_definitions(cpp-jwt INTERFACE CPP_JWT_USE_VENDORED_NLOHMANN_JSON)
    endif()
endif()

# lodepng
if(USE_SYSTEM_LODEPNG)
    add_library(lodepng INTERFACE)
    find_package(lodepng REQUIRED)
    target_link_libraries(lodepng INTERFACE lodepng::lodepng)
else()
    add_subdirectory(lodepng EXCLUDE_FROM_ALL)
endif()

# (xperia64): Only use libyuv on Android b/c of build issues on Windows and mandatory JPEG
if(ANDROID)
    # libyuv
    add_subdirectory(libyuv EXCLUDE_FROM_ALL)
    target_include_directories(yuv INTERFACE ./libyuv/include)
endif()

# OpenAL Soft
if (ENABLE_OPENAL)
    if(USE_SYSTEM_OPENAL)
        add_library(OpenAL INTERFACE)
        find_package(OpenAL REQUIRED)
        target_link_libraries(OpenAL INTERFACE OpenAL::OpenAL)
    else()
        if (BSD STREQUAL "OpenBSD")
            set(ALSOFT_BACKEND_SOLARIS OFF CACHE BOOL "")
        endif()
        set(ALSOFT_EMBED_HRTF_DATA OFF CACHE BOOL "")
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "")
        set(ALSOFT_INSTALL OFF CACHE BOOL "")
        set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "")
        set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "")
        set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "")
        set(ALSOFT_UTILS OFF CACHE BOOL "")
        set(LIBTYPE "STATIC" CACHE STRING "")
        add_subdirectory(openal-soft EXCLUDE_FROM_ALL)
    endif()
endif()

# OpenGL dependencies
if (ENABLE_OPENGL)
    # Glad
    add_subdirectory(glad EXCLUDE_FROM_ALL)
endif()

# Vulkan dependencies
if (ENABLE_VULKAN)
    # spirv-headers
    if(USE_SYSTEM_SPIRV_HEADERS)
        find_package(SPIRV-Headers REQUIRED)
        if(TARGET SPIRV-Headers::SPIRV-Headers)
            message(STATUS "Found SPIRV headers")
            get_target_property(SPIRV-Headers_SOURCE_DIR SPIRV-Headers::SPIRV-Headers INTERFACE_INCLUDE_DIRECTORIES)
            set(SPIRV-Headers_SOURCE_DIR "${SPIRV-Headers_SOURCE_DIR}/../") # Not sure why this is necessary
        endif()
    else()
        set(SPIRV-Headers_SOURCE_DIR "${CMAKE_SOURCE_DIR}/externals/spirv-headers")
        add_subdirectory(spirv-headers EXCLUDE_FROM_ALL)
    endif()

    # spirv-tools
    # TODO: Implement USE_SYSTEM_SPIRV_TOOLS -OS
    set(SPIRV_SKIP_EXECUTABLES ON)
    add_subdirectory(spirv-tools EXCLUDE_FROM_ALL)

    # glslang
    if(USE_SYSTEM_GLSLANG)
        find_package(glslang REQUIRED)
        add_library(glslang INTERFACE)
        add_library(SPIRV INTERFACE)
        target_link_libraries(glslang INTERFACE glslang::glslang)
        target_link_libraries(SPIRV INTERFACE glslang::SPIRV)
        # System include path is different from submodule include path
        get_target_property(GLSLANG_PREFIX glslang::SPIRV INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(SPIRV SYSTEM INTERFACE "${GLSLANG_PREFIX}/glslang")
    else()
        set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "")
        set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "")
        set(ENABLE_SPVREMAPPER OFF CACHE BOOL "")
        set(ENABLE_CTEST OFF CACHE BOOL "")
        set(ENABLE_HLSL OFF CACHE BOOL "")
        set(BUILD_EXTERNAL OFF CACHE BOOL "")
        add_subdirectory(glslang EXCLUDE_FROM_ALL)
    endif()

    # sirit
    add_subdirectory(sirit EXCLUDE_FROM_ALL)

    # VMA
    if(USE_SYSTEM_VMA)
        add_library(vma INTERFACE)
        find_package(VulkanMemoryAllocator REQUIRED)
        if(TARGET GPUOpen::VulkanMemoryAllocator)
            message(STATUS "Found VulkanMemoryAllocator")
            target_link_libraries(vma INTERFACE GPUOpen::VulkanMemoryAllocator)
        endif()
    else()
        add_library(vma INTERFACE)
        target_include_directories(vma INTERFACE ./vma/include)
        target_disable_warnings(vma)
    endif()

    # vulkan-headers
    add_library(vulkan-headers INTERFACE)
    if(USE_SYSTEM_VULKAN_HEADERS)
        find_package(Vulkan REQUIRED)
        if(TARGET Vulkan::Headers)
            message(STATUS "Found Vulkan headers")
            target_link_libraries(vulkan-headers INTERFACE Vulkan::Headers)
        endif()
    else()
        target_include_directories(vulkan-headers INTERFACE ./vulkan-headers/include)
        target_disable_warnings(vulkan-headers)
        if (BSD STREQUAL "NetBSD")
            # There may be a better way to do this with
            # find_package(X11), but I couldn't get
            # CMake to do it, so we're depending on
            # the x11-links package and assuming the
            # prefix location. -OS
            target_include_directories(vulkan-headers INTERFACE
                /usr/pkg/share/x11-links/include)
        elseif (BSD STREQUAL "OpenBSD")
            # This is fine to hardcode because it'll never change
            target_include_directories(vulkan-headers INTERFACE
                /usr/X11R6/include)
        endif()
    endif()

    # adrenotools
    if (ANDROID AND "arm64" IN_LIST ARCHITECTURE)
        add_subdirectory(libadrenotools EXCLUDE_FROM_ALL)
    endif()
endif()

set(XXHASH_BUILD_XXHSUM OFF)
add_subdirectory(xxHash/cmake_unofficial EXCLUDE_FROM_ALL)
target_compile_definitions(xxhash PRIVATE XXH_FORCE_MEMORY_ACCESS=2)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(xxhash PRIVATE XXH_VECTOR=XXH_SSE2)
    message(STATUS "Enabling SSE2 for xxHash")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|armv8")
    target_compile_definitions(xxhash PRIVATE XXH_VECTOR=XXH_NEON)
    message(STATUS "Enabling NEON for xxHash")
else()
    target_compile_definitions(xxhash PRIVATE XXH_VECTOR=XXH_SCALAR)
    message(STATUS "Disabling SIMD for xxHash")
endif()
