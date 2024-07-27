option(USE_SYSTEM_LIBS "Use system libraries over bundled ones" OFF)

# System library options
CMAKE_DEPENDENT_OPTION(USE_SYSTEM_QT "Use the system Qt lib (instead of the bundled one)" OFF "ENABLE_QT;MSVC OR APPLE" ON)
CMAKE_DEPENDENT_OPTION(USE_SYSTEM_MOLTENVK "Use the system MoltenVK lib (instead of the bundled one)" OFF "APPLE" OFF)
option(USE_SYSTEM_SDL2 "Use the system SDL2 lib (instead of the bundled one)" OFF)
option(USE_SYSTEM_BOOST "Use the system Boost libs (instead of the bundled ones)" OFF)
option(USE_SYSTEM_OPENSSL "Use the system OpenSSL libs (instead of the bundled LibreSSL)" OFF)
option(USE_SYSTEM_LIBUSB "Use the system libusb (instead of the bundled libusb)" OFF)
option(USE_SYSTEM_CPP_JWT "Use the system cpp-jwt (instead of the bundled one)" OFF)
option(USE_SYSTEM_SOUNDTOUCH "Use the system SoundTouch (instead of the bundled one)" OFF)
option(USE_SYSTEM_CPP_HTTPLIB "Use the system cpp-httplib (instead of the bundled one)" OFF)
option(USE_SYSTEM_JSON "Use the system JSON (nlohmann-json3) package (instead of the bundled one)" OFF)
option(USE_SYSTEM_DYNARMIC "Use the system dynarmic (instead of the bundled one)" OFF)
option(USE_SYSTEM_FMT "Use the system fmt (instead of the bundled one)" OFF)
option(USE_SYSTEM_XBYAK "Use the system xbyak (instead of the bundled one)" OFF)
option(USE_SYSTEM_INIH "Use the system inih (instead of the bundled one)" OFF)
option(USE_SYSTEM_FFMPEG_HEADERS "Use the system FFmpeg headers (instead of the bundled one)" OFF)
option(USE_SYSTEM_GLSLANG "Use the system glslang and SPIR-V libraries (instead of the bundled ones)" OFF)
option(USE_SYSTEM_ZSTD "Use the system Zstandard library (instead of the bundled one)" OFF)
option(USE_SYSTEM_ENET "Use the system libenet (instead of the bundled one)" OFF)
option(USE_SYSTEM_CRYPTOPP "Use the system cryptopp (instead of the bundled one)" OFF)
option(USE_SYSTEM_CUBEB "Use the system cubeb (instead of the bundled one)" OFF)
option(USE_SYSTEM_LODEPNG "Use the system lodepng (instead of the bundled one)" OFF)
option(USE_SYSTEM_OPENAL "Use the system OpenAL (instead of the bundled one)" OFF)
option(USE_SYSTEM_VMA "Use the system VulkanMemoryAllocator (instead of the bundled one)" OFF)
option(USE_SYSTEM_VULKAN_HEADERS "Use the system Vulkan headers (instead of the bundled ones)" OFF)
option(USE_SYSTEM_CATCH2 "Use the system Catch2 (instead of the bundled one)" OFF)

# Qt and MoltenVK are handled separately
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_SDL2 "Disable system SDL2" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_BOOST "Disable system Boost" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_OPENSSL "Disable system OpenSSL" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_LIBUSB "Disable system LibUSB" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_CPP_JWT "Disable system cpp-jwt" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_SOUNDTOUCH "Disable system SoundTouch" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_CPP_HTTPLIB "Disable system cpp-httplib" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_JSON "Disable system JSON" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_DYNARMIC "Disable system Dynarmic" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_FMT "Disable system fmt" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_XBYAK "Disable system xbyak" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_INIH "Disable system inih" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_FFMPEG_HEADERS "Disable system ffmpeg" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_GLSLANG "Disable system glslang" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_ZSTD "Disable system Zstandard" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_ENET "Disable system libenet" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_CRYPTOPP "Disable system cryptopp" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_CUBEB "Disable system cubeb" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_LODEPNG "Disable system lodepng" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_OPENAL "Disable system OpenAL" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_VMA "Disable system VulkanMemoryAllocator" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_VULKAN_HEADERS "Disable system Vulkan headers" OFF "USE_SYSTEM_LIBS" OFF)
CMAKE_DEPENDENT_OPTION(DISABLE_SYSTEM_CATCH2 "Disable system Catch2" OFF "USE_SYSTEM_LIBS" OFF)

set(LIB_VAR_LIST
    SDL2
    BOOST
    OPENSSL
    LIBUSB
    CPP_JWT
    SOUNDTOUCH
    CPP_HTTPLIB
    JSON
    DYNARMIC
    FMT
    XBYAK
    INIH
    FFMPEG_HEADERS
    GLSLANG
    ZSTD
    ENET
    CRYPTOPP
    CUBEB
    LODEPNG
    OPENAL
    VMA
    VULKAN_HEADERS
    CATCH2
    )

# First, check that USE_SYSTEM_XXX is not used with USE_SYSTEM_LIBS

if(USE_SYSTEM_LIBS)
    foreach(CURRENT_LIB IN LISTS LIB_VAR_LIST)
        if(USE_SYSTEM_${CURRENT_LIB})
            unset(USE_SYSTEM_${CURRENT_LIB})
        endif()
    endforeach()

    # Next, set which libraries to use

    foreach(CURRENT_LIB IN LISTS LIB_VAR_LIST)
        if(NOT DISABLE_SYSTEM_${CURRENT_LIB})
            set(USE_SYSTEM_${CURRENT_LIB} ON CACHE BOOL "Using system ${CURRENT_LIB}" FORCE)
        else()
            # Explicitly disable this in case of multiple CMake invocations
            set(USE_SYSTEM_${CURRENT_LIB} OFF CACHE BOOL "Using system ${CURRENT_LIB}" FORCE)
        endif()
    endforeach()
endif()
