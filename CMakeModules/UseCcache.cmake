# Copyright Citra Emulator Project / Azahar Emulator Project
# Licensed under GPLv2 or any later version
# Refer to the license.txt file included.

option(USE_CCACHE "Use ccache to speed up subsequent builds" OFF)
set(CCACHE_PATH "ccache" CACHE STRING "Path/name of an installed ccache binary")
if (USE_CCACHE)
    find_program(CCACHE_EXE ${CCACHE_PATH})
    if (CCACHE_EXE)
        message(STATUS "CCache found at: ${CCACHE_EXE}")
        set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_EXE})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_EXE})
    else()
        message(FATAL_ERROR "USE_CCACHE enabled, but no ccache found at ${CCACHE_PATH}")
    endif()
endif()
