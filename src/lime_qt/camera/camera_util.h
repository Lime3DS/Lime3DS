// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

class QImage;

namespace CameraUtil {

/// Converts QImage to a yuv formatted std::vector
std::vector<u16> Rgb2Yuv(const QImage& source, int width, int height);

/// Processes the QImage (resizing, flipping ...) and converts it to a std::vector
std::vector<u16> ProcessImage(const QImage& source, int width, int height, bool output_rgb,
                              bool flip_horizontal, bool flip_vertical);

} // namespace CameraUtil
