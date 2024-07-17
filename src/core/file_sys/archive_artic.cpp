// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "archive_artic.h"

namespace FileSys {

std::vector<u8> ArticArchive::BuildFSPath(const Path& path) {
    std::vector<u8> ret(sizeof(u32) * 2);
    u32* raw_data = reinterpret_cast<u32*>(ret.data());
    auto path_type = path.GetType();
    auto binary = path.AsBinary();
    raw_data[0] = static_cast<u32>(path_type);
    raw_data[1] = static_cast<u32>(binary.size());
    if (!binary.empty()) {
        ret.insert(ret.end(), binary.begin(), binary.end());
    }

    // The insert may have invalidated the pointer
    raw_data = reinterpret_cast<u32*>(ret.data());
    if (path_type != LowPathType::Binary && path_type != LowPathType::Invalid) {
        if (path_type == LowPathType::Wchar) {
            raw_data[1] += 2;
            ret.push_back(0);
            ret.push_back(0);
        } else {
            raw_data[1] += 1;
            ret.push_back(0);
        }
    }

    return ret;
}

Result ArticArchive::RespResult(const std::optional<Network::ArticBase::Client::Response>& resp) {
    if (!resp.has_value() || !resp->Succeeded()) {
        return ResultUnknown;
    }
    return Result(static_cast<u32>(resp->GetMethodResult()));
}

ArticArchive::~ArticArchive() {
    if (clear_cache_on_close) {
        cache_provider->ClearAllCache();
    }
    if (archive_handle != -1) {
        auto req = client->NewRequest("FSUSER_CloseArchive");
        req.AddParameterS64(archive_handle);
        client->Send(req);
        if (report_artic_event != Core::PerfStats::PerfArticEventBits::NONE) {
            client->ReportArticEvent(static_cast<u64>(report_artic_event));
        }
    }
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArticArchive::Open(
    std::shared_ptr<Network::ArticBase::Client>& client, Service::FS::ArchiveIdCode archive_id,
    const Path& path, Core::PerfStats::PerfArticEventBits report_artic_event,
    ArticCacheProvider& cache_provider, bool clear_cache_on_close) {

    auto req = client->NewRequest("FSUSER_OpenArchive");

    req.AddParameterS32(static_cast<s32>(archive_id));
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());

    auto resp = client->Send(req);
    if (!resp.has_value() || !resp->Succeeded()) {
        return ResultUnknown;
    }
    Result res(static_cast<u32>(resp->GetMethodResult()));
    if (res.IsError())
        return res;

    auto handle_opt = resp->GetResponseS64(0);
    if (!handle_opt.has_value()) {
        return ResultUnknown;
    }

    return std::make_unique<ArticArchive>(client, *handle_opt, report_artic_event, cache_provider,
                                          path, clear_cache_on_close);
}

void ArticArchive::Close() {
    if (clear_cache_on_close) {
        cache_provider->ClearAllCache();
    }

    auto req = client->NewRequest("FSUSER_CloseArchive");
    req.AddParameterS64(archive_handle);
    if (RespResult(client->Send(req)).IsSuccess()) {
        archive_handle = -1;
        if (report_artic_event != Core::PerfStats::PerfArticEventBits::NONE) {
            client->ReportArticEvent(static_cast<u64>(report_artic_event));
        }
    }
}

std::string ArticArchive::GetName() const {
    return "ArticArchive";
}

ResultVal<std::unique_ptr<FileBackend>> ArticArchive::OpenFile(const Path& path, const Mode& mode,
                                                               u32 attributes) {
    if (mode.create_flag) {
        auto cache = cache_provider->ProvideCache(
            client, cache_provider->PathsToVector(archive_path, path), false);
        if (cache != nullptr) {
            cache->Clear();
        }
    }
    auto req = client->NewRequest("FSUSER_OpenFile");

    req.AddParameterS64(archive_handle);
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());
    req.AddParameterU32(mode.hex);
    req.AddParameterU32(attributes);

    auto resp = client->Send(req);
    auto res = RespResult(resp);
    if (res.IsError())
        return res;

    auto handle_opt = resp->GetResponseS32(0);
    if (!handle_opt.has_value())
        return ResultUnknown;

    auto size_opt = resp->GetResponseU64(1);
    if (size_opt.has_value()) {
        auto cache = cache_provider->ProvideCache(
            client, cache_provider->PathsToVector(archive_path, path), true);
        if (cache != nullptr) {
            cache->ForceSetSize(static_cast<size_t>(*size_opt));
        }
    }

