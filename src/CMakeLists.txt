# Enable modules to include each other's files
include_directories(.)

# CMake seems to only define _DEBUG on Windows
set_property(DIRECTORY APPEND PROPERTY
    COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG> $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

if (ENABLE_LTO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Set compilation flags
if (MSVC)
    set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)

    # Silence "deprecation" warnings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)

    # Avoid windows.h junk
    add_definitions(-DNOMINMAX)

    # Avoid windows.h from including some usually unused libs like winsocks.h, since this might cause some redefinition errors.
    add_definitions(-DWIN32_LEAN_AND_MEAN)

    # Ensure that projects build with Unicode support.
    add_definitions(-DUNICODE -D_UNICODE)

    # /W3                 - Level 3 warnings
    # /MP                 - Multi-threaded compilation
    # /Zi                 - Output debugging information
    # /Zm                 - Specifies the precompiled header memory allocation limit
    # /Zo                 - Enhanced debug info for optimized builds
    # /permissive-        - Enables stricter C++ standards conformance checks
    # /EHsc               - C++-only exception handling semantics
    # /utf-8              - Set source and execution character sets to UTF-8
    # /volatile:iso       - Use strict standards-compliant volatile semantics.
    # /Zc:externConstexpr - Allow extern constexpr variables to have external linkage, like the standard mandates
    # /Zc:inline          - Let codegen omit inline functions in object files
    # /Zc:throwingNew     - Let codegen assume `operator new` (without std::nothrow) will never return null
    # /GT                 - Supports fiber safety for data allocated using static thread-local storage
    add_compile_options(
        /MP
        /Zm200
        /Zo
        /permissive-
        /EHsc
        /utf-8
        /volatile:iso
        /Zc:externConstexpr
        /Zc:inline
        /Zc:throwingNew
        /GT

        # Some flags for more deterministic builds, to aid caching.
        /experimental:deterministic
        /d1trimfile:"${CMAKE_SOURCE_DIR}"

        # External headers diagnostics
        /experimental:external  # Enables the external headers options. This option isn't required in Visual Studio 2019 version 16.10 and later
        /external:anglebrackets # Treats all headers included by #include <header>, where the header file is enclosed in angle brackets (< >), as external headers
        /external:W0            # Sets the default warning level to 0 for external headers, effectively turning off warnings for external headers

        # Warnings
        /W3

        /we4062 # Enumerator 'identifier' in a switch of enum 'enumeration' is not handled
        /we4189 # 'identifier': local variable is initialized but not referenced
        /we4265 # 'class': class has virtual functions, but destructor is not virtual
        /we4388 # 'expression': signed/unsigned mismatch
        /we4389 # 'operator': signed/unsigned mismatch
        /we4456 # Declaration of 'identifier' hides previous local declaration
        /we4457 # Declaration of 'identifier' hides function parameter
        # /we4458 TODO: Enable me # Declaration of 'identifier' hides class member
        /we4459 # Declaration of 'identifier' hides global declaration
        /we4505 # 'function': unreferenced local function has been removed
        /we4547 # 'operator': operator before comma has no effect; expected operator with side-effect
        /we4549 # 'operator1': operator before comma has no effect; did you intend 'operator2'?
        /we4555 # Expression has no effect; expected expression with side-effect
        /we4826 # Conversion from 'type1' to 'type2' is sign-extended. This may cause unexpected runtime behavior.
        /we5038 # data member 'member1' will be initialized after data member 'member2'
        /we5233 # explicit lambda capture 'identifier' is not used
        /we5245 # 'function': unreferenced function with internal linkage has been removed

        /wd4100 # 'identifier': unreferenced formal parameter
        /wd4324 # 'struct_name': structure was padded due to __declspec(align())
    )

    if (CITRA_WARNINGS_AS_ERRORS)
        add_compile_options(/WX)
    endif()

    # Since MSVC's debugging information is not very deterministic, so we have to disable it
    # when using ccache or other caching tools
    if (CMAKE_C_COMPILER_LAUNCHER STREQUAL "ccache" OR CMAKE_CXX_COMPILER_LAUNCHER STREQUAL "ccache"
            OR CITRA_USE_PRECOMPILED_HEADERS)
        # Precompiled headers are deleted if not using /Z7. See https://github.com/nanoant/CMakePCHCompiler/issues/21
        add_compile_options(/Z7)
    else()
        add_compile_options(/Zi)
    endif()

    # /GS- - No stack buffer overflow checks
    add_compile_options("$<$<CONFIG:Release>:/GS->")

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "/DEBUG /MANIFEST:NO" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /MANIFEST:NO /INCREMENTAL:NO /OPT:REF,ICF /PDBALTPATH:%_PDB%" CACHE STRING "" FORCE)
else()
    add_compile_options(
        -Wall
        # In case a flag isn't supported on e.g. a certain architecture, don't error.
        -Wno-unused-command-line-argument
        # Build fortification options
        -Wp,-D_GLIBCXX_ASSERTIONS
        -fstack-protector-strong
        -fstack-clash-protection
    )

    if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
        # _FORTIFY_SOURCE can't be used without optimizations.
        add_compile_options(-D_FORTIFY_SOURCE=2)
    endif()

    if (CITRA_WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()

    if (APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        add_compile_options("-stdlib=libc++")
    endif()

    if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
        # GCC may warn when it ignores attributes like maybe_unused,
        # which is a problem for older versions (e.g. GCC 11).
        add_compile_options("-Wno-attributes")
        add_compile_options("-Wno-interference-size")
    endif()

    if (MINGW)
        add_definitions(-DMINGW_HAS_SECURE_API)
        if (COMPILE_WITH_DWARF)
            add_compile_options("-gdwarf")
        endif()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
        # Set file offset size to 64 bits.
        #
        # On modern Unixes, this is typically already the case. The lone exception is
        # glibc, which may default to 32 bits. glibc allows this to be configured
        # by setting _FILE_OFFSET_BITS.
        add_definitions(-D_FILE_OFFSET_BITS=64)

        # GNU ar: Create thin archive files.
        # Requires binutils-2.19 or later.
        set(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> qcTP <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_C_ARCHIVE_APPEND   "<CMAKE_AR> qTP  <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcTP <TARGET> <LINK_FLAGS> <OBJECTS>")
        set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> qTP  <TARGET> <LINK_FLAGS> <OBJECTS>")
    endif()
endif()

if(ENABLE_SOFTWARE_RENDERER)
    add_compile_definitions(ENABLE_SOFTWARE_RENDERER)
endif()
if(ENABLE_OPENGL)
    add_compile_definitions(ENABLE_OPENGL)
endif()
if(ENABLE_VULKAN)
    add_compile_definitions(ENABLE_VULKAN)
endif()

add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(video_core)
add_subdirectory(audio_core)
add_subdirectory(network)
add_subdirectory(input_common)

if (ENABLE_TESTS)
    add_subdirectory(tests)
endif()

if (ENABLE_SDL2 AND ENABLE_SDL2_FRONTEND)
    add_subdirectory(lime)
endif()

if (ENABLE_QT)
    add_subdirectory(lime_qt)
endif()

if (ENABLE_DEDICATED_ROOM)
    add_subdirectory(dedicated_room)
endif()

if (ANDROID)
    add_subdirectory(android/app/src/main/jni)
    target_include_directories(lime-android PRIVATE android/app/src/main)
endif()

if (ENABLE_WEB_SERVICE)
    add_subdirectory(web_service)
endif()
