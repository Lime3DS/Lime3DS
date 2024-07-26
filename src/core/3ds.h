// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Core {

// 3DS Video Constants
// -------------------

// NOTE: The LCDs actually rotate the image 90 degrees when displaying. Because of that the
// framebuffers in video memory are stored in column-major order and rendered sideways, causing
// the widths and heights of the framebuffers read by the LCD to be switched compared to the
// heights and widths of the screens listed here.
constexpr int kScreenTopWidth = 400;     ///< 3DS top screen width
constexpr int kScreenTopHeight = 240;    ///< 3DS top screen height
constexpr int kScreenBottomWidth = 320;  ///< 3DS bottom screen width
constexpr int kScreenBottomHeight = 240; ///< 3DS bottom screen height

} // namespace Core
