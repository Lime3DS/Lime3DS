// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <array>
#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"

namespace Pica {

struct FramebufferRegs {
    enum class FragmentOperationMode : u32 {
        Default = 0,
        Gas = 1,
        Shadow = 3,
    };

    enum class LogicOp : u32 {
        Clear = 0,
        And = 1,
        AndReverse = 2,
        Copy = 3,
        Set = 4,
        CopyInverted = 5,
        NoOp = 6,
        Invert = 7,
        Nand = 8,
        Or = 9,
        Nor = 10,
        Xor = 11,
        Equiv = 12,
        AndInverted = 13,
        OrReverse = 14,
        OrInverted = 15,
    };

    enum class BlendEquation : u32 {
        Add = 0,
        Subtract = 1,
        ReverseSubtract = 2,
        Min = 3,
        Max = 4,
    };

    enum class BlendFactor : u32 {
        Zero = 0,
        One = 1,
        SourceColor = 2,
        OneMinusSourceColor = 3,
        DestColor = 4,
        OneMinusDestColor = 5,
        SourceAlpha = 6,
        OneMinusSourceAlpha = 7,
        DestAlpha = 8,
        OneMinusDestAlpha = 9,
        ConstantColor = 10,
        OneMinusConstantColor = 11,
        ConstantAlpha = 12,
        OneMinusConstantAlpha = 13,
        SourceAlphaSaturate = 14,
    };

    enum class CompareFunc : u32 {
        Never = 0,
        Always = 1,
        Equal = 2,
        NotEqual = 3,
        LessThan = 4,
        LessThanOrEqual = 5,
        GreaterThan = 6,
        GreaterThanOrEqual = 7,
    };

    enum class StencilAction : u32 {
        Keep = 0,
        Zero = 1,
        Replace = 2,
        Increment = 3,
        Decrement = 4,
        Invert = 5,
        IncrementWrap = 6,
        DecrementWrap = 7,
    };

    struct {
        union {
            BitField<0, 2, FragmentOperationMode> fragment_operation_mode;
            // If false, logic blending is used
            BitField<8, 1, u32> alphablend_enable;
        };

        union {
            BitField<0, 3, BlendEquation> blend_equation_rgb;
            BitField<8, 3, BlendEquation> blend_equation_a;

            BitField<16, 4, BlendFactor> factor_source_rgb;
            BitField<20, 4, BlendFactor> factor_dest_rgb;

            BitField<24, 4, BlendFactor> factor_source_a;
            BitField<28, 4, BlendFactor> factor_dest_a;
        } alpha_blending;

        union {
            BitField<0, 4, LogicOp> logic_op;
        };

        union {
            u32 raw;
            BitField<0, 8, u32> r;
            BitField<8, 8, u32> g;
            BitField<16, 8, u32> b;
            BitField<24, 8, u32> a;
        } blend_const;

        union {
            BitField<0, 1, u32> enable;
            BitField<4, 3, CompareFunc> func;
            BitField<8, 8, u32> ref;
        } alpha_test;

        struct {
            union {
                // Raw value of this register
                u32 raw_func;

                // If true, enable stencil testing
                BitField<0, 1, u32> enable;

                // Comparison operation for stencil testing
                BitField<4, 3, CompareFunc> func;

                // Mask used to control writing to the stencil buffer
                BitField<8, 8, u32> write_mask;

                // Value to compare against for stencil testing
                BitField<16, 8, u32> reference_value;

                // Mask to apply on stencil test inputs
                BitField<24, 8, u32> input_mask;
            };

            union {
                // Raw value of this register
                u32 raw_op;

                // Action to perform when the stencil test fails
                BitField<0, 3, StencilAction> action_stencil_fail;

                // Action to perform when stencil testing passed but depth testing fails
                BitField<4, 3, StencilAction> action_depth_fail;

                // Action to perform when both stencil and depth testing pass
                BitField<8, 3, StencilAction> action_depth_pass;
            };
        } stencil_test;

