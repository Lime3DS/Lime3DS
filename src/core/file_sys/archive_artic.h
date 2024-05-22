// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "atomic"

#include <boost/serialization/unique_ptr.hpp>
#include "common/common_types.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/artic_cache.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/fs/archive.h"
#include "core/perf_stats.h"
#include "network/artic_base/artic_base_client.h"

namespace FileSys {

class ArticArchive : public ArchiveBackend {
public:
    static std::vector<u8> BuildFSPath(const Path& path);
    static Result RespResult(const std::optional<Network::ArticBase::Client::Response>& resp);

    explicit ArticArchive(std::shared_ptr<Network::ArticBase::Client>& _client, s64 _archive_handle,
                          Core::PerfStats::PerfArticEventBits _report_artic_event,
                          ArticCacheProvider& _cache_provider, const Path& _archive_path,
                          bool _clear_cache_on_close)
        : client(_client), archive_handle(_archive_handle), report_artic_event(_report_artic_event),
          cache_provider(&_cache_provider), archive_path(_archive_path),
          clear_cache_on_close(_clear_cache_on_close) {
        open_reporter = std::make_shared<OpenFileReporter>(_client, _report_artic_event);
    }
    ~ArticArchive() override;

    static ResultVal<std::unique_ptr<ArchiveBackend>> Open(
        std::shared_ptr<Network::ArticBase::Client>& client, Service::FS::ArchiveIdCode archive_id,
        const Path& path, Core::PerfStats::PerfArticEventBits report_artic_event,
        ArticCacheProvider& cache_provider, bool clear_cache_on_close);

    void Close() override;

    /**
     * Get a descriptive name for the archive (e.g. "RomFS", "SaveData", etc.)
     */
    std::string GetName() const override;

    /**
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or error code
     */
    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path, const Mode& mode,
                                                     u32 attributes) override;

    /**
     * Delete a file specified by its path
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    Result DeleteFile(const Path& path) const override;

    /**
     * Rename a File specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Result of the operation
     */
    Result RenameFile(const Path& src_path, const Path& dest_path) const override;

    /**
     * Delete a directory specified by its path
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    Result DeleteDirectory(const Path& path) const override;

    /**
     * Delete a directory specified by its path and anything under it
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    Result DeleteDirectoryRecursively(const Path& path) const override;

    /**
     * Create a file specified by its path
     * @param path Path relative to the Archive
     * @param size The size of the new file, filled with zeroes
     * @return Result of the operation
     */
    Result CreateFile(const Path& path, u64 size, u32 attributes) const override;

    /**
     * Create a directory specified by its path
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    Result CreateDirectory(const Path& path, u32 attributes) const override;

    /**
     * Rename a Directory specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Result of the operation
     */
    Result RenameDirectory(const Path& src_path, const Path& dest_path) const override;

    /**
     * Open a directory specified by its path
     * @param path Path relative to the archive
     * @return Opened directory, or error code
     */
    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) override;

    /**
     * Get the free space
     * @return The number of free bytes in the archive
     */
    u64 GetFreeBytes() const override;

    Result Control(u32 action, u8* input, size_t input_size, u8* output,
                   size_t output_size) override;

    Result SetSaveDataSecureValue(u32 secure_value_slot, u64 secure_value, bool flush) override;

    ResultVal<std::tuple<bool, bool, u64>> GetSaveDataSecureValue(u32 secure_value_slot) override;

    bool IsSlow() override {
        return true;
    }

    const Path& GetArchivePath() {
        return archive_path;
    }

protected:
    ArticArchive() = default;

private:
    friend class ArticFileBackend;
    friend class ArticDirectoryBackend;
    class OpenFileReporter {
    public:
        OpenFileReporter(const std::shared_ptr<Network::ArticBase::Client>& cli,
                         Core::PerfStats::PerfArticEventBits _report_artic_event)
            : client(cli), report_artic_event(_report_artic_event) {}

        void OnFileClosed();

        void OnDirectoryClosed();

        std::shared_ptr<Network::ArticBase::Client> client;
        Core::PerfStats::PerfArticEventBits report_artic_event =
            Core::PerfStats::PerfArticEventBits::NONE;
        std::atomic<u32> open_files = 0;
    };

    std::shared_ptr<Network::ArticBase::Client> client;
    s64 archive_handle;
    std::shared_ptr<OpenFileReporter> open_reporter;
    Core::PerfStats::PerfArticEventBits report_artic_event =
        Core::PerfStats::PerfArticEventBits::NONE;
    ArticCacheProvider* cache_provider = nullptr;
    Path archive_path;
    bool clear_cache_on_close;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<ArchiveBackend>(*this);
        ar & archive_handle;
    }
    friend class boost::serialization::access;
};

class ArticFileBackend : public FileBackend {
public:
    explicit ArticFileBackend(std::shared_ptr<Network::ArticBase::Client>& _client,
                              s32 _file_handle,
                              const std::shared_ptr<ArticArchive::OpenFileReporter>& _open_reporter,
                              const Path& _archive_path, ArticCacheProvider& _cache_provider,
                              const Path& _file_path)
        : client(_client), file_handle(_file_handle), open_reporter(_open_reporter),
          archive_path(_archive_path), cache_provider(&_cache_provider), file_path(_file_path) {}
    ~ArticFileBackend() override;

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;

    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush, bool update_timestamp,
                                 const u8* buffer) override;

    u64 GetSize() const override;

    bool SetSize(u64 size) const override;

    bool Close() override;

    void Flush() const override;

    bool AllowsCachedReads() const override {
        return true;
    }

    bool CacheReady(std::size_t file_offset, std::size_t length) override {
        auto cache = cache_provider->ProvideCache(
            client, cache_provider->PathsToVector(archive_path, file_path), true);
        if (cache == nullptr) {
            return false;
        }
        return cache->CacheReady(file_offset, length);
    }

protected:
    ArticFileBackend() = default;

private:
    std::shared_ptr<Network::ArticBase::Client> client;
    s32 file_handle;
    std::shared_ptr<ArticArchive::OpenFileReporter> open_reporter;
    Path archive_path;
    ArticCacheProvider* cache_provider = nullptr;
    Path file_path;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<FileBackend>(*this);
        ar & file_handle;
    }
    friend class boost::serialization::access;
};

class ArticDirectoryBackend : public DirectoryBackend {
public:
    explicit ArticDirectoryBackend(
        std::shared_ptr<Network::ArticBase::Client>& _client, s32 _dir_handle,
        const Path& _archive_path,
        const std::shared_ptr<ArticArchive::OpenFileReporter>& _open_reporter)
        : client(_client), dir_handle(_dir_handle), archive_path(_archive_path),
          open_reporter(_open_reporter) {}
    ~ArticDirectoryBackend() override;

    u32 Read(const u32 count, Entry* entries) override;
    bool Close() override;

    bool IsSlow() override {
        return true;
    }

protected:
    ArticDirectoryBackend() = default;

private:
    std::shared_ptr<Network::ArticBase::Client> client;
    s32 dir_handle;
    Path archive_path;
    std::shared_ptr<ArticArchive::OpenFileReporter> open_reporter;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<DirectoryBackend>(*this);
        ar & dir_handle;
    }
    friend class boost::serialization::access;
};
} // namespace FileSys

BOOST_CLASS_EXPORT_KEY(FileSys::ArticArchive)
BOOST_CLASS_EXPORT_KEY(FileSys::ArticFileBackend)
BOOST_CLASS_EXPORT_KEY(FileSys::ArticDirectoryBackend)