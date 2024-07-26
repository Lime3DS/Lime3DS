// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/custom_textures/custom_format.h"

namespace VideoCore {

std::string_view CustomPixelFormatAsString(CustomPixelFormat format) {
    switch (format) {
    case CustomPixelFormat::RGBA8:
        return "RGBA8";
    case CustomPixelFormat::BC1:
        return "BC1";
    case CustomPixelFormat::BC3:
        return "BC3";
    case CustomPixelFormat::BC5:
        return "BC5";
    case CustomPixelFormat::BC7:
        return "BC7";
    case CustomPixelFormat::ASTC4:
        return "ASTC4";
    case CustomPixelFormat::ASTC6:
        return "ASTC6";
    case CustomPixelFormat::ASTC8:
        return "ASTC8";
    default:
        return "NotReal";
    }
}

bool IsCustomFormatCompressed(CustomPixelFormat format) {
    return format != CustomPixelFormat::RGBA8 && format != CustomPixelFormat::Invalid;
}

} // namespace VideoCore
