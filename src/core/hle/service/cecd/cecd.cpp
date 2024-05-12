// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/unique_ptr.hpp>
#include <cryptopp/base64.h>
#include <cryptopp/hmac.h>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/process.h"
#include "core/hle/result.h"
#include "core/hle/service/cecd/cecd.h"
#include "core/hle/service/cecd/cecd_ndm.h"
#include "core/hle/service/cecd/cecd_s.h"
#include "core/hle/service/cecd/cecd_u.h"
#include "core/hle/service/cfg/cfg.h"

SERVICE_CONSTRUCT_IMPL(Service::CECD::Module)
SERIALIZE_EXPORT_IMPL(Service::CECD::Module)
SERIALIZE_EXPORT_IMPL(Service::CECD::Module::SessionData)

namespace Service::CECD {

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int) {
    ar & cecd_system_save_data_archive;
    ar & cecinfo_event;
    ar & cecinfosys_event;
    ar & change_state_event;
}
SERIALIZE_IMPL(Module)

using CecDataPathType = Module::CecDataPathType;
using CecOpenMode = Module::CecOpenMode;
using CecSystemInfoType = Module::CecSystemInfoType;

void Module::Interface::Open(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ncch_program_id = rp.Pop<u32>();
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    CecOpenMode open_mode;
    open_mode.raw = rp.Pop<u32>();
    rp.PopPID();

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode mode;
    mode.read_flag.Assign(1);
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    SessionData* session_data = GetSessionData(ctx.Session());
    session_data->ncch_program_id = ncch_program_id;
    session_data->open_mode.raw = open_mode.raw;
    session_data->data_path_type = path_type;
    session_data->path = path;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    switch (path_type) {
    case CecDataPathType::RootDir:
    case CecDataPathType::MboxDir:
    case CecDataPathType::InboxDir:
    case CecDataPathType::OutboxDir: {
        auto dir_result = cecd->cecd_system_save_data_archive->OpenDirectory(path);
        if (dir_result.Failed()) {
            if (open_mode.create) {
                cecd->cecd_system_save_data_archive->CreateDirectory(path);
                rb.Push(ResultSuccess);
            } else {
                LOG_DEBUG(Service_CECD, "Failed to open directory: {}", path.AsString());
                rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                               ErrorLevel::Status));
            }
            rb.Push<u32>(0); // Zero entries
        } else {
            constexpr u32 max_entries = 32; // reasonable value, just over max boxes 24
            auto directory = std::move(dir_result).Unwrap();

            // Actual reading into vector seems to be required for entry count
            std::vector<FileSys::Entry> entries(max_entries);
            const u32 entry_count = directory->Read(max_entries, entries.data());

            LOG_DEBUG(Service_CECD, "Number of entries found: {}", entry_count);

            rb.Push(ResultSuccess);
            rb.Push<u32>(entry_count); // Entry count
            directory->Close();
        }
        break;
    }
    default: { // If not directory, then it is a file
        auto file_result = cecd->cecd_system_save_data_archive->OpenFile(path, mode);
        if (file_result.Failed()) {
            LOG_DEBUG(Service_CECD, "Failed to open file: {}", path.AsString());
            rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                           ErrorLevel::Status));
            rb.Push<u32>(0); // No file size
        } else {
            session_data->file = std::move(file_result).Unwrap();
            rb.Push(ResultSuccess);
            rb.Push<u32>(static_cast<u32>(session_data->file->GetSize())); // Return file size
        }

        if (path_type == CecDataPathType::MboxProgramId) {
            std::vector<u8> program_id(8);
            u64_le le_program_id = cecd->system.Kernel().GetCurrentProcess()->codeset->program_id;
            std::memcpy(program_id.data(), &le_program_id, sizeof(u64));
            session_data->file->Write(0, sizeof(u64), true, false, program_id.data());
            session_data->file->Close();
        }
    }
    }

    LOG_DEBUG(Service_CECD,
              "called, ncch_program_id={:#010x}, path_type={:#04x}, path={}, "
              "open_mode: raw={:#x}, unknown={}, read={}, write={}, create={}, check={}",
              ncch_program_id, path_type, path.AsString(), open_mode.raw, open_mode.unknown.Value(),
              open_mode.read.Value(), open_mode.write.Value(), open_mode.create.Value(),
              open_mode.check.Value());
}

