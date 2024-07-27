// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/texture.h"
#include "core/frontend/image_interface.h"
#include "video_core/custom_textures/material.h"

namespace VideoCore {

namespace {

CustomPixelFormat ToCustomPixelFormat(ddsktx_format format) {
    switch (format) {
    case DDSKTX_FORMAT_RGBA8:
        return CustomPixelFormat::RGBA8;
    case DDSKTX_FORMAT_BC1:
        return CustomPixelFormat::BC1;
    case DDSKTX_FORMAT_BC3:
        return CustomPixelFormat::BC3;
    case DDSKTX_FORMAT_BC5:
        return CustomPixelFormat::BC5;
    case DDSKTX_FORMAT_BC7:
        return CustomPixelFormat::BC7;
    case DDSKTX_FORMAT_ASTC4x4:
        return CustomPixelFormat::ASTC4;
    case DDSKTX_FORMAT_ASTC6x6:
        return CustomPixelFormat::ASTC6;
    case DDSKTX_FORMAT_ASTC8x6:
        return CustomPixelFormat::ASTC8;
    default:
        LOG_ERROR(Common, "Unknown dds/ktx pixel format {}", format);
        return CustomPixelFormat::RGBA8;
    }
}

std::string_view MapTypeName(MapType type) {
    switch (type) {
    case MapType::Color:
        return "Color";
    case MapType::Normal:
        return "Normal";
    default:
        return "Invalid";
    }
}

} // Anonymous namespace

CustomTexture::CustomTexture(Frontend::ImageInterface& image_interface_)
    : image_interface{image_interface_} {}

CustomTexture::~CustomTexture() = default;

void CustomTexture::LoadFromDisk(bool flip_png) {
    std::scoped_lock lock{decode_mutex};
    if (IsLoaded()) {
        return;
    }

    FileUtil::IOFile file{path, "rb"};
    std::vector<u8> input(file.GetSize());
    if (file.ReadBytes(input.data(), input.size()) != input.size()) {
        LOG_CRITICAL(Render, "Failed to open custom texture: {}", path);
        return;
    }
    switch (file_format) {
    case CustomFileFormat::PNG:
        LoadPNG(input, flip_png);
        break;
    case CustomFileFormat::DDS:
    case CustomFileFormat::KTX:
        LoadDDS(input);
        break;
    default:
        LOG_ERROR(Render, "Unknown file format {}", file_format);
    }
}

void CustomTexture::LoadPNG(std::span<const u8> input, bool flip_png) {
    if (!image_interface.DecodePNG(data, width, height, input)) {
        LOG_ERROR(Render, "Failed to decode png: {}", path);
        return;
    }
    if (flip_png) {
        Common::FlipRGBA8Texture(data, width, height);
    }
    format = CustomPixelFormat::RGBA8;
}

void CustomTexture::LoadDDS(std::span<const u8> input) {
    ddsktx_format dds_format{};
    image_interface.DecodeDDS(data, width, height, dds_format, input);
    format = ToCustomPixelFormat(dds_format);
}

void Material::LoadFromDisk(bool flip_png) noexcept {
    if (IsDecoded()) {
        return;
    }
    for (CustomTexture* const texture : textures) {
        if (!texture || texture->IsLoaded()) {
            continue;
        }
        texture->LoadFromDisk(flip_png);
        size += texture->data.size();
        LOG_DEBUG(Render, "Loading {} map {}", MapTypeName(texture->type), texture->path);
    }
    if (!textures[0]) {
        LOG_ERROR(Render, "Unable to create material without color texture!");
        state = DecodeState::Failed;
        return;
    }
    width = textures[0]->width;
    height = textures[0]->height;
    format = textures[0]->format;
    for (const CustomTexture* texture : textures) {
        if (!texture) {
            continue;
        }
        if (texture->width != width || texture->height != height) {
            LOG_ERROR(Render,
                      "{} map {} of material with hash {:#016X} has dimentions {}x{} "
                      "which do not match the color texture dimentions {}x{}",
                      MapTypeName(texture->type), texture->path, hash, texture->width,
                      texture->height, width, height);
            state = DecodeState::Failed;
            return;
        }
        if (texture->format != format) {
            LOG_ERROR(
                Render, "{} map {} is stored with {} format which does not match color format {}",
                MapTypeName(texture->type), texture->path,
                CustomPixelFormatAsString(texture->format), CustomPixelFormatAsString(format));
            state = DecodeState::Failed;
            return;
        }
    }
    state = DecodeState::Decoded;
}

void Material::AddMapTexture(CustomTexture* texture) noexcept {
    const std::size_t index = static_cast<std::size_t>(texture->type);
    if (textures[index]) {
        LOG_ERROR(Render, "Textures {} and {} are assigned to the same material, ignoring!",
                  textures[index]->path, texture->path);
        return;
    }
    textures[index] = texture;
}

} // namespace VideoCore
