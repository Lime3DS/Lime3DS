// Copyright 2016 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/thread.h"
#include "core/memory.h"

namespace IPC {

/// Size of the command buffer area, in 32-bit words.
constexpr std::size_t COMMAND_BUFFER_LENGTH = 0x100 / sizeof(u32);

// Maximum number of static buffers per thread.
constexpr std::size_t MAX_STATIC_BUFFERS = 16;

// These errors are commonly returned by invalid IPC translations, so alias them here for
// convenience.
// TODO(yuriks): These will probably go away once translation is implemented inside the kernel.
using Kernel::ResultInvalidBufferDescriptor;
constexpr auto ResultInvalidHandle = Kernel::ResultInvalidHandleOs;

enum DescriptorType : u32 {
    // Buffer related descriptors types (mask : 0x0F)
    StaticBuffer = 0x02,
    PXIBuffer = 0x04,
    MappedBuffer = 0x08,
    // Handle related descriptors types (mask : 0x30, but need to check for buffer related
    // descriptors first )
    CopyHandle = 0x00,
    MoveHandle = 0x10,
    CallingPid = 0x20,
};

union Header {
    u32 raw;
    BitField<0, 6, u32> translate_params_size;
    BitField<6, 6, u32> normal_params_size;
    BitField<16, 16, u32> command_id;
};

/**
 * @brief Creates a command header to be used for IPC
 * @param command_id            ID of the command to create a header for.
 * @param normal_params_size         Size of the normal parameters in words. Up to 63.
 * @param translate_params_size Size of the translate parameters in words. Up to 63.
 * @return The created IPC header.
 *
 * Normal parameters are sent directly to the process while the translate parameters might go
 * through modifications and checks by the kernel.
 * The translate parameters are described by headers generated with the IPC::*Desc functions.
 *
 * @note While @p normal_params_size is equivalent to the number of normal parameters,
 * @p translate_params_size includes the size occupied by the translate parameters headers.
 */
inline constexpr u32 MakeHeader(u16 command_id, u32 normal_params_size, u32 translate_params_size) {
    Header header{};
    header.command_id.Assign(command_id);
    header.normal_params_size.Assign(normal_params_size);
    header.translate_params_size.Assign(translate_params_size);
    return header.raw;
}

constexpr u32 MoveHandleDesc(u32 num_handles = 1) {
    return MoveHandle | ((num_handles - 1) << 26);
}

constexpr u32 CopyHandleDesc(u32 num_handles = 1) {
    return CopyHandle | ((num_handles - 1) << 26);
}

constexpr u32 CallingPidDesc() {
    return CallingPid;
}

constexpr bool IsHandleDescriptor(u32 descriptor) {
    return (descriptor & 0xF) == 0x0;
}

constexpr u32 HandleNumberFromDesc(u32 handle_descriptor) {
    return (handle_descriptor >> 26) + 1;
}

union StaticBufferDescInfo {
    u32 raw;
    BitField<0, 4, u32> descriptor_type;
    BitField<10, 4, u32> buffer_id;
    BitField<14, 18, u32> size;
};

inline u32 StaticBufferDesc(std::size_t size, u8 buffer_id) {
    StaticBufferDescInfo info{};
    info.descriptor_type.Assign(StaticBuffer);
    info.buffer_id.Assign(buffer_id);
    info.size.Assign(static_cast<u32>(size));
    return info.raw;
}

/**
 * @brief Creates a header describing a buffer to be sent over PXI.
 * @param size         Size of the buffer. Max 0x00FFFFFF.
 * @param buffer_id    The Id of the buffer. Max 0xF.
 * @param is_read_only true if the buffer is read-only. If false, the buffer is considered to have
 * read-write access.
 * @return The created PXI buffer header.
 *
 * The next value is a phys-address of a table located in the BASE memregion.
 */
inline u32 PXIBufferDesc(u32 size, unsigned buffer_id, bool is_read_only) {
    u32 type = PXIBuffer;
    if (is_read_only)
        type |= 0x2;
    return type | (size << 8) | ((buffer_id & 0xF) << 4);
}

enum MappedBufferPermissions : u32 {
    R = 1,
    W = 2,
    RW = R | W,
};

union MappedBufferDescInfo {
    u32 raw;
    BitField<0, 4, u32> flags;
    BitField<1, 2, MappedBufferPermissions> perms;
    BitField<4, 28, u32> size;
};

inline u32 MappedBufferDesc(std::size_t size, MappedBufferPermissions perms) {
    MappedBufferDescInfo info{};
    info.flags.Assign(MappedBuffer);
    info.perms.Assign(perms);
    info.size.Assign(static_cast<u32>(size));
    return info.raw;
}

inline DescriptorType GetDescriptorType(u32 descriptor) {
    // Note: Those checks must be done in this order
    if (IsHandleDescriptor(descriptor))
        return (DescriptorType)(descriptor & 0x30);

    // handle the fact that the following descriptors can have rights
    if (descriptor & MappedBuffer)
        return MappedBuffer;

    if (descriptor & PXIBuffer)
        return PXIBuffer;

    return StaticBuffer;
}

} // namespace IPC