void Module::Interface::Read(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 write_buffer_size = rp.Pop<u32>();
    auto& write_buffer = rp.PopMappedBuffer();

    SessionData* session_data = GetSessionData(ctx.Session());
    LOG_DEBUG(Service_CECD,
              "SessionData: ncch_program_id={:#010x}, data_path_type={:#04x}, "
              "path={}, open_mode: raw={:#x}, unknown={}, read={}, write={}, create={}, check={}",
              session_data->ncch_program_id, session_data->data_path_type,
              session_data->path.AsString(), session_data->open_mode.raw,
              session_data->open_mode.unknown.Value(), session_data->open_mode.read.Value(),
              session_data->open_mode.write.Value(), session_data->open_mode.create.Value(),
              session_data->open_mode.check.Value());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    switch (session_data->data_path_type) {
    case CecDataPathType::RootDir:
    case CecDataPathType::MboxDir:
    case CecDataPathType::InboxDir:
    case CecDataPathType::OutboxDir:
        rb.Push(Result(ErrorDescription::NotAuthorized, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
        rb.Push<u32>(0); // No bytes read
        break;
    default: // If not directory, then it is a file
        std::vector<u8> buffer(write_buffer_size);
        const u32 bytes_read = static_cast<u32>(
            session_data->file->Read(0, write_buffer_size, buffer.data()).Unwrap());

        write_buffer.Write(buffer.data(), 0, write_buffer_size);
        session_data->file->Close();

        rb.Push(ResultSuccess);
        rb.Push<u32>(bytes_read);
    }
    rb.PushMappedBuffer(write_buffer);

    LOG_DEBUG(Service_CECD, "called, write_buffer_size={:#x}, path={}", write_buffer_size,
              session_data->path.AsString());
}

void Module::Interface::ReadMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& message_id_buffer = rp.PopMappedBuffer();
    auto& write_buffer = rp.PopMappedBuffer();

    FileSys::Mode mode;
    mode.read_flag.Assign(1);

    std::vector<u8> id_buffer(message_id_size);
    message_id_buffer.Read(id_buffer.data(), 0, message_id_size);

    FileSys::Path message_path =
        cecd->GetCecDataPathTypeAsString(is_outbox ? CecDataPathType::OutboxMsg
                                                   : CecDataPathType::InboxMsg,
                                         ncch_program_id, id_buffer)
            .data();

    auto message_result = cecd->cecd_system_save_data_archive->OpenFile(message_path, mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 4);
    if (message_result.Succeeded()) {
        auto message = std::move(message_result).Unwrap();
        std::vector<u8> buffer(buffer_size);

        const u32 bytes_read =
            static_cast<u32>(message->Read(0, buffer_size, buffer.data()).Unwrap());
        write_buffer.Write(buffer.data(), 0, buffer_size);
        message->Close();

        CecMessageHeader msg_header;
        std::memcpy(&msg_header, buffer.data(), sizeof(CecMessageHeader));

        LOG_DEBUG(Service_CECD,
                  "magic={:#06x}, message_size={:#010x}, header_size={:#010x}, "
                  "body_size={:#010x}, title_id={:#010x}, title_id_2={:#010x}, "
                  "batch_id={:#010x}",
                  msg_header.magic, msg_header.message_size, msg_header.header_size,
                  msg_header.body_size, msg_header.title_id, msg_header.title_id2,
                  msg_header.batch_id);
        LOG_DEBUG(Service_CECD,
                  "unknown_id={:#010x}, version={:#010x}, flag={:#04x}, "
                  "send_method={:#04x}, is_unopen={:#04x}, is_new={:#04x}, "
                  "sender_id={:#018x}, sender_id2={:#018x}, send_count={:#04x}, "
                  "forward_count={:#04x}, user_data={:#06x}, ",
                  msg_header.unknown_id, msg_header.version, msg_header.flag,
                  msg_header.send_method, msg_header.is_unopen, msg_header.is_new,
                  msg_header.sender_id, msg_header.sender_id2, msg_header.send_count,
                  msg_header.forward_count, msg_header.user_data);

        rb.Push(ResultSuccess);
        rb.Push<u32>(bytes_read);
    } else {
        rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
        rb.Push<u32>(0); // zero bytes read
    }
    rb.PushMappedBuffer(message_id_buffer);
    rb.PushMappedBuffer(write_buffer);

    LOG_DEBUG(
        Service_CECD,
        "called, ncch_program_id={:#010x}, is_outbox={}, message_id_size={:#x}, buffer_size={:#x}",
        ncch_program_id, is_outbox, message_id_size, buffer_size);
}

void Module::Interface::ReadMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& message_id_buffer = rp.PopMappedBuffer();
    auto& hmac_key_buffer = rp.PopMappedBuffer();
    auto& write_buffer = rp.PopMappedBuffer();

    FileSys::Mode mode;
    mode.read_flag.Assign(1);

    std::vector<u8> id_buffer(message_id_size);
    message_id_buffer.Read(id_buffer.data(), 0, message_id_size);

    FileSys::Path message_path =
        cecd->GetCecDataPathTypeAsString(is_outbox ? CecDataPathType::OutboxMsg
                                                   : CecDataPathType::InboxMsg,
                                         ncch_program_id, id_buffer)
            .data();

    auto message_result = cecd->cecd_system_save_data_archive->OpenFile(message_path, mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 6);
    if (message_result.Succeeded()) {
        auto message = std::move(message_result).Unwrap();
        std::vector<u8> buffer(buffer_size);

        const u32 bytes_read =
            static_cast<u32>(message->Read(0, buffer_size, buffer.data()).Unwrap());
        write_buffer.Write(buffer.data(), 0, buffer_size);
        message->Close();

        CecMessageHeader msg_header;
        std::memcpy(&msg_header, buffer.data(), sizeof(CecMessageHeader));

        LOG_DEBUG(Service_CECD,
                  "magic={:#06x}, message_size={:#010x}, header_size={:#010x}, "
                  "body_size={:#010x}, title_id={:#010x}, title_id_2={:#010x}, "
                  "batch_id={:#010x}",
                  msg_header.magic, msg_header.message_size, msg_header.header_size,
                  msg_header.body_size, msg_header.title_id, msg_header.title_id2,
                  msg_header.batch_id);
        LOG_DEBUG(Service_CECD,
                  "unknown_id={:#010x}, version={:#010x}, flag={:#04x}, "
                  "send_method={:#04x}, is_unopen={:#04x}, is_new={:#04x}, "
                  "sender_id={:#018x}, sender_id2={:#018x}, send_count={:#04x}, "
                  "forward_count={:#04x}, user_data={:#06x}, ",
                  msg_header.unknown_id, msg_header.version, msg_header.flag,
                  msg_header.send_method, msg_header.is_unopen, msg_header.is_new,
                  msg_header.sender_id, msg_header.sender_id2, msg_header.send_count,
                  msg_header.forward_count, msg_header.user_data);

        std::vector<u8> hmac_digest(0x20);
        std::memcpy(hmac_digest.data(),
                    buffer.data() + msg_header.header_size + msg_header.body_size, 0x20);

        std::vector<u8> message_body(msg_header.body_size);
        std::memcpy(message_body.data(), buffer.data() + msg_header.header_size,
                    msg_header.body_size);

        using namespace CryptoPP;
        SecByteBlock key(0x20);
        hmac_key_buffer.Read(key.data(), 0, key.size());

        HMAC<SHA256> hmac(key, key.size());

        const bool verify_hmac =
            hmac.VerifyDigest(hmac_digest.data(), message_body.data(), message_body.size());

        if (verify_hmac)
            LOG_DEBUG(Service_CECD, "Verification succeeded");
        else
            LOG_DEBUG(Service_CECD, "Verification failed");

        rb.Push(ResultSuccess);
        rb.Push<u32>(bytes_read);
    } else {
        rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
        rb.Push<u32>(0); // zero bytes read
    }

    rb.PushMappedBuffer(message_id_buffer);
    rb.PushMappedBuffer(hmac_key_buffer);
    rb.PushMappedBuffer(write_buffer);

    LOG_DEBUG(
        Service_CECD,
        "called, ncch_program_id={:#010x}, is_outbox={}, message_id_size={:#x}, buffer_size={:#x}",
        ncch_program_id, is_outbox, message_id_size, buffer_size);
}

