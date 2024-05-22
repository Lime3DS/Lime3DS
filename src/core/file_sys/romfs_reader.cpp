#include <algorithm>
#include <vector>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "common/archives.h"
#include "common/logging/log.h"
#include "core/file_sys/archive_artic.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/romfs_reader.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/loader/loader.h"

SERIALIZE_EXPORT_IMPL(FileSys::DirectRomFSReader)

namespace FileSys {

std::size_t DirectRomFSReader::ReadFile(std::size_t offset, std::size_t length, u8* buffer) {
    length = std::min(length, static_cast<std::size_t>(data_size) - offset);
    if (length == 0)
        return 0; // Crypto++ does not like zero size buffer

    const auto segments = BreakupRead(offset, length);
    std::size_t read_progress = 0;

    // Skip cache if the read is too big
    if (segments.size() == 1 && segments[0].second > cache_line_size) {
        length = file.ReadAtBytes(buffer, length, file_offset + offset);
        if (is_encrypted) {
            CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d(key.data(), key.size(), ctr.data());
            d.Seek(crypto_offset + offset);
            d.ProcessData(buffer, buffer, length);
        }
        LOG_TRACE(Service_FS, "RomFS Cache SKIP: offset={}, length={}", offset, length);
        return length;
    }

    // TODO(PabloMK7): Make cache thread safe, read the comment in CacheReady function.
    // std::unique_lock<std::shared_mutex> read_guard(cache_mutex);
    for (const auto& seg : segments) {
        std::size_t read_size = cache_line_size;
        std::size_t page = OffsetToPage(seg.first);
        // Check if segment is in cache
        auto cache_entry = cache.request(page);
        if (!cache_entry.first) {
            // If not found, read from disk and cache the data
            read_size = file.ReadAtBytes(cache_entry.second.data(), read_size, file_offset + page);
            if (is_encrypted && read_size) {
                CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption d(key.data(), key.size(), ctr.data());
                d.Seek(crypto_offset + page);
                d.ProcessData(cache_entry.second.data(), cache_entry.second.data(), read_size);
            }
            LOG_TRACE(Service_FS, "RomFS Cache MISS: page={}, length={}, into={}", page, seg.second,
                      (seg.first - page));
        } else {
            LOG_TRACE(Service_FS, "RomFS Cache HIT: page={}, length={}, into={}", page, seg.second,
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

bool DirectRomFSReader::AllowsCachedReads() const {
    return true;
}

bool DirectRomFSReader::CacheReady(std::size_t file_offset, std::size_t length) {
    auto segments = BreakupRead(file_offset, length);
    if (segments.size() == 1 && segments[0].second > cache_line_size) {
        return false;
    } else {
        // TODO(PabloMK7): Since the LRU cache is not thread safe, a lock must be used.
        // However, this completely breaks the point of using a cache, because
        // smaller reads may be blocked by bigger reads. For now, always return
        // data being in cache to prevent the need of a lock, and only read data
        // asynchronously if it is too big to use the cache.
        /*
        std::shared_lock<std::shared_mutex> read_guard(cache_mutex);
        for (auto it = segments.begin(); it != segments.end(); it++) {
            if (!cache.contains(OffsetToPage(it->first)))
                return false;
        }
        */
        return true;
    }
}

std::vector<std::pair<std::size_t, std::size_t>> DirectRomFSReader::BreakupRead(
    std::size_t offset, std::size_t length) {

    std::vector<std::pair<std::size_t, std::size_t>> ret;

    // Reads bigger than the cache line size will probably never hit again
    if (length > cache_line_size) {
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

ArticRomFSReader::ArticRomFSReader(std::shared_ptr<Network::ArticBase::Client>& cli,
                                   bool is_update_romfs)
    : client(cli), cache(cli) {
    auto req = client->NewRequest("FSUSER_OpenFileDirectly");

    FileSys::Path archive(FileSys::LowPathType::Empty, {});
    std::vector<u8> fileVec(0xC);
    fileVec[0] = static_cast<u8>(is_update_romfs ? 5 : 0);
    FileSys::Path file(FileSys::LowPathType::Binary, fileVec);

    req.AddParameterS32(static_cast<s32>(Service::FS::ArchiveIdCode::SelfNCCH));

    auto archive_buf = ArticArchive::BuildFSPath(archive);
    req.AddParameterBuffer(archive_buf.data(), archive_buf.size());
    auto file_buf = ArticArchive::BuildFSPath(file);
    req.AddParameterBuffer(file_buf.data(), file_buf.size());

    req.AddParameterS32(1);
    req.AddParameterS32(0);

    auto resp = client->Send(req);

    if (!resp.has_value() || !resp->Succeeded()) {
        load_status = Loader::ResultStatus::Error;
        return;
    }
    if (resp->GetMethodResult() != 0) {
        load_status = Loader::ResultStatus::ErrorNotUsed;
        return;
    }

    auto handle_buf = resp->GetResponseBuffer(0);
    if (!handle_buf.has_value() || handle_buf->second != sizeof(s32)) {
        load_status = Loader::ResultStatus::Error;
        return;
    }

    romfs_handle = *reinterpret_cast<s32*>(handle_buf->first);

    req = client->NewRequest("FSFILE_GetSize");

    req.AddParameterS32(romfs_handle);

    resp = client->Send(req);

    if (!resp.has_value() || !resp->Succeeded()) {
        load_status = Loader::ResultStatus::Error;
        return;
    }
    if (resp->GetMethodResult() != 0) {
        load_status = Loader::ResultStatus::ErrorNotUsed;
        return;
    }

    auto size_buf = resp->GetResponseBuffer(0);
    if (!size_buf.has_value() || size_buf->second != sizeof(u64)) {
        load_status = Loader::ResultStatus::Error;
        return;
    }

    data_size = static_cast<size_t>(*reinterpret_cast<u64*>(size_buf->first));
    load_status = Loader::ResultStatus::Success;
}

ArticRomFSReader::~ArticRomFSReader() {
    if (romfs_handle != -1) {
        auto req = client->NewRequest("FSFILE_Close");
        req.AddParameterS32(romfs_handle);
        client->Send(req);
        romfs_handle = -1;
    }
}

std::size_t ArticRomFSReader::ReadFile(std::size_t offset, std::size_t length, u8* buffer) {
    length = std::min(length, static_cast<std::size_t>(data_size) - offset);
    auto res = cache.Read(romfs_handle, offset, length, buffer);
    if (res.Failed())
        return 0;
    return res.Unwrap();
}

bool ArticRomFSReader::AllowsCachedReads() const {
    return true;
}

bool ArticRomFSReader::CacheReady(std::size_t file_offset, std::size_t length) {
    return cache.CacheReady(file_offset, length);
}

void ArticRomFSReader::CloseFile() {
    if (romfs_handle != -1) {
        auto req = client->NewRequest("FSFILE_Close");
        req.AddParameterS32(romfs_handle);
        client->Send(req);
        romfs_handle = -1;
    }
}

} // namespace FileSys