    if (open_reporter->open_files++ == 0 &&
        report_artic_event != Core::PerfStats::PerfArticEventBits::NONE) {
        client->ReportArticEvent(static_cast<u64>(report_artic_event) | (1ULL << 32));
    }

    return std::make_unique<ArticFileBackend>(client, *handle_opt, open_reporter, archive_path,
                                              *cache_provider, path);
}

Result ArticArchive::DeleteFile(const Path& path) const {
    auto cache = cache_provider->ProvideCache(
        client, cache_provider->PathsToVector(archive_path, path), false);
    if (cache != nullptr) {
        cache->Clear();
    }

    auto req = client->NewRequest("FSUSER_DeleteFile");

    req.AddParameterS64(archive_handle);
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());

    return RespResult(client->Send(req));
}

Result ArticArchive::RenameFile(const Path& src_path, const Path& dest_path) const {
    auto cache = cache_provider->ProvideCache(
        client, cache_provider->PathsToVector(archive_path, src_path), false);
    if (cache != nullptr) {
        cache->Clear();
    }
    cache = cache_provider->ProvideCache(
        client, cache_provider->PathsToVector(archive_path, dest_path), false);
    if (cache != nullptr) {
        cache->Clear();
    }

    auto req = client->NewRequest("FSUSER_RenameFile");

    req.AddParameterS64(archive_handle);
    auto src_path_buf = BuildFSPath(src_path);
    req.AddParameterBuffer(src_path_buf.data(), src_path_buf.size());
    req.AddParameterS64(archive_handle);
    auto dest_path_buf = BuildFSPath(dest_path);
    req.AddParameterBuffer(dest_path_buf.data(), dest_path_buf.size());

    return RespResult(client->Send(req));
}

Result ArticArchive::DeleteDirectory(const Path& path) const {
    cache_provider->ClearAllCache();

    auto req = client->NewRequest("FSUSER_DeleteDirectory");

    req.AddParameterS64(archive_handle);
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());

    return RespResult(client->Send(req));
}

Result ArticArchive::DeleteDirectoryRecursively(const Path& path) const {
    cache_provider->ClearAllCache();

    auto req = client->NewRequest("FSUSER_DeleteDirectoryRec");

    req.AddParameterS64(archive_handle);
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());

    return RespResult(client->Send(req));
}

Result ArticArchive::CreateFile(const Path& path, u64 size, u32 attributes) const {
    auto cache = cache_provider->ProvideCache(
        client, cache_provider->PathsToVector(archive_path, path), false);
    if (cache != nullptr) {
        cache->Clear();
    }

    auto req = client->NewRequest("FSUSER_CreateFile");

    req.AddParameterS64(archive_handle);
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());
    req.AddParameterU32(attributes);
    req.AddParameterU64(size);

    return RespResult(client->Send(req));
}

Result ArticArchive::CreateDirectory(const Path& path, u32 attributes) const {
    auto req = client->NewRequest("FSUSER_CreateDirectory");

    req.AddParameterS64(archive_handle);
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());
    req.AddParameterU32(attributes);

    return RespResult(client->Send(req));
}

Result ArticArchive::RenameDirectory(const Path& src_path, const Path& dest_path) const {
    cache_provider->ClearAllCache();

    auto req = client->NewRequest("FSUSER_RenameDirectory");

    req.AddParameterS64(archive_handle);
    auto src_path_buf = BuildFSPath(src_path);
    req.AddParameterBuffer(src_path_buf.data(), src_path_buf.size());
    req.AddParameterS64(archive_handle);
    auto dest_path_buf = BuildFSPath(dest_path);
    req.AddParameterBuffer(dest_path_buf.data(), dest_path_buf.size());

    return RespResult(client->Send(req));
}

ResultVal<std::unique_ptr<DirectoryBackend>> ArticArchive::OpenDirectory(const Path& path) {
    auto req = client->NewRequest("FSUSER_OpenDirectory");

    req.AddParameterS64(archive_handle);
    auto path_buf = BuildFSPath(path);
    req.AddParameterBuffer(path_buf.data(), path_buf.size());

    auto resp = client->Send(req);
    auto res = RespResult(resp);
    if (res.IsError())
        return res;

    auto handle_opt = resp->GetResponseS32(0);
    if (!handle_opt.has_value())
        return ResultUnknown;

    if (open_reporter->open_files++ == 0 &&
        report_artic_event != Core::PerfStats::PerfArticEventBits::NONE) {
        client->ReportArticEvent(static_cast<u64>(report_artic_event) | (1ULL << 32));
    }

    return std::make_unique<ArticDirectoryBackend>(client, *handle_opt, archive_path,
                                                   open_reporter);
}