void Module::Interface::Write(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 read_buffer_size = rp.Pop<u32>();
    auto& read_buffer = rp.PopMappedBuffer();

    SessionData* session_data = GetSessionData(ctx.Session());
    LOG_DEBUG(Service_CECD,
              "SessionData: ncch_program_id={:#010x}, data_path_type={:#04x}, "
              "path={}, open_mode: raw={:#x}, unknown={}, read={}, write={}, create={}, check={}",
              session_data->ncch_program_id, session_data->data_path_type,
              session_data->path.AsString(), session_data->open_mode.raw,
              session_data->open_mode.unknown.Value(), session_data->open_mode.read.Value(),
              session_data->open_mode.write.Value(), session_data->open_mode.create.Value(),
              session_data->open_mode.check.Value());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    switch (session_data->data_path_type) {
    case CecDataPathType::RootDir:
    case CecDataPathType::MboxDir:
    case CecDataPathType::InboxDir:
    case CecDataPathType::OutboxDir:
        rb.Push(Result(ErrorDescription::NotAuthorized, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
        break;
    default: // If not directory, then it is a file
        std::vector<u8> buffer(read_buffer_size);
        read_buffer.Read(buffer.data(), 0, read_buffer_size);

        if (session_data->file->GetSize() != read_buffer_size) {
            session_data->file->SetSize(read_buffer_size);
        }

        if (session_data->open_mode.check) {
            cecd->CheckAndUpdateFile(session_data->data_path_type, session_data->ncch_program_id,
                                     buffer);
        }

        [[maybe_unused]] const u32 bytes_written = static_cast<u32>(
            session_data->file->Write(0, buffer.size(), true, false, buffer.data()).Unwrap());
        session_data->file->Close();

        rb.Push(ResultSuccess);
    }
    rb.PushMappedBuffer(read_buffer);

    LOG_DEBUG(Service_CECD, "called, read_buffer_size={:#x}", read_buffer_size);
}

void Module::Interface::WriteMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& read_buffer = rp.PopMappedBuffer();
    auto& message_id_buffer = rp.PopMappedBuffer();

    FileSys::Mode mode;
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    std::vector<u8> id_buffer(message_id_size);
    message_id_buffer.Read(id_buffer.data(), 0, message_id_size);

    FileSys::Path message_path =
        cecd->GetCecDataPathTypeAsString(is_outbox ? CecDataPathType::OutboxMsg
                                                   : CecDataPathType::InboxMsg,
                                         ncch_program_id, id_buffer)
            .data();

    auto message_result = cecd->cecd_system_save_data_archive->OpenFile(message_path, mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    if (message_result.Succeeded()) {
        auto message = std::move(message_result).Unwrap();

        std::vector<u8> buffer(buffer_size);
        read_buffer.Read(buffer.data(), 0, buffer_size);

        CecMessageHeader msg_header;
        std::memcpy(&msg_header, buffer.data(), sizeof(CecMessageHeader));

        LOG_DEBUG(Service_CECD,
                  "magic={:#06x}, message_size={:#010x}, header_size={:#010x}, "
                  "body_size={:#010x}, title_id={:#010x}, title_id_2={:#010x}, "
                  "batch_id={:#010x}",
                  msg_header.magic, msg_header.message_size, msg_header.header_size,
                  msg_header.body_size, msg_header.title_id, msg_header.title_id2,
                  msg_header.batch_id);
        LOG_DEBUG(Service_CECD,
                  "unknown_id={:#010x}, version={:#010x}, flag={:#04x}, "
                  "send_method={:#04x}, is_unopen={:#04x}, is_new={:#04x}, "
                  "sender_id={:#018x}, sender_id2={:#018x}, send_count={:#04x}, "
                  "forward_count={:#04x}, user_data={:#06x}, ",
                  msg_header.unknown_id, msg_header.version, msg_header.flag,
                  msg_header.send_method, msg_header.is_unopen, msg_header.is_new,
                  msg_header.sender_id, msg_header.sender_id2, msg_header.send_count,
                  msg_header.forward_count, msg_header.user_data);

        [[maybe_unused]] const u32 bytes_written =
            static_cast<u32>(message->Write(0, buffer_size, true, false, buffer.data()).Unwrap());
        message->Close();

        rb.Push(ResultSuccess);
    } else {
        rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
    }

    rb.PushMappedBuffer(read_buffer);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_DEBUG(
        Service_CECD,
        "called, ncch_program_id={:#010x}, is_outbox={}, message_id_size={:#x}, buffer_size={:#x}",
        ncch_program_id, is_outbox, message_id_size, buffer_size);
}

void Module::Interface::WriteMessageWithHMAC(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ncch_program_id = rp.Pop<u32>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& read_buffer = rp.PopMappedBuffer();
    auto& hmac_key_buffer = rp.PopMappedBuffer();
    auto& message_id_buffer = rp.PopMappedBuffer();

    FileSys::Mode mode;
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    std::vector<u8> id_buffer(message_id_size);
    message_id_buffer.Read(id_buffer.data(), 0, message_id_size);

    FileSys::Path message_path =
        cecd->GetCecDataPathTypeAsString(is_outbox ? CecDataPathType::OutboxMsg
                                                   : CecDataPathType::InboxMsg,
                                         ncch_program_id, id_buffer)
            .data();

    auto message_result = cecd->cecd_system_save_data_archive->OpenFile(message_path, mode);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 6);
    if (message_result.Succeeded()) {
        auto message = std::move(message_result).Unwrap();

        std::vector<u8> buffer(buffer_size);
        read_buffer.Read(buffer.data(), 0, buffer_size);

        CecMessageHeader msg_header;
        std::memcpy(&msg_header, buffer.data(), sizeof(CecMessageHeader));

        LOG_DEBUG(Service_CECD,
                  "magic={:#06x}, message_size={:#010x}, header_size={:#010x}, "
                  "body_size={:#010x}, title_id={:#010x}, title_id_2={:#010x}, "
                  "batch_id={:#010x}",
                  msg_header.magic, msg_header.message_size, msg_header.header_size,
                  msg_header.body_size, msg_header.title_id, msg_header.title_id2,
                  msg_header.batch_id);
        LOG_DEBUG(Service_CECD,
                  "unknown_id={:#010x}, version={:#010x}, flag={:#04x}, "
                  "send_method={:#04x}, is_unopen={:#04x}, is_new={:#04x}, "
                  "sender_id={:#018x}, sender_id2={:#018x}, send_count={:#04x}, "
                  "forward_count={:#04x}, user_data={:#06x}, ",
                  msg_header.unknown_id, msg_header.version, msg_header.flag,
                  msg_header.send_method, msg_header.is_unopen, msg_header.is_new,
                  msg_header.sender_id, msg_header.sender_id2, msg_header.send_count,
                  msg_header.forward_count, msg_header.user_data);

        const u32 hmac_offset = msg_header.header_size + msg_header.body_size;
        const u32 hmac_size = 0x20;

        std::vector<u8> hmac_digest(hmac_size);
        std::vector<u8> message_body(msg_header.body_size);
        std::memcpy(message_body.data(), buffer.data() + msg_header.header_size,
                    msg_header.body_size);

        using namespace CryptoPP;
        SecByteBlock key(hmac_size);
        hmac_key_buffer.Read(key.data(), 0, hmac_size);

        HMAC<SHA256> hmac(key, hmac_size);
        hmac.CalculateDigest(hmac_digest.data(), message_body.data(), msg_header.body_size);
        std::memcpy(buffer.data() + hmac_offset, hmac_digest.data(), hmac_size);

        [[maybe_unused]] const u32 bytes_written =
            static_cast<u32>(message->Write(0, buffer_size, true, false, buffer.data()).Unwrap());
        message->Close();

        rb.Push(ResultSuccess);
    } else {
        rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
    }

    rb.PushMappedBuffer(read_buffer);
    rb.PushMappedBuffer(hmac_key_buffer);
    rb.PushMappedBuffer(message_id_buffer);

    LOG_DEBUG(
        Service_CECD,
        "called, ncch_program_id={:#010x}, is_outbox={}, message_id_size={:#x}, buffer_size={:#x}",
        ncch_program_id, is_outbox, message_id_size, buffer_size);
}

void Module::Interface::Delete(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ncch_program_id = rp.Pop<u32>();
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    const bool is_outbox = rp.Pop<bool>();
    const u32 message_id_size = rp.Pop<u32>();
    auto& message_id_buffer = rp.PopMappedBuffer();

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode mode;
    mode.write_flag.Assign(1);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    switch (path_type) {
    case CecDataPathType::RootDir:
    case CecDataPathType::MboxDir:
    case CecDataPathType::InboxDir:
    case CecDataPathType::OutboxDir:
        rb.Push(cecd->cecd_system_save_data_archive->DeleteDirectoryRecursively(path));
        break;
    default: // If not directory, then it is a file
        if (message_id_size == 0) {
            rb.Push(cecd->cecd_system_save_data_archive->DeleteFile(path));
        } else {
            std::vector<u8> id_buffer(message_id_size);
            message_id_buffer.Read(id_buffer.data(), 0, message_id_size);

            FileSys::Path message_path =
                cecd->GetCecDataPathTypeAsString(is_outbox ? CecDataPathType::OutboxMsg
                                                           : CecDataPathType::InboxMsg,
                                                 ncch_program_id, id_buffer)
                    .data();
            rb.Push(cecd->cecd_system_save_data_archive->DeleteFile(message_path));
        }
    }

    rb.PushMappedBuffer(message_id_buffer);

    LOG_DEBUG(Service_CECD,
              "called, ncch_program_id={:#010x}, path_type={:#04x}, path={}, "
              "is_outbox={}, message_id_size={:#x}",
              ncch_program_id, path_type, path.AsString(), is_outbox, message_id_size);
}

void Module::Interface::SetData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 ncch_program_id = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    const u32 option = rp.Pop<u32>();
    auto& read_buffer = rp.PopMappedBuffer();

    if (option == 2 && buffer_size > 0) { // update obindex?
        FileSys::Path path(
            cecd->GetCecDataPathTypeAsString(CecDataPathType::OutboxIndex, ncch_program_id).data());
        FileSys::Mode mode;
        mode.write_flag.Assign(1);
        mode.create_flag.Assign(1);

        auto file_result = cecd->cecd_system_save_data_archive->OpenFile(path, mode);
        if (file_result.Succeeded()) {
            auto file = std::move(file_result).Unwrap();
            std::vector<u8> buffer(buffer_size);
            read_buffer.Read(buffer.data(), 0, buffer_size);

            cecd->CheckAndUpdateFile(CecDataPathType::OutboxIndex, ncch_program_id, buffer);

            file->Write(0, buffer.size(), true, false, buffer.data());
            file->Close();
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(read_buffer);

    LOG_DEBUG(Service_CECD, "called, ncch_program_id={:#010x}, buffer_size={:#x}, option={:#x}",
              ncch_program_id, buffer_size, option);
}

void Module::Interface::ReadData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto dest_buffer_size = rp.Pop<u32>();
    const auto info_type = rp.PopEnum<CecSystemInfoType>();
    const auto param_buffer_size = rp.Pop<u32>();
    auto& param_buffer = rp.PopMappedBuffer();
    auto& dest_buffer = rp.PopMappedBuffer();

    // TODO: Other CecSystemInfoTypes
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    std::vector<u8> buffer;
    switch (info_type) {
    case CecSystemInfoType::EulaVersion: {
        const auto cfg = Service::CFG::GetModule(cecd->system);
        const auto version = cfg->GetEULAVersion();
        buffer = {version.minor, version.major};
        break;
    }
    case CecSystemInfoType::Eula:
        buffer = {true}; // Eula agreed
        break;
    case CecSystemInfoType::ParentControl:
        buffer = {false}; // No parent control
        break;
    default:
        LOG_ERROR(Service_CECD, "Unknown system info type={:#x}", info_type);
        buffer = {};
    }
    dest_buffer.Write(buffer.data(), 0,
                      std::min(static_cast<std::size_t>(dest_buffer_size), buffer.size()));

    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(param_buffer);
    rb.PushMappedBuffer(dest_buffer);

    LOG_DEBUG(Service_CECD,
              "called, dest_buffer_size={:#x}, info_type={:#x}, param_buffer_size={:#x}",
              dest_buffer_size, info_type, param_buffer_size);
}

void Module::Interface::Start(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const CecCommand command = rp.PopEnum<CecCommand>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_CECD, "(STUBBED) called, command={}", cecd->GetCecCommandAsString(command));
}

