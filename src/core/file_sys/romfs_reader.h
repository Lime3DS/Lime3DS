#pragma once

#include <array>
#include <shared_mutex>
#include <boost/serialization/array.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include "common/alignment.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/static_lru_cache.h"
#include "core/file_sys/artic_cache.h"
#include "network/artic_base/artic_base_client.h"

namespace Loader {
enum class ResultStatus;
}

namespace FileSys {

/**
 * Interface for reading RomFS data.
 */
class RomFSReader {
public:
    virtual ~RomFSReader() = default;

    virtual std::size_t GetSize() const = 0;
    virtual std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) = 0;
    virtual bool AllowsCachedReads() const = 0;
    virtual bool CacheReady(std::size_t file_offset, std::size_t length) = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {}
    friend class boost::serialization::access;
};

/**
 * A RomFS reader that directly reads the RomFS file.
 */
class DirectRomFSReader : public RomFSReader {
public:
    DirectRomFSReader(FileUtil::IOFile&& file, std::size_t file_offset, std::size_t data_size)
        : is_encrypted(false), file(std::move(file)), file_offset(file_offset),
          data_size(data_size) {}

    DirectRomFSReader(FileUtil::IOFile&& file, std::size_t file_offset, std::size_t data_size,
                      const std::array<u8, 16>& key, const std::array<u8, 16>& ctr,
                      std::size_t crypto_offset)
        : is_encrypted(true), file(std::move(file)), key(key), ctr(ctr), file_offset(file_offset),
          crypto_offset(crypto_offset), data_size(data_size) {}

    ~DirectRomFSReader() override = default;

    std::size_t GetSize() const override {
        return data_size;
    }

    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) override;

    bool AllowsCachedReads() const override;

    bool CacheReady(std::size_t file_offset, std::size_t length) override;

private:
    bool is_encrypted;
    FileUtil::IOFile file;
    std::array<u8, 16> key;
    std::array<u8, 16> ctr;
    u64 file_offset;
    u64 crypto_offset;
    u64 data_size;

    // Total cache size: 128KB
    static constexpr std::size_t cache_line_size = (1 << 13); // About 8KB
    static constexpr std::size_t cache_line_count = 16;

    Common::StaticLRUCache<std::size_t, std::array<u8, cache_line_size>, cache_line_count> cache;
    // TODO(PabloMK7): Make cache thread safe, read the comment in CacheReady function.
    // std::shared_mutex cache_mutex;

    DirectRomFSReader() = default;

    std::size_t OffsetToPage(std::size_t offset) {
        return Common::AlignDown<std::size_t>(offset, cache_line_size);
    }

    std::vector<std::pair<std::size_t, std::size_t>> BreakupRead(std::size_t offset,
                                                                 std::size_t length);

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<RomFSReader>(*this);
        ar & is_encrypted;
        ar & file;
        ar & key;
        ar & ctr;
        ar & file_offset;
        ar & crypto_offset;
        ar & data_size;
    }
    friend class boost::serialization::access;
};

/**
 * A RomFS reader that reads from an artic base server.
 */
class ArticRomFSReader : public RomFSReader {
public:
    ArticRomFSReader() = default;
    ArticRomFSReader(std::shared_ptr<Network::ArticBase::Client>& cli, bool is_update_romfs);

    ~ArticRomFSReader() override;

    std::size_t GetSize() const override {
        return data_size;
    }

    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) override;

    bool AllowsCachedReads() const override;

    bool CacheReady(std::size_t file_offset, std::size_t length) override;

    Loader::ResultStatus OpenStatus() {
        return load_status;
    }

    void ClearCache() {
        cache.Clear();
    }

    void CloseFile();

private:
    std::shared_ptr<Network::ArticBase::Client> client;
    size_t data_size = 0;
    s32 romfs_handle = -1;
    Loader::ResultStatus load_status;

    ArticCache cache;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<RomFSReader>(*this);
        ar & data_size;
    }
    friend class boost::serialization::access;
};

} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::DirectRomFSReader)
BOOST_CLASS_EXPORT_KEY(FileSys::ArticRomFSReader)
