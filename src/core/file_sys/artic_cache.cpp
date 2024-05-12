// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "artic_cache.h"

namespace FileSys {
ResultVal<std::size_t> ArticCache::Read(s32 file_handle, std::size_t offset, std::size_t length,
                                        u8* buffer) {
    if (length == 0)
        return size_t();

    const auto segments = BreakupRead(offset, length);
    std::size_t read_progress = 0;

    // Skip cache if the read is too big
    if (segments.size() == 1 && segments[0].second > cache_line_size) {
        if (segments[0].second < big_cache_skip) {
            std::unique_lock big_read_guard(big_cache_mutex);
            auto big_cache_entry = big_cache.request(std::make_pair(offset, length));
            if (!big_cache_entry.first) {
                LOG_TRACE(Service_FS, "ArticCache BMISS: offset={}, length={}", offset, length);
                big_cache_entry.second.clear();
                big_cache_entry.second.resize(length);
                auto res =
                    ReadFromArtic(file_handle, reinterpret_cast<u8*>(big_cache_entry.second.data()),
                                  length, offset);
                if (res.Failed())
                    return res;
                length = res.Unwrap();
            } else {
                LOG_TRACE(Service_FS, "ArticCache BHIT: offset={}, length={}", offset, length);
            }
            memcpy(buffer, big_cache_entry.second.data(), length);
        } else {
            if (segments[0].second < very_big_cache_skip) {
                std::unique_lock very_big_read_guard(very_big_cache_mutex);
                auto very_big_cache_entry = very_big_cache.request(std::make_pair(offset, length));
                if (!very_big_cache_entry.first) {
                    LOG_TRACE(Service_FS, "ArticCache VBMISS: offset={}, length={}", offset,
                              length);
                    very_big_cache_entry.second.clear();
                    very_big_cache_entry.second.resize(length);
                    auto res = ReadFromArtic(
                        file_handle, reinterpret_cast<u8*>(very_big_cache_entry.second.data()),
                        length, offset);
                    if (res.Failed())
                        return res;
                    length = res.Unwrap();
                } else {
                    LOG_TRACE(Service_FS, "ArticCache VBHIT: offset={}, length={}", offset, length);
                }
                memcpy(buffer, very_big_cache_entry.second.data(), length);
            } else {
                LOG_TRACE(Service_FS, "ArticCache SKIP: offset={}, length={}", offset, length);

                auto res = ReadFromArtic(file_handle, buffer, length, offset);
                if (res.Failed())
                    return res;
                length = res.Unwrap();
            }
        }
        return length;
    }

    // TODO(PabloMK7): Make cache thread safe, read the comment in CacheReady function.
    std::unique_lock read_guard(cache_mutex);
    for (const auto& seg : segments) {
        std::size_t read_size = cache_line_size;
        std::size_t page = OffsetToPage(seg.first);
        // Check if segment is in cache
        auto cache_entry = cache.request(page);
        if (!cache_entry.first) {
            // If not found, read from artic and cache the data
            auto res = ReadFromArtic(file_handle, cache_entry.second.data(), read_size, page);
            if (res.Failed())
                return res;
            read_size = res.Unwrap();
            LOG_TRACE(Service_FS, "ArticCache MISS: page={}, length={}, into={}", page, seg.second,
                      (seg.first - page));
        } else {
            LOG_TRACE(Service_FS, "ArticCache HIT: page={}, length={}, into={}", page, seg.second,
                      (seg.first - page));
        }
        std::size_t copy_amount =
            (read_size > (seg.first - page))
                ? std::min((seg.first - page) + seg.second, read_size) - (seg.first - page)
                : 0;
        std::memcpy(buffer + read_progress, cache_entry.second.data() + (seg.first - page),
                    copy_amount);
        read_progress += copy_amount;
    }
    return read_progress;
}

bool ArticCache::CacheReady(std::size_t file_offset, std::size_t length) {
    auto segments = BreakupRead(file_offset, length);
    if (segments.size() == 1 && segments[0].second > cache_line_size) {
        return false;
    } else {
        std::shared_lock read_guard(cache_mutex);
        for (auto it = segments.begin(); it != segments.end(); it++) {
            if (!cache.contains(OffsetToPage(it->first)))
                return false;
        }
        return true;
    }
}

void ArticCache::Clear() {
    std::unique_lock l1(cache_mutex), l2(big_cache_mutex), l3(very_big_cache_mutex);
    cache.clear();
    big_cache.clear();
    very_big_cache.clear();
    data_size = std::nullopt;
}

ResultVal<size_t> ArticCache::Write(s32 file_handle, std::size_t offset, std::size_t length,
                                    const u8* buffer, u32 flags) {
    // Can probably do better, but write operations are usually done at the end, so it doesn't
    // matter much
    Clear();

    size_t written_amount = 0;
    while (written_amount != length) {
        size_t to_write =
            std::min<size_t>(client->GetServerRequestMaxSize() - 0x100, length - written_amount);

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

ResultVal<size_t> ArticCache::GetSize(s32 file_handle) {
    std::unique_lock l1(cache_mutex);

    if (data_size.has_value())
        return data_size.value();

    auto req = client->NewRequest("FSFILE_GetSize");

    req.AddParameterS32(file_handle);

    auto resp = client->Send(req);
    if (!resp.has_value() || !resp->Succeeded())
        return Result(-1);

    auto res = Result(static_cast<u32>(resp->GetMethodResult()));
    if (res.IsError())
        return res;

    auto size_buf = resp->GetResponseS64(0);
    if (!size_buf) {
        return Result(-1);
    }

    data_size = static_cast<size_t>(*size_buf);
    return data_size.value();
}

ResultVal<size_t> ArticCache::ReadFromArtic(s32 file_handle, u8* buffer, size_t len,
                                            size_t offset) {
    size_t read_amount = 0;
    while (read_amount != len) {
        size_t to_read =
            std::min<size_t>(client->GetServerRequestMaxSize() - 0x100, len - read_amount);

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
        if (!read_buff.has_value())
            return Result(-1);
        size_t actually_read = read_buff->second;

        memcpy(buffer + read_amount, read_buff->first, actually_read);
        read_amount += actually_read;
        if (actually_read != to_read)
            break;
    }
    return read_amount;
}

std::vector<std::pair<std::size_t, std::size_t>> ArticCache::BreakupRead(std::size_t offset,
                                                                         std::size_t length) {
    std::vector<std::pair<std::size_t, std::size_t>> ret;

    // Reads bigger than the cache line size will probably never hit again
    if (length > max_breakup_size) {
        ret.push_back(std::make_pair(offset, length));
        return ret;
    }

    std::size_t curr_offset = offset;
    while (length) {
        std::size_t next_page = OffsetToPage(curr_offset + cache_line_size);
        std::size_t curr_page_len = std::min(length, next_page - curr_offset);
        ret.push_back(std::make_pair(curr_offset, curr_page_len));
        curr_offset = next_page;
        length -= curr_page_len;
    }
    return ret;
}
} // namespace FileSys
