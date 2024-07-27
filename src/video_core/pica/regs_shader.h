// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/vector_math.h"

namespace Pica {

struct ShaderRegs {
    BitField<0, 16, u32> bool_uniforms;

    union {
        BitField<0, 8, u32> x;
        BitField<8, 8, u32> y;
        BitField<16, 8, u32> z;
        BitField<24, 8, u32> w;
    } int_uniforms[4];

    Common::Vec4<u8> GetIntUniform(u32 index) const {
        const auto& values = int_uniforms[index];
        return Common::MakeVec<u8>(values.x, values.y, values.z, values.w);
    }

    INSERT_PADDING_WORDS(0x4);

    enum ShaderMode {
        GS = 0x08,
        VS = 0xA0,
    };

    union {
        // Number of input attributes to shader unit - 1
        u32 input_buffer_config;
        BitField<0, 4, u32> max_input_attribute_index;
        BitField<8, 8, u32> input_to_uniform;
        BitField<24, 8, ShaderMode> shader_mode;
    };

    // Offset to shader program entry point (in words)
    BitField<0, 16, u32> main_offset;

    /// Maps input attributes to registers. 4-bits per attribute, specifying a register index
    u32 input_attribute_to_register_map_low;
    u32 input_attribute_to_register_map_high;

    u32 GetRegisterForAttribute(std::size_t attribute_index) const {
        const u64 map = (static_cast<u64>(input_attribute_to_register_map_high) << 32) |
                        static_cast<u64>(input_attribute_to_register_map_low);
        return static_cast<u32>((map >> (attribute_index * 4)) & 0b1111);
    }

    BitField<0, 16, u32> output_mask;

    // 0x28E, CODETRANSFER_END
    INSERT_PADDING_WORDS(0x2);

    struct {
        enum class Format : u32 {
            Float24 = 0,
            Float32 = 1,
        };

        bool IsFloat32() const {
            return format == Format::Float32;
        }

        union {
            // Index of the next uniform to write to
            // TODO: ctrulib uses 8 bits for this, however that seems to yield lots of invalid
            // indices
            // TODO: Maybe the uppermost index is for the geometry shader? Investigate!
            BitField<0, 7, u32> index;
            BitField<31, 1, Format> format;
        };

        // Writing to these registers sets the current uniform.
        u32 set_value[8];

    } uniform_setup;

    INSERT_PADDING_WORDS(0x2);

    struct {
        // Offset of the next instruction to write code to.
        // Incremented with each instruction write.
        u32 offset;

        // Writing to these registers sets the "current" word in the shader program.
        u32 set_word[8];
    } program;

    INSERT_PADDING_WORDS(0x1);

    // This register group is used to load an internal table of swizzling patterns,
    // which are indexed by each shader instruction to specify vector component swizzling.
    struct {
        // Offset of the next swizzle pattern to write code to.
        // Incremented with each instruction write.
        u32 offset;

        // Writing to these registers sets the current swizzle pattern in the table.
        u32 set_word[8];
    } swizzle_patterns;

    INSERT_PADDING_WORDS(0x2);
};

static_assert(sizeof(ShaderRegs) == 0x30 * sizeof(u32), "ShaderRegs struct has incorrect size");

} // namespace Pica