        union {
            u32 depth_color_mask;
            BitField<0, 1, u32> depth_test_enable;
            BitField<4, 3, CompareFunc> depth_test_func;
            BitField<8, 1, u32> red_enable;
            BitField<9, 1, u32> green_enable;
            BitField<10, 1, u32> blue_enable;
            BitField<11, 1, u32> alpha_enable;
            BitField<12, 1, u32> depth_write_enable;
        };

        INSERT_PADDING_WORDS(0x8);
    } output_merger;

    // Components are laid out in reverse byte order, most significant bits first.
    enum class ColorFormat : u32 {
        RGBA8 = 0,
        RGB8 = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4 = 4,
    };

    enum class DepthFormat : u32 {
        D16 = 0,
        D24 = 2,
        D24S8 = 3,
    };

    // Returns the number of bytes in the specified color format
    static unsigned BytesPerColorPixel(ColorFormat format) {
        switch (format) {
        case ColorFormat::RGBA8:
            return 4;
        case ColorFormat::RGB8:
            return 3;
        case ColorFormat::RGB5A1:
        case ColorFormat::RGB565:
        case ColorFormat::RGBA4:
            return 2;
        default:
            LOG_CRITICAL(HW_GPU, "Unknown color format {}", format);
            UNIMPLEMENTED();
        }
    }

    struct FramebufferConfig {
        INSERT_PADDING_WORDS(0x3);

        union {
            BitField<0, 4, u32> allow_color_write; // 0 = disable, else enable
        };

        INSERT_PADDING_WORDS(0x1);

        union {
            BitField<0, 2, u32> allow_depth_stencil_write; // 0 = disable, else enable
        };

        BitField<0, 2, DepthFormat> depth_format;

        BitField<16, 3, ColorFormat> color_format;

        INSERT_PADDING_WORDS(0x4);

        BitField<0, 28, u32> depth_buffer_address;
        BitField<0, 28, u32> color_buffer_address;

        union {
            // Apparently, the framebuffer width is stored as expected,
            // while the height is stored as the actual height minus one.
            // Hence, don't access these fields directly but use the accessors
            // GetWidth() and GetHeight() instead.
            BitField<0, 11, u32> width;
            BitField<12, 10, u32> height;
        };

        INSERT_PADDING_WORDS(0x1);

        inline PAddr GetColorBufferPhysicalAddress() const {
            return color_buffer_address * 8;
        }
        inline PAddr GetDepthBufferPhysicalAddress() const {
            return depth_buffer_address * 8;
        }

        inline u32 GetWidth() const {
            return width;
        }

        inline u32 GetHeight() const {
            return height + 1;
        }
    } framebuffer;

    // Returns the number of bytes in the specified depth format
    static u32 BytesPerDepthPixel(DepthFormat format) {
        switch (format) {
        case DepthFormat::D16:
            return 2;
        case DepthFormat::D24:
            return 3;
        case DepthFormat::D24S8:
            return 4;
        }

        ASSERT_MSG(false, "Unknown depth format {}", format);
    }

    // Returns the number of bits per depth component of the specified depth format
    static u32 DepthBitsPerPixel(DepthFormat format) {
        switch (format) {
        case DepthFormat::D16:
            return 16;
        case DepthFormat::D24:
        case DepthFormat::D24S8:
            return 24;
        default:
            UNREACHABLE_MSG("Unknown depth format {}", format);
        }

        return 0;
    }

    [[nodiscard]] bool IsShadowRendering() const {
        return output_merger.fragment_operation_mode == FragmentOperationMode::Shadow;
    }

    [[nodiscard]] bool HasStencil() const {
        return framebuffer.depth_format == DepthFormat::D24S8;
    };

    INSERT_PADDING_WORDS(0x10); // Gas related registers

    union {
        BitField<0, 16, u32> constant; // float1.5.10
        BitField<16, 16, u32> linear;  // float1.5.10
    } shadow;

    INSERT_PADDING_WORDS(0xF);
};

static_assert(sizeof(FramebufferRegs) == 0x40 * sizeof(u32),
              "FramebufferRegs struct has incorrect size");

} // namespace Pica
