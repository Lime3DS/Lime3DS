// Copyright 2023 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/ranges.h>
#include "common/string_util.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/gdbstub/hio.h"

namespace GDBStub {

namespace {

static VAddr current_hio_request_addr;
static PackedGdbHioRequest current_hio_request;

enum class Status {
    NoRequest,
    NotSent,
    SentWaitingReply,
};

static std::atomic<Status> request_status{Status::NoRequest};

static std::atomic<bool> was_halted = false;
static std::atomic<bool> was_stepping = false;

} // namespace

/**
 * @return Whether the application has requested I/O, and it has not been sent.
 */
static bool HasPendingHioRequest() {
    return current_hio_request_addr != 0 && request_status == Status::NotSent;
}

/**
 * @return Whether the GDB stub is awaiting a reply from the client.
 */
static bool IsWaitingForHioReply() {
    return current_hio_request_addr != 0 && request_status == Status::SentWaitingReply;
}

/**
 * Send a response indicating an error back to the application.
 *
 * @param error_code The error code to respond back to the app. This typically corresponds to errno.
 *
 * @param retval The return value of the syscall the app requested.
 */
static void SendErrorReply(int error_code, int retval = -1) {
    auto packet = fmt::format("F{:x},{:x}", retval, error_code);
    SendReply(packet.data());
}

void SetHioRequest(Core::System& system, const VAddr addr) {
    if (!IsServerEnabled()) {
        LOG_WARNING(Debug_GDBStub, "HIO requested but GDB stub is not running");
        return;
    }

    if (IsWaitingForHioReply()) {
        LOG_WARNING(Debug_GDBStub, "HIO requested while already in progress");
        return;
    }

    if (HasPendingHioRequest()) {
        LOG_INFO(Debug_GDBStub, "overwriting existing HIO request that was not sent yet");
    }

    auto& memory = system.Memory();
    const auto process = system.Kernel().GetCurrentProcess();

    if (!memory.IsValidVirtualAddress(*process, addr)) {
        LOG_WARNING(Debug_GDBStub, "Invalid address for HIO request");
        return;
    }

    memory.ReadBlock(addr, &current_hio_request, sizeof(PackedGdbHioRequest));

    if (current_hio_request.magic != std::array{'G', 'D', 'B', '\0'}) {
        std::string_view bad_magic{
            current_hio_request.magic.data(),
            current_hio_request.magic.size(),
        };
        LOG_WARNING(Debug_GDBStub, "Invalid HIO request sent by application: bad magic '{}'",
                    bad_magic);

        current_hio_request_addr = 0;
        current_hio_request = {};
        request_status = Status::NoRequest;
        return;
    }

    LOG_DEBUG(Debug_GDBStub, "HIO request initiated at 0x{:X}", addr);
    current_hio_request_addr = addr;
    request_status = Status::NotSent;

    was_halted = GetCpuHaltFlag();
    was_stepping = GetCpuStepFlag();

    // Now halt, so that no further instructions are executed until the request
    // is processed by the client. We will continue after the reply comes back
    Break();
    SetCpuHaltFlag(true);
    SetCpuStepFlag(false);
    system.GetRunningCore().ClearInstructionCache();
}

void HandleHioReply(Core::System& system, const u8* const command_buffer,
                    const u32 command_length) {
    if (!IsWaitingForHioReply()) {
        LOG_WARNING(Debug_GDBStub, "Got HIO reply but never sent a request");
        return;
    }

    // Skip 'F' header
    const u8* command_pos = command_buffer + 1;

    if (*command_pos == 0 || *command_pos == ',') {
        LOG_WARNING(Debug_GDBStub, "bad HIO packet format position 0: {}", *command_pos);
        SendErrorReply(EILSEQ);
        return;
    }

    // Set the sign of the retval
    if (*command_pos == '-') {
        command_pos++;
        current_hio_request.retval = -1;
    } else {
        if (*command_pos == '+') {
            command_pos++;
        }

        current_hio_request.retval = 1;
    }

    const std::string command_str{command_pos, command_buffer + command_length};
    const auto command_parts = Common::SplitString(command_str, ',');

    if (command_parts.empty() || command_parts.size() > 3) {
        LOG_WARNING(Debug_GDBStub, "Unexpected reply packet size: {}", command_parts);
        SendErrorReply(EILSEQ);
        return;
    }

    u64 unsigned_retval =
        HexToInt(reinterpret_cast<const u8*>(command_parts[0].data()), command_parts[0].size());
    current_hio_request.retval *= unsigned_retval;

    if (command_parts.size() > 1) {
        // Technically the errno could be signed but in practice this doesn't happen
        current_hio_request.gdb_errno =
            HexToInt(reinterpret_cast<const u8*>(command_parts[1].data()), command_parts[1].size());
    } else {
        current_hio_request.gdb_errno = 0;
    }

    current_hio_request.ctrl_c = false;

    if (command_parts.size() > 2 && !command_parts[2].empty()) {
        if (command_parts[2][0] != 'C') {
            LOG_WARNING(Debug_GDBStub, "expected ctrl-c flag got '{}'", command_parts[2][0]);
            SendErrorReply(EILSEQ);
            return;
        }

        // for now we just ignore any trailing ";..." attachments
        current_hio_request.ctrl_c = true;
    }

    std::fill(std::begin(current_hio_request.param_format),
              std::end(current_hio_request.param_format), 0);

    LOG_TRACE(Debug_GDBStub, "HIO reply: {{retval = {}, errno = {}, ctrl_c = {}}}",
              current_hio_request.retval, current_hio_request.gdb_errno,
              current_hio_request.ctrl_c);

    const auto process = system.Kernel().GetCurrentProcess();
    auto& memory = system.Memory();

    // should have been checked when we first initialized the request,
    // but just double check again before we write to memory
    if (!memory.IsValidVirtualAddress(*process, current_hio_request_addr)) {
        LOG_WARNING(Debug_GDBStub, "Invalid address {:#X} to write HIO reply",
                    current_hio_request_addr);
        return;
    }

    memory.WriteBlock(current_hio_request_addr, &current_hio_request, sizeof(PackedGdbHioRequest));

    current_hio_request = {};
    current_hio_request_addr = 0;
    request_status = Status::NoRequest;

    // Restore state from before the request came in
    SetCpuStepFlag(was_stepping);
    SetCpuHaltFlag(was_halted);
    system.GetRunningCore().ClearInstructionCache();
}

bool HandlePendingHioRequestPacket() {
    if (!HasPendingHioRequest()) {
        return false;
    }

    if (IsWaitingForHioReply()) {
        // We already sent it, continue waiting for a reply
        return true;
    }

    // We need a null-terminated string from char* instead of using
    // the full length of the array (like {.begin(), .end()} constructor would)
    std::string_view function_name{current_hio_request.function_name.data()};

    std::string packet = fmt::format("F{}", function_name);

    u32 str_length_idx = 0;

    for (u32 i = 0; i < 8 && current_hio_request.param_format[i] != 0; i++) {
        u64 param = current_hio_request.parameters[i];

        // TODO: should we use the IntToGdbHex funcs instead of fmt::format_to ?
        switch (current_hio_request.param_format[i]) {
        case 'i':
        case 'I':
        case 'p':
            // For pointers and 32-bit ints, truncate down to size before sending
            param = static_cast<u32>(param);
            [[fallthrough]];

        case 'l':
        case 'L':
            fmt::format_to(std::back_inserter(packet), ",{:x}", param);
            break;

        case 's':
            // strings are written as {pointer}/{length}
            fmt::format_to(std::back_inserter(packet), ",{:x}/{:x}",
                           static_cast<u32>(current_hio_request.parameters[i]),
                           current_hio_request.string_lengths[str_length_idx++]);
            break;

        default:
            LOG_WARNING(Debug_GDBStub, "unexpected hio request param format '{}'",
                        current_hio_request.param_format[i]);
            SendErrorReply(EILSEQ);
            return false;
        }
    }

    LOG_TRACE(Debug_GDBStub, "HIO request packet: '{}'", packet);

    SendReply(packet.data());
    request_status = Status::SentWaitingReply;
    return true;
}

} // namespace GDBStub
