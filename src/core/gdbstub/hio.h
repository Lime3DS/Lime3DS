// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Core {
class System;
}

namespace GDBStub {

/**
 * A request from a debugged application to perform some I/O with the GDB client.
 * This structure is also used to encode the reply back to the application.
 *
 * Based on the Rosalina + libctru implementations:
 * https://github.com/LumaTeam/Luma3DS/blob/master/sysmodules/rosalina/include/gdb.h#L46C27-L62
 * https://github.com/devkitPro/libctru/blob/master/libctru/source/gdbhio.c#L71-L87
 */
struct PackedGdbHioRequest {
    std::array<char, 4> magic; // "GDB\0"
    u32 version;

private:
    static inline constexpr std::size_t MAX_FUNCNAME_LEN = 16;
    static inline constexpr std::size_t PARAM_COUNT = 8;

public:
    // Request. Char arrays have +1 entry for null terminator
    std::array<char, MAX_FUNCNAME_LEN + 1> function_name;
    std::array<char, PARAM_COUNT + 1> param_format;

    std::array<u64, PARAM_COUNT> parameters;
    std::array<u32, PARAM_COUNT> string_lengths;

    // Return
    s64 retval;
    s32 gdb_errno;
    bool ctrl_c;
};

static_assert(sizeof(PackedGdbHioRequest) == 152,
              "HIO request size must match libctru implementation");

/**
 * Set the current HIO request to the given address. This is how the debugged
 * app indicates to the gdbstub that it wishes to perform a request.
 *
 *  @param address The memory address of the \ref PackedGdbHioRequest.
 */
void SetHioRequest(Core::System& system, const VAddr address);

/**
 * If there is a pending HIO request, send it to the client.
 *
 * @returns whethere any request was sent to the client.
 */
bool HandlePendingHioRequestPacket();

/**
 * Process an HIO reply from the client.
 */
void HandleHioReply(Core::System& system, const u8* const command_buffer, const u32 command_length);

} // namespace GDBStub
