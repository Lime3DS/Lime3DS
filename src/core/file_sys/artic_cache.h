// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <shared_mutex>
#include "vector"

#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include "common/alignment.h"
#include "common/common_types.h"
#include "common/static_lru_cache.h"
#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"
#include "network/artic_base/artic_base_client.h"

namespace FileSys {
class ArticCache {
public:
    ArticCache() = default;

    ArticCache(const std::shared_ptr<Network::ArticBase::Client>& cli) : client(cli) {}

    ResultVal<std::size_t> Read(s32 file_handle, std::size_t offset, std::size_t length,
                                u8* buffer);

    bool CacheReady(std::size_t file_offset, std::size_t length);

    void Clear();

    ResultVal<std::size_t> Write(s32 file_handle, std::size_t offset, std::size_t length,
                                 const u8* buffer, u32 flags);

    ResultVal<size_t> GetSize(s32 file_handle);

    void ForceSetSize(const std::optional<size_t>& size) {
        data_size = size;
    }

private:
    std::shared_ptr<Network::ArticBase::Client> client;
    std::optional<size_t> data_size;

    // Total cache size: 32MB small, 512MB big (worst case), 160MB very big (worst case).
    // The worst case values are unrealistic, they will never happen in any real game.
    static constexpr std::size_t cache_line_size = 4 * 1024;
    static constexpr std::size_t cache_line_count = 256;
    static constexpr std::size_t max_breakup_size = 8 * 1024;

    static constexpr std::size_t big_cache_skip = 1 * 1024 * 1024;
    static constexpr std::size_t big_cache_lines = 1024;

    static constexpr std::size_t very_big_cache_skip = 10 * 1024 * 1024;
    static constexpr std::size_t very_big_cache_lines = 24;

    Common::StaticLRUCache<std::size_t, std::array<u8, cache_line_size>, cache_line_count> cache;
    std::shared_mutex cache_mutex;

    struct NoInitChar {
        u8 value;
        NoInitChar() noexcept {
            // do nothing
            static_assert(sizeof *this == sizeof value, "invalid size");
        }
    };
    Common::StaticLRUCache<std::pair<std::size_t, std::size_t>, std::vector<NoInitChar>,
                           big_cache_lines>
        big_cache;
    std::shared_mutex big_cache_mutex;
    Common::StaticLRUCache<std::pair<std::size_t, std::size_t>, std::vector<NoInitChar>,
                           very_big_cache_lines>
        very_big_cache;
    std::shared_mutex very_big_cache_mutex;

    ResultVal<std::size_t> ReadFromArtic(s32 file_handle, u8* buffer, size_t len, size_t offset);

    std::size_t OffsetToPage(std::size_t offset) {
        return Common::AlignDown<std::size_t>(offset, cache_line_size);
    }

    std::vector<std::pair<std::size_t, std::size_t>> BreakupRead(std::size_t offset,
                                                                 std::size_t length);

protected:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;
};

class ArticCacheProvider {
public:
    virtual ~ArticCacheProvider() {}

    std::vector<u8> PathsToVector(const Path& archive_path, const Path& file_path) {
        auto archive_path_binary = archive_path.AsBinary();
        auto file_path_binary = file_path.AsBinary();

        std::vector<u8> ret;
        ret.push_back(static_cast<u8>(file_path.GetType()));
        ret.insert(ret.end(), archive_path_binary.begin(), archive_path_binary.end());
        ret.push_back(static_cast<u8>(archive_path.GetType()));
        ret.insert(ret.end(), file_path_binary.begin(), file_path_binary.end());
        return ret;
    }

    virtual std::shared_ptr<ArticCache> ProvideCache(
        const std::shared_ptr<Network::ArticBase::Client>& cli, const std::vector<u8>& path,
        bool create) {
        if (file_caches == nullptr)
            return nullptr;

        auto it = file_caches->find(path);
        if (it == file_caches->end()) {
            if (!create) {
                return nullptr;
            }
            auto res = std::make_shared<ArticCache>(cli);
            file_caches->insert({path, res});
            return res;
        }
        return it->second;
    }

    virtual void ClearAllCache() {
        if (file_caches != nullptr) {
            file_caches->clear();
        }
    }

    virtual void EnsureCacheCreated() {
        if (file_caches == nullptr) {
            file_caches =
                std::make_unique<std::map<std::vector<u8>, std::shared_ptr<ArticCache>>>();
        }
    }

protected:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {}
    friend class boost::serialization::access;

private:
    std::unique_ptr<std::map<std::vector<u8>, std::shared_ptr<ArticCache>>> file_caches = nullptr;
    std::shared_ptr<Network::ArticBase::Client> client;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::ArticCache)
BOOST_CLASS_EXPORT_KEY(FileSys::ArticCacheProvider)