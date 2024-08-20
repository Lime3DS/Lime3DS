// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdlib>
#include "common/common_funcs.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"

// For asserts we'd like to keep all the junk executed when an assert happens away from the
// important code in the function. One way of doing this is to put all the relevant code inside a
// lambda and force the compiler to not inline it.

#define ASSERT(_a_)                                                                                \
    do                                                                                             \
        if (!(_a_)) [[unlikely]] {                                                                 \
            []() LIME3DS_NO_INLINE LIME3DS_NO_RETURN {                                             \
                LOG_CRITICAL(Debug, "Assertion Failed!");                                          \
                Common::Log::Stop();                                                               \
                Crash();                                                                           \
                exit(1);                                                                           \
            }();                                                                                   \
        }                                                                                          \
    while (0)

#define ASSERT_MSG(_a_, ...)                                                                       \
    do                                                                                             \
        if (!(_a_)) [[unlikely]] {                                                                 \
            [&]() LIME3DS_NO_INLINE LIME3DS_NO_RETURN {                                            \
                LOG_CRITICAL(Debug, "Assertion Failed!\n" __VA_ARGS__);                            \
                Common::Log::Stop();                                                               \
                Crash();                                                                           \
                exit(1);                                                                           \
            }();                                                                                   \
        }                                                                                          \
    while (0)

#define UNREACHABLE()                                                                              \
    ([]() LIME3DS_NO_INLINE LIME3DS_NO_RETURN {                                                    \
        LOG_CRITICAL(Debug, "Unreachable code!");                                                  \
        Common::Log::Stop();                                                                       \
        Crash();                                                                                   \
        exit(1);                                                                                   \
    }())

#define UNREACHABLE_MSG(...)                                                                       \
    ([&]() LIME3DS_NO_INLINE LIME3DS_NO_RETURN {                                                   \
        LOG_CRITICAL(Debug, "Unreachable code!\n" __VA_ARGS__);                                    \
        Common::Log::Stop();                                                                       \
        Crash();                                                                                   \
        exit(1);                                                                                   \
    }())

#ifdef _DEBUG
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#else // not debug
#define DEBUG_ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, _desc_, ...)
#endif

#define UNIMPLEMENTED() LOG_CRITICAL(Debug, "Unimplemented code!")
#define UNIMPLEMENTED_MSG(_a_, ...) LOG_CRITICAL(Debug, _a_, __VA_ARGS__)
