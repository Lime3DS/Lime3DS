// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>
#include <fmt/format.h>

// adapted from https://github.com/fmtlib/fmt/issues/2704
// a generic formatter for enum classes
#if FMT_VERSION >= 80100
template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_enum_v<T>, char>>
    : fmt::formatter<std::underlying_type_t<T>> {
    template <typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::formatter<std::underlying_type_t<T>>::format(
            static_cast<std::underlying_type_t<T>>(value), ctx);
    }
};
#endif
