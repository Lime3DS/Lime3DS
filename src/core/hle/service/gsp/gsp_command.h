// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"

namespace Service::GSP {

/// GSP command ID
enum class CommandId : u32 {
    RequestDma = 0x00,
    SubmitCmdList = 0x01,
    MemoryFill = 0x02,
    DisplayTransfer = 0x03,
    TextureCopy = 0x04,
    CacheFlush = 0x05,
};

struct DmaCommand {
    u32 source_address;
    u32 dest_address;
    u32 size;
};

struct SubmitCmdListCommand {
    u32 address;
    u32 size;
    u32 flags;
    u32 unused[3];
    u32 do_flush;
};

struct MemoryFillCommand {
    u32 start1;
    u32 value1;
    u32 end1;

    u32 start2;
    u32 value2;
    u32 end2;

    u16 control1;
    u16 control2;
};

struct DisplayTransferCommand {
    u32 in_buffer_address;
    u32 out_buffer_address;
    u32 in_buffer_size;
    u32 out_buffer_size;
    u32 flags;
};

struct TextureCopyCommand {
    u32 in_buffer_address;
    u32 out_buffer_address;
    u32 size;
    u32 in_width_gap;
    u32 out_width_gap;
    u32 flags;
};

struct CacheFlushCommand {
    struct {
        u32 address;
        u32 size;
    } regions[3];
};

/// GSP command
struct Command {
    BitField<0, 8, CommandId> id;
    union {
        DmaCommand dma_request;
        SubmitCmdListCommand submit_gpu_cmdlist;
        MemoryFillCommand memory_fill;
        DisplayTransferCommand display_transfer;
        TextureCopyCommand texture_copy;
        CacheFlushCommand cache_flush;
        std::array<u8, 0x1C> raw_data;
    };
};
static_assert(sizeof(Command) == 0x20, "Command struct has incorrect size");

/// GSP shared memory GX command buffer header
struct CommandBuffer {
    union {
        u32 hex;

        // Current command index. This index is updated by GSP module after loading the command
        // data, right before the command is processed. When this index is updated by GSP module,
        // the total commands field is decreased by one as well.
        BitField<0, 8, u32> index;

        // Total commands to process, must not be value 0 when GSP module handles commands. This
        // must be <=15 when writing a command to shared memory. This is incremented by the
        // application when writing a command to shared memory, after increasing this value
        // TriggerCmdReqQueue is only used if this field is value 1.
        BitField<8, 8, u32> number_commands;
    };

    u32 unk[7];

    Command commands[0xF];
};
static_assert(sizeof(CommandBuffer) == 0x200, "CommandBuffer struct has incorrect size");

} // namespace Service::GSP
