// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstring>

#include "common/common_types.h"
#include "common/swap.h"
#include "common/vector_math.h"

namespace Common::Color {

/// Convert a 1-bit color component to 8 bit
[[nodiscard]] constexpr u8 Convert1To8(u8 value) {
    return value * 255;
}

/// Convert a 4-bit color component to 8 bit
[[nodiscard]] constexpr u8 Convert4To8(u8 value) {
    return (value << 4) | value;
}

/// Convert a 5-bit color component to 8 bit
[[nodiscard]] constexpr u8 Convert5To8(u8 value) {
    return (value << 3) | (value >> 2);
}

/// Convert a 6-bit color component to 8 bit
[[nodiscard]] constexpr u8 Convert6To8(u8 value) {
    return (value << 2) | (value >> 4);
}

/// Convert a 8-bit color component to 1 bit
[[nodiscard]] constexpr u8 Convert8To1(u8 value) {
    return value >> 7;
}

/// Convert a 8-bit color component to 4 bit
[[nodiscard]] constexpr u8 Convert8To4(u8 value) {
    return value >> 4;
}

/// Convert a 8-bit color component to 5 bit
[[nodiscard]] constexpr u8 Convert8To5(u8 value) {
    return value >> 3;
}

/// Convert a 8-bit color component to 6 bit
[[nodiscard]] constexpr u8 Convert8To6(u8 value) {
    return value >> 2;
}

/// Averages the RGB components of a color
[[nodiscard]] constexpr u8 AverageRgbComponents(const Common::Vec4<u8>& color) {
    return (static_cast<u32>(color.r()) + color.g() + color.b()) / 3;
}

/**
 * Decode a color stored in RGBA8 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeRGBA8(const u8* bytes) {
    return {bytes[3], bytes[2], bytes[1], bytes[0]};
}

/**
 * Decode a color stored in RGB8 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeRGB8(const u8* bytes) {
    return {bytes[2], bytes[1], bytes[0], 255};
}

/**
 * Decode a color stored in RG8 (aka HILO8) format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeRG8(const u8* bytes) {
    return {bytes[1], bytes[0], 0, 255};
}

/**
 * Decode a color stored in RGB565 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeRGB565(const u8* bytes) {
    u16_le pixel;
    std::memcpy(&pixel, bytes, sizeof(pixel));
    return {Convert5To8((pixel >> 11) & 0x1F), Convert6To8((pixel >> 5) & 0x3F),
            Convert5To8(pixel & 0x1F), 255};
}

/**
 * Decode a color stored in RGB5A1 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeRGB5A1(const u8* bytes) {
    u16_le pixel;
    std::memcpy(&pixel, bytes, sizeof(pixel));
    return {Convert5To8((pixel >> 11) & 0x1F), Convert5To8((pixel >> 6) & 0x1F),
            Convert5To8((pixel >> 1) & 0x1F), Convert1To8(pixel & 0x1)};
}

/**
 * Decode a color stored in RGBA4 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeRGBA4(const u8* bytes) {
    u16_le pixel;
    std::memcpy(&pixel, bytes, sizeof(pixel));
    return {Convert4To8((pixel >> 12) & 0xF), Convert4To8((pixel >> 8) & 0xF),
            Convert4To8((pixel >> 4) & 0xF), Convert4To8(pixel & 0xF)};
}

/**
 * Decode a color stored in IA8 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeIA8(const u8* bytes) {
    return {bytes[1], bytes[1], bytes[1], bytes[0]};
}

/**
 * Decode a color stored in I8 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeI8(const u8* bytes) {
    return {bytes[0], bytes[0], bytes[0], 255};
}

/**
 * Decode a color stored in A8 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeA8(const u8* bytes) {
    return {0, 0, 0, bytes[0]};
}

/**
 * Decode a color stored in IA4 format
 * @param bytes Pointer to encoded source color
 * @return Result color decoded as Common::Vec4<u8>
 */
[[nodiscard]] inline Common::Vec4<u8> DecodeIA4(const u8* bytes) {
    u8 i = Common::Color::Convert4To8((bytes[0] & 0xF0) >> 4);
    u8 a = Common::Color::Convert4To8(bytes[0] & 0x0F);
    return {i, i, i, a};
}

/**
 * Decode a depth value stored in D16 format
 * @param bytes Pointer to encoded source value
 * @return Depth value as an u32
 */
[[nodiscard]] inline u32 DecodeD16(const u8* bytes) {
    u16_le data;
    std::memcpy(&data, bytes, sizeof(data));
    return data;
}

/**
 * Decode a depth value stored in D24 format
 * @param bytes Pointer to encoded source value
 * @return Depth value as an u32
 */
[[nodiscard]] inline u32 DecodeD24(const u8* bytes) {
    return (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
}

/**
 * Decode a depth value and a stencil value stored in D24S8 format
 * @param bytes Pointer to encoded source values
 * @return Resulting values stored as a Common::Vec2
 */
[[nodiscard]] inline Common::Vec2<u32> DecodeD24S8(const u8* bytes) {
    return {static_cast<u32>((bytes[2] << 16) | (bytes[1] << 8) | bytes[0]), bytes[3]};
}

/**
 * Encode a color as RGBA8 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeRGBA8(const Common::Vec4<u8>& color, u8* bytes) {
    bytes[3] = color.r();
    bytes[2] = color.g();
    bytes[1] = color.b();
    bytes[0] = color.a();
}

/**
 * Encode a color as RGB8 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeRGB8(const Common::Vec4<u8>& color, u8* bytes) {
    bytes[2] = color.r();
    bytes[1] = color.g();
    bytes[0] = color.b();
}

/**
 * Encode a color as RG8 (aka HILO8) format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeRG8(const Common::Vec4<u8>& color, u8* bytes) {
    bytes[1] = color.r();
    bytes[0] = color.g();
}

/**
 * Encode a color as RGB565 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeRGB565(const Common::Vec4<u8>& color, u8* bytes) {
    const u16_le data =
        (Convert8To5(color.r()) << 11) | (Convert8To6(color.g()) << 5) | Convert8To5(color.b());

    std::memcpy(bytes, &data, sizeof(data));
}

/**
 * Encode a color as RGB5A1 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeRGB5A1(const Common::Vec4<u8>& color, u8* bytes) {
    const u16_le data = (Convert8To5(color.r()) << 11) | (Convert8To5(color.g()) << 6) |
                        (Convert8To5(color.b()) << 1) | Convert8To1(color.a());

    std::memcpy(bytes, &data, sizeof(data));
}

/**
 * Encode a color as RGBA4 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeRGBA4(const Common::Vec4<u8>& color, u8* bytes) {
    const u16 data = (Convert8To4(color.r()) << 12) | (Convert8To4(color.g()) << 8) |
                     (Convert8To4(color.b()) << 4) | Convert8To4(color.a());

    std::memcpy(bytes, &data, sizeof(data));
}

/**
 * Encode a color as IA8 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeIA8(const Common::Vec4<u8>& color, u8* bytes) {
    bytes[1] = AverageRgbComponents(color);
    bytes[0] = color.a();
}

/**
 * Encode a color as I8 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeI8(const Common::Vec4<u8>& color, u8* bytes) {
    bytes[0] = AverageRgbComponents(color);
}

/**
 * Encode a color as A8 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeA8(const Common::Vec4<u8>& color, u8* bytes) {
    bytes[0] = color.a();
}

/**
 * Encode a color as IA4 format
 * @param color Source color to encode
 * @param bytes Destination pointer to store encoded color
 */
inline void EncodeIA4(const Common::Vec4<u8>& color, u8* bytes) {
    bytes[0] = (Convert8To4(AverageRgbComponents(color)) << 4) | Convert8To4(color.a());
}

/**
 * Encode a 16 bit depth value as D16 format
 * @param value 16 bit source depth value to encode
 * @param bytes Pointer where to store the encoded value
 */
inline void EncodeD16(u32 value, u8* bytes) {
    const u16_le data = static_cast<u16>(value);
    std::memcpy(bytes, &data, sizeof(data));
}

/**
 * Encode a 24 bit depth value as D24 format
 * @param value 24 bit source depth value to encode
 * @param bytes Pointer where to store the encoded value
 */
inline void EncodeD24(u32 value, u8* bytes) {
    bytes[0] = value & 0xFF;
    bytes[1] = (value >> 8) & 0xFF;
    bytes[2] = (value >> 16) & 0xFF;
}

/**
 * Encode a 24 bit depth and 8 bit stencil values as D24S8 format
 * @param depth 24 bit source depth value to encode
 * @param stencil 8 bit source stencil value to encode
 * @param bytes Pointer where to store the encoded value
 */
inline void EncodeD24S8(u32 depth, u8 stencil, u8* bytes) {
    bytes[0] = depth & 0xFF;
    bytes[1] = (depth >> 8) & 0xFF;
    bytes[2] = (depth >> 16) & 0xFF;
    bytes[3] = stencil;
}

/**
 * Encode a 24 bit depth value as D24X8 format (32 bits per pixel with 8 bits unused)
 * @param depth 24 bit source depth value to encode
 * @param bytes Pointer where to store the encoded value
 * @note unused bits will not be modified
 */
inline void EncodeD24X8(u32 depth, u8* bytes) {
    bytes[0] = depth & 0xFF;
    bytes[1] = (depth >> 8) & 0xFF;
    bytes[2] = (depth >> 16) & 0xFF;
}

/**
 * Encode an 8 bit stencil value as X24S8 format (32 bits per pixel with 24 bits unused)
 * @param stencil 8 bit source stencil value to encode
 * @param bytes Pointer where to store the encoded value
 * @note unused bits will not be modified
 */
inline void EncodeX24S8(u8 stencil, u8* bytes) {
    bytes[3] = stencil;
}

} // namespace Common::Color
