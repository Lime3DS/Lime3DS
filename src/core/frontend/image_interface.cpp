// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#define DDSKTX_IMPLEMENT
#include <dds-ktx.h>
#include <lodepng.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/frontend/image_interface.h"

namespace Frontend {

bool ImageInterface::DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                               std::span<const u8> src) {
    const u32 lodepng_ret = lodepng::decode(dst, width, height, src.data(), src.size());
    if (lodepng_ret) {
        LOG_ERROR(Frontend, "Failed to decode because {}", lodepng_error_text(lodepng_ret));
        return false;
    }
    return true;
}

bool ImageInterface::DecodeDDS(std::vector<u8>& dst, u32& width, u32& height, ddsktx_format& format,
                               std::span<const u8> src) {
    ddsktx_texture_info tc{};
    const int size = static_cast<int>(src.size());

    if (!ddsktx_parse(&tc, src.data(), size, nullptr)) {
        LOG_ERROR(Frontend, "Failed to decode");
        return false;
    }
    width = tc.width;
    height = tc.height;
    format = tc.format;

    ddsktx_sub_data sub_data{};
    ddsktx_get_sub(&tc, &sub_data, src.data(), size, 0, 0, 0);

    dst.resize(sub_data.size_bytes);
    std::memcpy(dst.data(), sub_data.buff, sub_data.size_bytes);

    return true;
}

bool ImageInterface::EncodePNG(const std::string& path, u32 width, u32 height,
                               std::span<const u8> src) {
    std::vector<u8> out;
    const u32 lodepng_ret = lodepng::encode(out, src.data(), width, height);
    if (lodepng_ret) {
        LOG_ERROR(Frontend, "Failed to encode {} because {}", path,
                  lodepng_error_text(lodepng_ret));
        return false;
    }

    FileUtil::IOFile file{path, "wb"};
    if (file.WriteBytes(out.data(), out.size()) != out.size()) {
        LOG_ERROR(Frontend, "Failed to save encode to path {}", path);
        return false;
    }

    return true;
}

} // namespace Frontend