u64 ArticArchive::GetFreeBytes() const {
    auto req = client->NewRequest("FSUSER_GetFreeBytes");

    req.AddParameterS64(archive_handle);

    auto resp = client->Send(req);
    auto res = RespResult(resp);
    if (res.IsError()) // TODO(PabloMK7): Return error code and not u64
        return 0;

    auto free_bytes_opt = resp->GetResponseS64(0);
    return free_bytes_opt.has_value() ? static_cast<u64>(*free_bytes_opt) : 0;
}

Result ArticArchive::Control(u32 action, u8* input, size_t input_size, u8* output,
                             size_t output_size) {
    auto req = client->NewRequest("FSUSER_ControlArchive");

    req.AddParameterS64(archive_handle);
    req.AddParameterU32(action);
    req.AddParameterBuffer(input, input_size);
    req.AddParameterU32(static_cast<u32>(output_size));

    auto resp = client->Send(req);
    auto res = RespResult(resp);
    if (res.IsError())
        return res;

    auto output_buf = resp->GetResponseBuffer(0);
    if (!output_buf.has_value())
        return res;

    if (output_buf->second != output_size)
        return ResultUnknown;

    memcpy(output, output_buf->first, output_buf->second);
    return res;
}

Result ArticArchive::SetSaveDataSecureValue(u32 secure_value_slot, u64 secure_value, bool flush) {
    auto req = client->NewRequest("FSUSER_SetSaveDataSecureValue");

    req.AddParameterS64(archive_handle);
    req.AddParameterU32(secure_value_slot);
    req.AddParameterU64(secure_value);
    req.AddParameterS8(flush != 0);

    return RespResult(client->Send(req));
}

ResultVal<std::tuple<bool, bool, u64>> ArticArchive::GetSaveDataSecureValue(u32 secure_value_slot) {
    auto req = client->NewRequest("FSUSER_GetSaveDataSecureValue");

    req.AddParameterS64(archive_handle);
    req.AddParameterU32(secure_value_slot);

    auto resp = client->Send(req);
    auto res = RespResult(resp);
    if (res.IsError())
        return res;

    struct {
        bool exists;
        bool isGamecard;
        u64 secure_value;
    } secure_value_result;
    static_assert(sizeof(secure_value_result) == 0x10);

    auto output_buf = resp->GetResponseBuffer(0);
    if (!output_buf.has_value())
        return res;

    if (output_buf->second != sizeof(secure_value_result))
        return ResultUnknown;

    memcpy(&secure_value_result, output_buf->first, output_buf->second);
    return std::make_tuple(secure_value_result.exists, secure_value_result.isGamecard,
                           secure_value_result.secure_value);
}

void ArticArchive::OpenFileReporter::OnFileClosed() {
    if (--open_files == 0 && report_artic_event != Core::PerfStats::PerfArticEventBits::NONE) {
        client->ReportArticEvent(static_cast<u64>(report_artic_event));
    }
}

void ArticArchive::OpenFileReporter::OnDirectoryClosed() {
    if (--open_files == 0 && report_artic_event != Core::PerfStats::PerfArticEventBits::NONE) {
        client->ReportArticEvent(static_cast<u64>(report_artic_event));
    }
}

ArticFileBackend::~ArticFileBackend() {
    if (file_handle != -1) {
        auto req = client->NewRequest("FSFILE_Close");
        req.AddParameterS32(file_handle);
        client->Send(req);
        open_reporter->OnFileClosed();
    }
}

ResultVal<std::size_t> ArticFileBackend::Read(u64 offset, std::size_t length, u8* buffer) const {
    auto cache = cache_provider->ProvideCache(
        client, cache_provider->PathsToVector(archive_path, file_path), true);

    if (cache != nullptr && (offset + static_cast<u64>(length)) < GetSize()) {
        return cache->Read(file_handle, offset, length, buffer);
    }

    size_t read_amount = 0;
    while (read_amount != length) {
        size_t to_read =
            std::min<size_t>(client->GetServerRequestMaxSize() - 0x100, length - read_amount);

        auto req = client->NewRequest("FSFILE_Read");
        req.AddParameterS32(file_handle);
        req.AddParameterS64(static_cast<s64>(offset + read_amount));
        req.AddParameterS32(static_cast<s32>(to_read));
        auto resp = client->Send(req);
        if (!resp.has_value() || !resp->Succeeded())
            return Result(-1);

        auto res = Result(static_cast<u32>(resp->GetMethodResult()));
        if (res.IsError())
            return res;

        auto read_buff = resp->GetResponseBuffer(0);
        size_t actually_read = 0;
        if (read_buff.has_value()) {
            actually_read = read_buff->second;
            memcpy(buffer + read_amount, read_buff->first, actually_read);
        }

        read_amount += actually_read;
        if (actually_read != to_read)
            break;
    }
    return read_amount;
}

