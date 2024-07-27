// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <string>
#include <vector>
#include <dds-ktx.h>
#include "common/common_types.h"

namespace Frontend {

/**
 * Utility class that provides image decoding/encoding to the custom texture manager.
 * Can be optionally overriden by frontends to provide a custom implementation.
 */
class ImageInterface {
public:
    virtual ~ImageInterface() = default;

    virtual bool DecodePNG(std::vector<u8>& dst, u32& width, u32& height, std::span<const u8> src);
    virtual bool DecodeDDS(std::vector<u8>& dst, u32& width, u32& height, ddsktx_format& format,
                           std::span<const u8> src);
    virtual bool EncodePNG(const std::string& path, u32 width, u32 height, std::span<const u8> src);
};

} // namespace Frontend
