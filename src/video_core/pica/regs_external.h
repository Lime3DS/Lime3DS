// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <fmt/format.h>

#include <string>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"

namespace Pica {

/**
 * Most physical addresses which GPU registers refer to are 8-byte aligned.
 * This function should be used to get the address from a raw register value.
 */
constexpr u32 DecodeAddressRegister(u32 register_value) {
    return register_value * 8;
}

/// Components are laid out in reverse byte order, most significant bits first.
enum class PixelFormat : u32 {
    RGBA8 = 0,
    RGB8 = 1,
    RGB565 = 2,
    RGB5A1 = 3,
    RGBA4 = 4,
};

constexpr u32 BytesPerPixel(Pica::PixelFormat format) {
    switch (format) {
    case Pica::PixelFormat::RGBA8:
        return 4;
    case Pica::PixelFormat::RGB8:
        return 3;
    case Pica::PixelFormat::RGB565:
    case Pica::PixelFormat::RGB5A1:
    case Pica::PixelFormat::RGBA4:
        return 2;
    default:
        UNREACHABLE();
    }

    return 0;
}

struct MemoryFillConfig {
    u32 address_start;
    u32 address_end;

    union {
        u32 value_32bit;

        BitField<0, 16, u32> value_16bit;

        // TODO: Verify component order
        BitField<0, 8, u32> value_24bit_r;
        BitField<8, 8, u32> value_24bit_g;
        BitField<16, 8, u32> value_24bit_b;
    };

    union {
        u32 control;

        // Setting this field to 1 triggers the memory fill.
        // This field also acts as a status flag, and gets reset to 0 upon completion.
        BitField<0, 1, u32> trigger;
        // Set to 1 upon completion.
        BitField<1, 1, u32> finished;
        // If both of these bits are unset, then it will fill the memory with a 16 bit value
        // 1: fill with 24-bit wide values
        BitField<8, 1, u32> fill_24bit;
        // 1: fill with 32-bit wide values
        BitField<9, 1, u32> fill_32bit;
    };

    inline u32 GetStartAddress() const {
        return DecodeAddressRegister(address_start);
    }

    inline u32 GetEndAddress() const {
        return DecodeAddressRegister(address_end);
    }

    inline std::string DebugName() const {
        return fmt::format("from {:#X} to {:#X} with {}-bit value {:#X}", GetStartAddress(),
                           GetEndAddress(), fill_32bit ? "32" : (fill_24bit ? "24" : "16"),
                           value_32bit);
    }
};
static_assert(sizeof(MemoryFillConfig) == 0x10);

struct FramebufferConfig {
    INSERT_PADDING_WORDS(0x17);

    union {
        u32 size;

        BitField<0, 16, u32> width;
        BitField<16, 16, u32> height;
    };

    INSERT_PADDING_WORDS(0x2);

    u32 address_left1;
    u32 address_left2;

    union {
        u32 format;

        BitField<0, 3, PixelFormat> color_format;
    };

    INSERT_PADDING_WORDS(0x1);

    union {
        u32 active_fb;

        // 0: Use parameters ending with "1"
        // 1: Use parameters ending with "2"
        BitField<0, 1, u32> second_fb_active;
    };

    INSERT_PADDING_WORDS(0x5);

    // Distance between two pixel rows, in bytes
    u32 stride;

    u32 address_right1;
    u32 address_right2;

    INSERT_PADDING_WORDS(0x19);
};
static_assert(sizeof(FramebufferConfig) == 0x100);

struct DisplayTransferConfig {
    u32 input_address;
    u32 output_address;

    inline u32 GetPhysicalInputAddress() const {
        return DecodeAddressRegister(input_address);
    }

    inline u32 GetPhysicalOutputAddress() const {
        return DecodeAddressRegister(output_address);
    }

    inline std::string DebugName() const noexcept {
        return fmt::format("from {:#x} to {:#x} with {} scaling and stride {}, width {}",
                           GetPhysicalInputAddress(), GetPhysicalOutputAddress(),
                           scaling == NoScale ? "no" : (scaling == ScaleX ? "X" : "XY"),
                           input_width.Value(), output_width.Value());
    }

    union {
        u32 output_size;

        BitField<0, 16, u32> output_width;
        BitField<16, 16, u32> output_height;
    };

    union {
        u32 input_size;

        BitField<0, 16, u32> input_width;
        BitField<16, 16, u32> input_height;
    };

    enum ScalingMode : u32 {
        NoScale = 0, // Doesn't scale the image
        ScaleX = 1,  // Downscales the image in half in the X axis and applies a box filter
        ScaleXY =
            2, // Downscales the image in half in both the X and Y axes and applies a box filter
    };

    union {
        u32 flags;

        BitField<0, 1, u32> flip_vertically; // flips input data vertically
        BitField<1, 1, u32> input_linear;    // Converts from linear to tiled format
        BitField<2, 1, u32> crop_input_lines;
        BitField<3, 1, u32> is_texture_copy; // Copies the data without performing any
                                             // processing and respecting texture copy fields
        BitField<5, 1, u32> dont_swizzle;
        BitField<8, 3, PixelFormat> input_format;
        BitField<12, 3, PixelFormat> output_format;
        /// Uses some kind of 32x32 block swizzling mode, instead of the usual 8x8 one.
        BitField<16, 1, u32> block_32;        // TODO(yuriks): unimplemented
        BitField<24, 2, ScalingMode> scaling; // Determines the scaling mode of the transfer
    };

    INSERT_PADDING_WORDS(0x1);

    // it seems that writing to this field triggers the display transfer
    BitField<0, 1, u32> trigger;

    INSERT_PADDING_WORDS(0x1);

    struct {
        u32 size; // The lower 4 bits are ignored

        union {
            u32 input_size;

            BitField<0, 16, u32> input_width;
            BitField<16, 16, u32> input_gap;
        };

        union {
            u32 output_size;

            BitField<0, 16, u32> output_width;
            BitField<16, 16, u32> output_gap;
        };
    } texture_copy;
};
static_assert(sizeof(DisplayTransferConfig) == 0x2c);

} // namespace Pica