void Module::Interface::Stop(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const CecCommand command = rp.PopEnum<CecCommand>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_CECD, "(STUBBED) called, command={}", cecd->GetCecCommandAsString(command));
}

void Module::Interface::GetCecInfoBuffer(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 possible_info_type = rp.Pop<u32>();
    const u32 buffer_size = rp.Pop<u32>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(buffer);

    LOG_DEBUG(Service_CECD, "called, possible_info_type={}, buffer_size={}", possible_info_type,
              buffer_size);
}

void Module::Interface::GetCecdState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum(CecdState::NdmStatusIdle);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::GetCecInfoEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(cecd->cecinfo_event);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::GetChangeStateEventHandle(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(cecd->change_state_event);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

void Module::Interface::OpenAndWrite(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 buffer_size = rp.Pop<u32>();
    const u32 ncch_program_id = rp.Pop<u32>();
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    CecOpenMode open_mode;
    open_mode.raw = rp.Pop<u32>();
    rp.PopPID();
    auto& read_buffer = rp.PopMappedBuffer();

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode mode;
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    switch (path_type) {
    case CecDataPathType::RootDir:
    case CecDataPathType::MboxDir:
    case CecDataPathType::InboxDir:
    case CecDataPathType::OutboxDir:
        rb.Push(Result(ErrorDescription::NotAuthorized, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
        break;
    default: // If not directory, then it is a file
        auto file_result = cecd->cecd_system_save_data_archive->OpenFile(path, mode);
        if (file_result.Succeeded()) {
            auto file = std::move(file_result).Unwrap();

            std::vector<u8> buffer(buffer_size);
            read_buffer.Read(buffer.data(), 0, buffer_size);

            if (file->GetSize() != buffer_size) {
                file->SetSize(buffer_size);
            }

            if (open_mode.check) {
                cecd->CheckAndUpdateFile(path_type, ncch_program_id, buffer);
            }

            [[maybe_unused]] const u32 bytes_written = static_cast<u32>(
                file->Write(0, buffer.size(), true, false, buffer.data()).Unwrap());
            file->Close();

            rb.Push(ResultSuccess);
        } else {
            rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                           ErrorLevel::Status));
        }
    }
    rb.PushMappedBuffer(read_buffer);

    LOG_DEBUG(Service_CECD,
              "called, ncch_program_id={:#010x}, path_type={:#04x}, path={}, buffer_size={:#x} "
              "open_mode: raw={:#x}, unknown={}, read={}, write={}, create={}, check={}",
              ncch_program_id, path_type, path.AsString(), buffer_size, open_mode.raw,
              open_mode.unknown.Value(), open_mode.read.Value(), open_mode.write.Value(),
              open_mode.create.Value(), open_mode.check.Value());
}

void Module::Interface::OpenAndRead(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 buffer_size = rp.Pop<u32>();
    const u32 ncch_program_id = rp.Pop<u32>();
    const CecDataPathType path_type = rp.PopEnum<CecDataPathType>();
    CecOpenMode open_mode;
    open_mode.raw = rp.Pop<u32>();
    rp.PopPID();
    auto& write_buffer = rp.PopMappedBuffer();

    FileSys::Path path(cecd->GetCecDataPathTypeAsString(path_type, ncch_program_id).data());
    FileSys::Mode mode;
    mode.read_flag.Assign(1);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    switch (path_type) {
    case CecDataPathType::RootDir:
    case CecDataPathType::MboxDir:
    case CecDataPathType::InboxDir:
    case CecDataPathType::OutboxDir:
        rb.Push(Result(ErrorDescription::NotAuthorized, ErrorModule::CEC, ErrorSummary::NotFound,
                       ErrorLevel::Status));
        rb.Push<u32>(0); // No entries read
        break;
    default: // If not directory, then it is a file
        auto file_result = cecd->cecd_system_save_data_archive->OpenFile(path, mode);
        if (file_result.Succeeded()) {
            auto file = std::move(file_result).Unwrap();
            std::vector<u8> buffer(buffer_size);

            const u32 bytes_read =
                static_cast<u32>(file->Read(0, buffer_size, buffer.data()).Unwrap());
            write_buffer.Write(buffer.data(), 0, buffer_size);
            file->Close();

            rb.Push(ResultSuccess);
            rb.Push<u32>(bytes_read);
        } else {
            rb.Push(Result(ErrorDescription::NoData, ErrorModule::CEC, ErrorSummary::NotFound,
                           ErrorLevel::Status));
            rb.Push<u32>(0); // No bytes read
        }
    }
    rb.PushMappedBuffer(write_buffer);

    LOG_DEBUG(Service_CECD,
              "called, ncch_program_id={:#010x}, path_type={:#04x}, path={}, buffer_size={:#x} "
              "open_mode: raw={:#x}, unknown={}, read={}, write={}, create={}, check={}",
              ncch_program_id, path_type, path.AsString(), buffer_size, open_mode.raw,
              open_mode.unknown.Value(), open_mode.read.Value(), open_mode.write.Value(),
              open_mode.create.Value(), open_mode.check.Value());
}

void Module::Interface::GetCecInfoEventHandleSys(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(cecd->cecinfosys_event);

    LOG_WARNING(Service_CECD, "(STUBBED) called");
}

std::string Module::EncodeBase64(std::span<const u8> in) const {
    using namespace CryptoPP;
    using Name::EncodingLookupArray;
    using Name::InsertLineBreaks;
    using Name::Pad;

    std::string out;
    Base64Encoder encoder;
    AlgorithmParameters params =
        MakeParameters(EncodingLookupArray(), (const byte*)base64_dict.data())(InsertLineBreaks(),
                                                                               false)(Pad(), false);

    encoder.IsolatedInitialize(params);
    encoder.Attach(new StringSink(out));
    encoder.Put(in.data(), in.size());
    encoder.MessageEnd();

    return out;
}

std::string Module::GetCecDataPathTypeAsString(const CecDataPathType type, const u32 program_id,
                                               std::span<const u8> msg_id) const {
    switch (type) {
    case CecDataPathType::MboxList:
        return "/CEC/MBoxList____";
    case CecDataPathType::MboxInfo:
        return fmt::format("/CEC/{:08x}/MBoxInfo____", program_id);
    case CecDataPathType::InboxInfo:
        return fmt::format("/CEC/{:08x}/InBox___/BoxInfo_____", program_id);
    case CecDataPathType::OutboxInfo:
        return fmt::format("/CEC/{:08x}/OutBox__/BoxInfo_____", program_id);
    case CecDataPathType::OutboxIndex:
        return fmt::format("/CEC/{:08x}/OutBox__/OBIndex_____", program_id);
    case CecDataPathType::InboxMsg:
        return fmt::format("/CEC/{:08x}/InBox___/_{}", program_id, EncodeBase64(msg_id));
    case CecDataPathType::OutboxMsg:
        return fmt::format("/CEC/{:08x}/OutBox__/_{}", program_id, EncodeBase64(msg_id));
    case CecDataPathType::RootDir:
        return "/CEC";
    case CecDataPathType::MboxDir:
        return fmt::format("/CEC/{:08x}", program_id);
    case CecDataPathType::InboxDir:
        return fmt::format("/CEC/{:08x}/InBox___", program_id);
    case CecDataPathType::OutboxDir:
        return fmt::format("/CEC/{:08x}/OutBox__", program_id);
    case CecDataPathType::MboxData:
    case CecDataPathType::MboxIcon:
    case CecDataPathType::MboxTitle:
    default:
        return fmt::format("/CEC/{:08x}/MBoxData.{:03}", program_id, static_cast<u32>(type) - 100);
    }
}

std::string Module::GetCecCommandAsString(const CecCommand command) const {
    switch (command) {
    case CecCommand::None:
        return "None";
    case CecCommand::Start:
        return "Start";
    case CecCommand::ResetStart:
        return "ResetStart";
    case CecCommand::ReadyScan:
        return "ReadyScan";
    case CecCommand::ReadyScanWait:
        return "ReadyScanWait";
    case CecCommand::StartScan:
        return "StartScan";
    case CecCommand::Rescan:
        return "Rescan";
    case CecCommand::NdmResume:
        return "NdmResume";
    case CecCommand::NdmSuspend:
        return "NdmSuspend";
    case CecCommand::NdmSuspendImmediate:
        return "NdmSuspendImmediate";
    case CecCommand::StopWait:
        return "StopWait";
    case CecCommand::Stop:
        return "Stop";
    case CecCommand::StopForce:
        return "StopForce";
    case CecCommand::StopForceWait:
        return "StopForceWait";
    case CecCommand::ResetFilter:
        return "ResetFilter";
    case CecCommand::DaemonStop:
        return "DaemonStop";
    case CecCommand::DaemonStart:
        return "DaemonStart";
    case CecCommand::Exit:
        return "Exit";
    case CecCommand::OverBoss:
        return "OverBoss";
    case CecCommand::OverBossForce:
        return "OverBossForce";
    case CecCommand::OverBossForceWait:
        return "OverBossForceWait";
    case CecCommand::End:
        return "End";
    default:
        return "Unknown";
    }
}

void Module::CheckAndUpdateFile(const CecDataPathType path_type, const u32 ncch_program_id,
                                std::vector<u8>& file_buffer) {
    constexpr u32 max_num_boxes = 24;
    constexpr u32 name_size = 16;      // fixed size 16 characters long
    constexpr u32 valid_name_size = 8; // 8 characters are valid, the rest are null
    const u32 file_size = static_cast<u32>(file_buffer.size());

    switch (path_type) {
    case CecDataPathType::MboxList: {
        CecMBoxListHeader mbox_list_header = {};
        std::memcpy(&mbox_list_header, file_buffer.data(), sizeof(CecMBoxListHeader));

        LOG_DEBUG(Service_CECD, "CecMBoxList: magic={:#06x}, version={:#06x}, num_boxes={:#06x}",
                  mbox_list_header.magic, mbox_list_header.version, mbox_list_header.num_boxes);

        if (file_size != sizeof(CecMBoxListHeader)) { // 0x18C
            LOG_DEBUG(Service_CECD, "CecMBoxListHeader size is incorrect: {}", file_size);
        }

        if (mbox_list_header.magic != 0x6868) { // 'hh'
            if (mbox_list_header.magic == 0 || mbox_list_header.magic == 0xFFFF) {
                LOG_DEBUG(Service_CECD, "CecMBoxListHeader magic number is not set");
            } else {
                LOG_DEBUG(Service_CECD, "CecMBoxListHeader magic number is incorrect: {}",
                          mbox_list_header.magic);
            }
            std::memset(&mbox_list_header, 0, sizeof(CecMBoxListHeader));
            mbox_list_header.magic = 0x6868;
        }

        if (mbox_list_header.version != 0x01) { // Not quite sure if it is a version
            if (mbox_list_header.version == 0)
                LOG_DEBUG(Service_CECD, "CecMBoxListHeader version is not set");
            else
                LOG_DEBUG(Service_CECD, "CecMBoxListHeader version is incorrect: {}",
                          mbox_list_header.version);
            mbox_list_header.version = 0x01;
        }

        if (mbox_list_header.num_boxes > 24) {
            LOG_DEBUG(Service_CECD, "CecMBoxListHeader number of boxes is too large: {}",
                      mbox_list_header.num_boxes);
        } else {
            std::vector<u8> name_buffer(name_size);
            std::memset(name_buffer.data(), 0, name_size);

            if (ncch_program_id != 0) {
                std::string name = fmt::format("{:08x}", ncch_program_id);
                std::memcpy(name_buffer.data(), name.data(), name.size());

                bool already_activated = false;
                for (auto i = 0; i < mbox_list_header.num_boxes; i++) {
                    // Box names start at offset 0xC, are 16 char long, first 8 id, last 8 null
                    if (std::memcmp(name_buffer.data(), &mbox_list_header.box_names[i],
                                    valid_name_size) == 0) {
                        LOG_DEBUG(Service_CECD, "Title already activated");
                        already_activated = true;
                    }
                };

                if (!already_activated) {
                    if (mbox_list_header.num_boxes < max_num_boxes) { // max boxes
                        LOG_DEBUG(Service_CECD, "Adding title to mboxlist____: {}", name);
                        std::memcpy(&mbox_list_header.box_names[mbox_list_header.num_boxes],
                                    name_buffer.data(), name_size);
                        mbox_list_header.num_boxes++;
                    }
                }
            } else { // ncch_program_id == 0, remove/update activated boxes
                /// We need to read the /CEC directory to find out which titles, if any,
                /// are activated. The num_of_titles = (total_read_count) - 1, to adjust for
                /// the MBoxList____ file that is present in the directory as well.
                FileSys::Path root_path(
                    GetCecDataPathTypeAsString(CecDataPathType::RootDir, 0).data());

                auto dir_result = cecd_system_save_data_archive->OpenDirectory(root_path);

                auto root_dir = std::move(dir_result).Unwrap();
                std::vector<FileSys::Entry> entries(max_num_boxes + 1); // + 1 mboxlist
                const u32 entry_count = root_dir->Read(max_num_boxes + 1, entries.data());
                root_dir->Close();

                LOG_DEBUG(Service_CECD, "Number of entries found in /CEC: {}", entry_count);

                std::string mbox_list_name("MBoxList____");
                std::string file_name;
                std::u16string u16_filename;

                // Loop through entries but don't add mboxlist____ to itself.
                for (u32 i = 0; i < entry_count; i++) {
                    u16_filename = std::u16string(entries[i].filename);
                    file_name = Common::UTF16ToUTF8(u16_filename);

                    if (mbox_list_name.compare(file_name) != 0) {
                        LOG_DEBUG(Service_CECD, "Adding title to mboxlist____: {}", file_name);
                        std::memcpy(&mbox_list_header.box_names[mbox_list_header.num_boxes++],
                                    file_name.data(), valid_name_size);
                    }
                }
            }
        }
        std::memcpy(file_buffer.data(), &mbox_list_header, sizeof(CecMBoxListHeader));
        break;
    }
    case CecDataPathType::MboxInfo: {
        CecMBoxInfoHeader mbox_info_header = {};
        std::memcpy(&mbox_info_header, file_buffer.data(), sizeof(CecMBoxInfoHeader));

        LOG_DEBUG(Service_CECD,
                  "CecMBoxInfoHeader: magic={:#06x}, program_id={:#010x}, "
                  "private_id={:#010x}, flag={:#04x}, flag2={:#04x}",
                  mbox_info_header.magic, mbox_info_header.program_id, mbox_info_header.private_id,
                  mbox_info_header.flag, mbox_info_header.flag2);

        if (file_size != sizeof(CecMBoxInfoHeader)) { // 0x60
            LOG_DEBUG(Service_CECD, "CecMBoxInfoHeader size is incorrect: {}", file_size);
        }

        if (mbox_info_header.magic != 0x6363) { // 'cc'
            if (mbox_info_header.magic == 0)
                LOG_DEBUG(Service_CECD, "CecMBoxInfoHeader magic number is not set");
            else
                LOG_DEBUG(Service_CECD, "CecMBoxInfoHeader magic number is incorrect: {}",
                          mbox_info_header.magic);
            mbox_info_header.magic = 0x6363;
        }

        if (mbox_info_header.program_id != ncch_program_id) {
            if (mbox_info_header.program_id == 0)
                LOG_DEBUG(Service_CECD, "CecMBoxInfoHeader program id is not set");
            else
                LOG_DEBUG(Service_CECD, "CecMBoxInfoHeader program id doesn't match current id: {}",
                          mbox_info_header.program_id);
        }

        std::memcpy(file_buffer.data(), &mbox_info_header, sizeof(CecMBoxInfoHeader));
        break;
    }
    case CecDataPathType::InboxInfo: {
        CecBoxInfoHeader inbox_info_header = {};
        std::memcpy(&inbox_info_header, file_buffer.data(), sizeof(CecBoxInfoHeader));

        LOG_DEBUG(Service_CECD,
                  "CecBoxInfoHeader: magic={:#06x}, box_info_size={:#010x}, "
                  "max_box_size={:#010x}, box_size={:#010x}, "
                  "max_message_num={:#010x}, message_num={:#010x}, "
                  "max_batch_size={:#010x}, max_message_size={:#010x}",
                  inbox_info_header.magic, inbox_info_header.box_info_size,
                  inbox_info_header.max_box_size, inbox_info_header.box_size,
                  inbox_info_header.max_message_num, inbox_info_header.message_num,
                  inbox_info_header.max_batch_size, inbox_info_header.max_message_size);

        if (inbox_info_header.magic != 0x6262) { // 'bb'
            if (inbox_info_header.magic == 0)
                LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader magic number is not set");
            else
                LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader magic number is incorrect: {}",
                          inbox_info_header.magic);
            inbox_info_header.magic = 0x6262;
        }

        if (inbox_info_header.box_info_size != file_size) {
            if (inbox_info_header.box_info_size == 0)
                LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader box info size is not set");
            else
                LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader box info size is incorrect:",
                          inbox_info_header.box_info_size);
            inbox_info_header.box_info_size = sizeof(CecBoxInfoHeader);
        }

        if (inbox_info_header.max_box_size == 0) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max box size is not set");
        } else if (inbox_info_header.max_box_size > 0x100000) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max box size is too large: {}",
                      inbox_info_header.max_box_size);
        }

        if (inbox_info_header.max_message_num == 0) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max message number is not set");
        } else if (inbox_info_header.max_message_num > 99) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max message number is too large: {}",
                      inbox_info_header.max_message_num);
        }

        if (inbox_info_header.max_message_size == 0) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max message size is not set");
        } else if (inbox_info_header.max_message_size > 0x019000) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max message size is too large");
        }

        if (inbox_info_header.max_batch_size == 0) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max batch size is not set");
            inbox_info_header.max_batch_size = inbox_info_header.max_message_num;
        } else if (inbox_info_header.max_batch_size != inbox_info_header.max_message_num) {
            LOG_DEBUG(Service_CECD, "CecInBoxInfoHeader max batch size != max message number");
        }

        std::memcpy(file_buffer.data(), &inbox_info_header, sizeof(CecBoxInfoHeader));
        break;
    }
    case CecDataPathType::OutboxInfo: {
        CecBoxInfoHeader outbox_info_header = {};
        std::memcpy(&outbox_info_header, file_buffer.data(), sizeof(CecBoxInfoHeader));

        LOG_DEBUG(Service_CECD,
                  "CecBoxInfoHeader: magic={:#06x}, box_info_size={:#010x}, "
                  "max_box_size={:#010x}, box_size={:#010x}, "
                  "max_message_num={:#010x}, message_num={:#010x}, "
                  "max_batch_size={:#010x}, max_message_size={:#010x}",
                  outbox_info_header.magic, outbox_info_header.box_info_size,
                  outbox_info_header.max_box_size, outbox_info_header.box_size,
                  outbox_info_header.max_message_num, outbox_info_header.message_num,
                  outbox_info_header.max_batch_size, outbox_info_header.max_message_size);

        if (outbox_info_header.magic != 0x6262) { // 'bb'
            if (outbox_info_header.magic == 0)
                LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader magic number is not set");
            else
                LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader magic number is incorrect: {}",
                          outbox_info_header.magic);
            outbox_info_header.magic = 0x6262;
        }

        if (outbox_info_header.box_info_size != file_buffer.size()) {
            if (outbox_info_header.box_info_size == 0)
                LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader box info size is not set");
            else
                LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader box info size is incorrect:",
                          outbox_info_header.box_info_size);
            outbox_info_header.box_info_size = sizeof(CecBoxInfoHeader);
            outbox_info_header.message_num = 0;
        }

        if (outbox_info_header.max_box_size == 0) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max box size is not set");
        } else if (outbox_info_header.max_box_size > 0x100000) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max box size is too large");
        }

        if (outbox_info_header.max_message_num == 0) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max message number is not set");
        } else if (outbox_info_header.max_message_num > 99) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max message number is too large");
        }

        if (outbox_info_header.max_message_size == 0) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max message size is not set");
        } else if (outbox_info_header.max_message_size > 0x019000) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max message size is too large");
        }

        if (outbox_info_header.max_batch_size == 0) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max batch size is not set");
            outbox_info_header.max_batch_size = outbox_info_header.max_message_num;
        } else if (outbox_info_header.max_batch_size != outbox_info_header.max_message_num) {
            LOG_DEBUG(Service_CECD, "CecOutBoxInfoHeader max batch size != max message number");
        }

        /// We need to read the /CEC/<id>/OutBox directory to find out which messages, if any,
        /// are present. The num_of_messages = (total_read_count) - 2, to adjust for
        /// the BoxInfo____ and OBIndex_____files that are present in the directory as well.
        FileSys::Path outbox_path(
            GetCecDataPathTypeAsString(CecDataPathType::OutboxDir, ncch_program_id).data());

        auto dir_result = cecd_system_save_data_archive->OpenDirectory(outbox_path);

        auto outbox_dir = std::move(dir_result).Unwrap();
        std::vector<FileSys::Entry> entries(outbox_info_header.max_message_num + 2);
        const u32 entry_count =
            outbox_dir->Read(outbox_info_header.max_message_num + 2, entries.data());
        outbox_dir->Close();

        LOG_DEBUG(Service_CECD, "Number of entries found in /OutBox: {}", entry_count);
        std::array<CecMessageHeader, 8> message_headers;

        std::string boxinfo_name("BoxInfo_____");
        std::string obindex_name("OBIndex_____");
        std::string file_name;
        std::u16string u16_filename;

        for (u32 i = 0; i < entry_count; i++) {
            u16_filename = std::u16string(entries[i].filename);
            file_name = Common::UTF16ToUTF8(u16_filename);

            if (boxinfo_name.compare(file_name) != 0 && obindex_name.compare(file_name) != 0) {
                LOG_DEBUG(Service_CECD, "Adding message to BoxInfo_____: {}", file_name);

                FileSys::Path message_path(
                    (GetCecDataPathTypeAsString(CecDataPathType::OutboxDir, ncch_program_id) + "/" +
                     file_name)
                        .data());

                FileSys::Mode mode;
                mode.read_flag.Assign(1);

                auto message_result = cecd_system_save_data_archive->OpenFile(message_path, mode);

                auto message = std::move(message_result).Unwrap();
                const u32 message_size = static_cast<u32>(message->GetSize());
                std::vector<u8> buffer(message_size);

                void(message->Read(0, message_size, buffer.data()).Unwrap());
                message->Close();

                std::memcpy(&message_headers[outbox_info_header.message_num++], buffer.data(),
                            sizeof(CecMessageHeader));
            }
        }

        if (outbox_info_header.message_num > 0) {
            const u32 message_headers_size =
                outbox_info_header.message_num * sizeof(CecMessageHeader);

            file_buffer.resize(sizeof(CecBoxInfoHeader) + message_headers_size, 0);
            outbox_info_header.box_info_size += message_headers_size;

            std::memcpy(file_buffer.data() + sizeof(CecBoxInfoHeader), &message_headers,
                        message_headers_size);
        }

        std::memcpy(file_buffer.data(), &outbox_info_header, sizeof(CecBoxInfoHeader));
        break;
    }
    case CecDataPathType::OutboxIndex: {
        CecOBIndexHeader obindex_header = {};
        std::memcpy(&obindex_header, file_buffer.data(), sizeof(CecOBIndexHeader));

        if (file_size < sizeof(CecOBIndexHeader)) { // 0x08, minimum size
            LOG_DEBUG(Service_CECD, "CecOBIndexHeader size is too small: {}", file_size);
        }

        if (obindex_header.magic != 0x6767) { // 'gg'
            if (obindex_header.magic == 0)
                LOG_DEBUG(Service_CECD, "CecOBIndexHeader magic number is not set");
            else
                LOG_DEBUG(Service_CECD, "CecOBIndexHeader magic number is incorrect: {}",
                          obindex_header.magic);
            obindex_header.magic = 0x6767;
        }

        if (obindex_header.message_num == 0) {
            if (file_size > sizeof(CecOBIndexHeader)) {
                LOG_DEBUG(Service_CECD, "CecOBIndexHeader message number is not set");
                obindex_header.message_num = (file_size % 8) - 1; // 8 byte message id - 1 header
            }
        } else if (obindex_header.message_num != (file_size % 8) - 1) {
            LOG_DEBUG(Service_CECD, "CecOBIndexHeader message number is incorrect: {}",
                      obindex_header.message_num);
            obindex_header.message_num = 0;
        }

        /// We need to read the /CEC/<id>/OutBox directory to find out which messages, if any,
        /// are present. The num_of_messages = (total_read_count) - 2, to adjust for
        /// the BoxInfo____ and OBIndex_____files that are present in the directory as well.
        FileSys::Path outbox_path(
            GetCecDataPathTypeAsString(CecDataPathType::OutboxDir, ncch_program_id).data());

        auto dir_result = cecd_system_save_data_archive->OpenDirectory(outbox_path);

        auto outbox_dir = std::move(dir_result).Unwrap();
        std::vector<FileSys::Entry> entries(8);
        const u32 entry_count = outbox_dir->Read(8, entries.data());
        outbox_dir->Close();

        LOG_DEBUG(Service_CECD, "Number of entries found in /OutBox: {}", entry_count);
        std::array<std::array<u8, 8>, 8> message_ids;

        std::string boxinfo_name("BoxInfo_____");
        std::string obindex_name("OBIndex_____");
        std::string file_name;
        std::u16string u16_filename;

        for (u32 i = 0; i < entry_count; i++) {
            u16_filename = std::u16string(entries[i].filename);
            file_name = Common::UTF16ToUTF8(u16_filename);

            if (boxinfo_name.compare(file_name) != 0 && obindex_name.compare(file_name) != 0) {
                FileSys::Path message_path(
                    (GetCecDataPathTypeAsString(CecDataPathType::OutboxDir, ncch_program_id) + "/" +
                     file_name)
                        .data());

                FileSys::Mode mode;
                mode.read_flag.Assign(1);

                auto message_result = cecd_system_save_data_archive->OpenFile(message_path, mode);

                auto message = std::move(message_result).Unwrap();
                const u32 message_size = static_cast<u32>(message->GetSize());
                std::vector<u8> buffer(message_size);

                void(message->Read(0, message_size, buffer.data()).Unwrap());
                message->Close();

                // Message id is at offset 0x20, and is 8 bytes
                std::memcpy(&message_ids[obindex_header.message_num++], buffer.data() + 0x20, 8);
            }
        }

        if (obindex_header.message_num > 0) {
            const u32 message_ids_size = obindex_header.message_num * 8;
            file_buffer.resize(sizeof(CecOBIndexHeader) + message_ids_size);
            std::memcpy(file_buffer.data() + sizeof(CecOBIndexHeader), &message_ids,
                        message_ids_size);
        }

        std::memcpy(file_buffer.data(), &obindex_header, sizeof(CecOBIndexHeader));
        break;
    }
    case CecDataPathType::InboxMsg:
        break;
    case CecDataPathType::OutboxMsg:
        break;
    case CecDataPathType::RootDir:
    case CecDataPathType::MboxDir:
    case CecDataPathType::InboxDir:
    case CecDataPathType::OutboxDir:
        break;
    case CecDataPathType::MboxData:
    case CecDataPathType::MboxIcon:
    case CecDataPathType::MboxTitle:
    default: {
    }
    }
}

