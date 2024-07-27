// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <span>
#include <string>
#include <vector>
#include "video_core/custom_textures/custom_format.h"

namespace Frontend {
class ImageInterface;
}

namespace VideoCore {

enum class MapType : u32 {
    Color = 0,
    Normal = 1,
    MapCount = 2,
};
constexpr std::size_t MAX_MAPS = static_cast<std::size_t>(MapType::MapCount);

enum class DecodeState : u32 {
    None = 0,
    Pending = 1,
    Decoded = 2,
    Failed = 3,
};

class CustomTexture {
public:
    explicit CustomTexture(Frontend::ImageInterface& image_interface);
    ~CustomTexture();

    void LoadFromDisk(bool flip_png);

    [[nodiscard]] bool IsParsed() const noexcept {
        return file_format != CustomFileFormat::None && !hashes.empty();
    }

    [[nodiscard]] bool IsLoaded() const noexcept {
        return !data.empty();
    }

private:
    void LoadPNG(std::span<const u8> input, bool flip_png);

    void LoadDDS(std::span<const u8> input);

public:
    Frontend::ImageInterface& image_interface;
    std::string path;
    u32 width;
    u32 height;
    std::vector<u64> hashes;
    std::mutex decode_mutex;
    CustomPixelFormat format;
    CustomFileFormat file_format;
    std::vector<u8> data;
    MapType type;
};

struct Material {
    u32 width;
    u32 height;
    u64 size;
    u64 hash;
    CustomPixelFormat format;
    std::array<CustomTexture*, MAX_MAPS> textures;
    std::atomic<DecodeState> state{};

    void LoadFromDisk(bool flip_png) noexcept;

    void AddMapTexture(CustomTexture* texture) noexcept;

    [[nodiscard]] CustomTexture* Map(MapType type) const noexcept {
        return textures.at(static_cast<std::size_t>(type));
    }

    [[nodiscard]] bool IsPending() const noexcept {
        return state == DecodeState::Pending;
    }

    [[nodiscard]] bool IsFailed() const noexcept {
        return state == DecodeState::Failed;
    }

    [[nodiscard]] bool IsDecoded() const noexcept {
        return state == DecodeState::Decoded;
    }

    [[nodiscard]] bool IsUnloaded() const noexcept {
        return state == DecodeState::None;
    }
};

} // namespace VideoCore