ResultVal<std::size_t> ArticFileBackend::Write(u64 offset, std::size_t length, bool flush,
                                               bool update_timestamp, const u8* buffer) {
    u32 flags = (flush ? 1 : 0) | (update_timestamp ? (1 << 8) : 0);
    auto cache = cache_provider->ProvideCache(
        client, cache_provider->PathsToVector(archive_path, file_path), true);
    if (cache != nullptr) {
        return cache->Write(file_handle, offset, length, buffer, flags);
    } else {
        size_t written_amount = 0;
        while (written_amount != length) {
            size_t to_write = std::min<size_t>(client->GetServerRequestMaxSize() - 0x100,
                                               length - written_amount);

            auto req = client->NewRequest("FSFILE_Write");
            req.AddParameterS32(file_handle);
            req.AddParameterS64(static_cast<s64>(offset + written_amount));
            req.AddParameterS32(static_cast<s32>(to_write));
            req.AddParameterS32(static_cast<s32>(flags));
            req.AddParameterBuffer(buffer + written_amount, to_write);
            auto resp = client->Send(req);
            if (!resp.has_value() || !resp->Succeeded())
                return Result(-1);

            auto res = Result(static_cast<u32>(resp->GetMethodResult()));
            if (res.IsError())
                return res;

            auto actually_written_opt = resp->GetResponseS32(0);
            if (!actually_written_opt.has_value())
                return Result(-1);

            size_t actually_written = static_cast<size_t>(actually_written_opt.value());

            written_amount += actually_written;
            if (actually_written != to_write)
                break;
        }
        return written_amount;
    }
}

u64 ArticFileBackend::GetSize() const {
    auto cache = cache_provider->ProvideCache(
        client, cache_provider->PathsToVector(archive_path, file_path), true);
    if (cache != nullptr) {
        auto res = cache->GetSize(file_handle);
        if (res.Failed())
            return 0;
        return res.Unwrap();
    } else {

        auto req = client->NewRequest("FSFILE_GetSize");

        req.AddParameterS32(file_handle);

        auto resp = client->Send(req);
        auto res = ArticArchive::RespResult(resp);
        if (res.IsError())
            return 0;

        auto size_buf = resp->GetResponseS64(0);
        if (!size_buf) {
            return 0;
        }
        return *size_buf;
    }
}

bool ArticFileBackend::SetSize(u64 size) const {
    auto req = client->NewRequest("FSFILE_SetSize");

    req.AddParameterS32(file_handle);
    req.AddParameterU64(size);

    return ArticArchive::RespResult(client->Send(req)).IsSuccess();
}

bool ArticFileBackend::Close() {
    auto req = client->NewRequest("FSFILE_Close");
    req.AddParameterS32(file_handle);
    bool ret = ArticArchive::RespResult(client->Send(req)).IsSuccess();
    if (ret) {
        file_handle = -1;
        open_reporter->OnFileClosed();
    }
    return ret;
}

void ArticFileBackend::Flush() const {
    auto req = client->NewRequest("FSFILE_Flush");

    req.AddParameterS32(file_handle);

    client->Send(req);
}

ArticDirectoryBackend::~ArticDirectoryBackend() {
    if (dir_handle != -1) {
        auto req = client->NewRequest("FSDIR_Close");
        req.AddParameterS32(dir_handle);
        client->Send(req);
        open_reporter->OnDirectoryClosed();
    }
}

u32 ArticDirectoryBackend::Read(const u32 count, Entry* entries) {
    auto req = client->NewRequest("FSDIR_Read");

    req.AddParameterS32(dir_handle);
    req.AddParameterU32(count);

    auto resp = client->Send(req);
    auto res = ArticArchive::RespResult(resp);
    if (res.IsError())
        return 0;

    auto entry_buf = resp->GetResponseBuffer(0);
    if (!entry_buf) {
        return 0;
    }
    u32 ret_count = static_cast<u32>(entry_buf->second / sizeof(Entry));

    memcpy(entries, entry_buf->first, ret_count * sizeof(Entry));
    return ret_count;
}

bool ArticDirectoryBackend::Close() {
    auto req = client->NewRequest("FSDIR_Close");
    req.AddParameterS32(dir_handle);
    bool ret = ArticArchive::RespResult(client->Send(req)).IsSuccess();
    if (ret) {
        dir_handle = -1;
        open_reporter->OnDirectoryClosed();
    }
    return ret;
}
} // namespace FileSys