Module::SessionData::SessionData() {}

Module::SessionData::~SessionData() {
    if (file)
        file->Close();
}

Module::Interface::Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cecd(std::move(cecd)) {}

Module::Module(Core::System& system) : system(system) {
    using namespace Kernel;
    cecinfo_event = system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "CECD::cecinfo_event");
    cecinfosys_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "CECD::cecinfosys_event");
    change_state_event =
        system.Kernel().CreateEvent(Kernel::ResetType::OneShot, "CECD::change_state_event");

    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010026
    FileSys::Path archive_path(cecd_system_savedata_id);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() != FileSys::ResultNotFound) {
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the CECD SystemSaveData archive!");
        cecd_system_save_data_archive = std::move(archive_result).Unwrap();
    } else {
        // Format the archive to create the directories
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0, 0, 0);

        // Open it again to get a valid archive now that the folder exists
        cecd_system_save_data_archive = systemsavedata_factory.Open(archive_path, 0).Unwrap();

        /// Now that the archive is formatted, we need to create the root CEC directory,
        /// eventlog.dat, and CEC/MBoxList____
        const FileSys::Path root_dir_path(
            GetCecDataPathTypeAsString(CecDataPathType::RootDir, 0).data());
        cecd_system_save_data_archive->CreateDirectory(root_dir_path);

        FileSys::Mode mode;
        mode.write_flag.Assign(1);
        mode.create_flag.Assign(1);

        /// eventlog.dat resides in the root of the archive beside the CEC directory
        /// Initially created, at offset 0x0, are bytes 0x01 0x41 0x12, followed by
        /// zeroes until offset 0x1000, where it changes to 0xDD until the end of file
        /// at offset 0x30d53. 0xDD means that the cec module hasn't written data to that
        /// region yet.
        FileSys::Path eventlog_path("/eventlog.dat");

        auto eventlog_result = cecd_system_save_data_archive->OpenFile(eventlog_path, mode);

        constexpr u32 eventlog_size = 0x30d54;
        auto eventlog = std::move(eventlog_result).Unwrap();
        std::vector<u8> eventlog_buffer(eventlog_size);

        std::memset(&eventlog_buffer[0], 0, 0x1000);
        eventlog_buffer[0] = 0x01;
        eventlog_buffer[1] = 0x41;
        eventlog_buffer[2] = 0x12;

        eventlog->Write(0, eventlog_size, true, false, eventlog_buffer.data());
        eventlog->Close();

        /// MBoxList____ resides within the root CEC/ directory.
        /// Initially created, at offset 0x0, are bytes 0x68 0x68 0x00 0x00 0x01, with 0x6868 'hh',
        /// being the magic number. The rest of the file is filled with zeroes, until the end of
        /// file at offset 0x18b
        FileSys::Path mboxlist_path(
            GetCecDataPathTypeAsString(CecDataPathType::MboxList, 0).data());

        auto mboxlist_result = cecd_system_save_data_archive->OpenFile(mboxlist_path, mode);

        constexpr u32 mboxlist_size = 0x18c;
        auto mboxlist = std::move(mboxlist_result).Unwrap();
        std::vector<u8> mboxlist_buffer(mboxlist_size);

        std::memset(&mboxlist_buffer[0], 0, mboxlist_size);
        mboxlist_buffer[0] = 0x68;
        mboxlist_buffer[1] = 0x68;
        // mboxlist_buffer[2-3] are already zeroed
        mboxlist_buffer[4] = 0x01;

        mboxlist->Write(0, mboxlist_size, true, false, mboxlist_buffer.data());
        mboxlist->Close();
    }
}

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto cecd = std::make_shared<Module>(system);
    std::make_shared<CECD_NDM>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_S>(cecd)->InstallAsService(service_manager);
    std::make_shared<CECD_U>(cecd)->InstallAsService(service_manager);
}

} // namespace Service::CECD
