// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace CiTrace {

// NOTE: Things are stored in little-endian

#pragma pack(1)

struct CTHeader {
    static const char* ExpectedMagicWord() {
        return "CiTr";
    }

    static u32 ExpectedVersion() {
        return 1;
    }

    char magic[4];
    u32 version;
    u32 header_size;

    struct {
        // NOTE: Register range sizes are technically hardware-constants, but the actual limits
        // aren't known. Hence we store the presumed limits along the offsets.
        // Sizes are given in u32 units.
        u32 gpu_registers;
        u32 gpu_registers_size;
        u32 lcd_registers;
        u32 lcd_registers_size;
        u32 pica_registers;
        u32 pica_registers_size;
        u32 default_attributes;
        u32 default_attributes_size;
        u32 vs_program_binary;
        u32 vs_program_binary_size;
        u32 vs_swizzle_data;
        u32 vs_swizzle_data_size;
        u32 vs_float_uniforms;
        u32 vs_float_uniforms_size;
        u32 gs_program_binary;
        u32 gs_program_binary_size;
        u32 gs_swizzle_data;
        u32 gs_swizzle_data_size;
        u32 gs_float_uniforms;
        u32 gs_float_uniforms_size;

        // Other things we might want to store here:
        // - Initial framebuffer data, maybe even a full copy of FCRAM/VRAM
        // - Lookup tables for fragment lighting
        // - Lookup tables for procedural textures
    } initial_state_offsets;

    u32 stream_offset;
    u32 stream_size;
};

enum CTStreamElementType : u32 {
    FrameMarker = 0xE1,
    MemoryLoad = 0xE2,
    RegisterWrite = 0xE3,
};

struct CTMemoryLoad {
    u32 file_offset;
    u32 size;
    u32 physical_address;
    u32 pad;
};

struct CTRegisterWrite {
    u32 physical_address;
    u32 value;
};

struct CTStreamElement {
    CTStreamElementType type;

    union {
        CTMemoryLoad memory_load;
        CTRegisterWrite register_write;
    };
};

#pragma pack()
} // namespace CiTrace
