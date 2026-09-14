// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <streambuf>
#include <vector>
#include "common/common_types.h"

struct ZSTD_CCtx_s;
struct ZSTD_DCtx_s;

namespace FileUtil {
class IOFile;
}

namespace Common::Compression {

/**
 * std::streambuf that Zstandard-compresses everything written to it and streams the result
 * straight into an IOFile. Peak memory usage is bounded by the small staging buffers plus the
 * zstd context, no matter how much data passes through. Call Finish() before the file is closed.
 */
class ZstdOutputStreamBuf final : public std::streambuf {
public:
    explicit ZstdOutputStreamBuf(FileUtil::IOFile& file, int compression_level);
    ~ZstdOutputStreamBuf() override;

    ZstdOutputStreamBuf(const ZstdOutputStreamBuf&) = delete;
    ZstdOutputStreamBuf& operator=(const ZstdOutputStreamBuf&) = delete;

    /// Flushes all pending input and terminates the zstd frame. Throws std::runtime_error.
    void Finish();

protected:
    int_type overflow(int_type ch) override;
    std::streamsize xsputn(const char* s, std::streamsize n) override;
    int sync() override;

private:
    void FlushPending();
    void Compress(const char* data, std::size_t size);
    void WriteOut(std::size_t size);

    FileUtil::IOFile& file;
    ZSTD_CCtx_s* cstream{};
    std::vector<char> in_buf;
    std::vector<char> out_buf;
};

/**
 * std::streambuf that reads a Zstandard frame from an IOFile (starting at the file's current
 * position, `compressed_size` bytes long) and decompresses it on the fly.
 */
class ZstdInputStreamBuf final : public std::streambuf {
public:
    ZstdInputStreamBuf(FileUtil::IOFile& file, u64 compressed_size);
    ~ZstdInputStreamBuf() override;

    ZstdInputStreamBuf(const ZstdInputStreamBuf&) = delete;
    ZstdInputStreamBuf& operator=(const ZstdInputStreamBuf&) = delete;

protected:
    int_type underflow() override;
    std::streamsize xsgetn(char* s, std::streamsize n) override;

private:
    /// Decompresses up to `size` bytes into `dst`. Returns 0 once the stream is exhausted.
    std::size_t Decompress(char* dst, std::size_t size);

    FileUtil::IOFile& file;
    ZSTD_DCtx_s* dstream{};
    std::vector<char> in_buf;
    std::vector<char> out_buf;
    std::size_t in_pos{};
    std::size_t in_size{};
    u64 remaining_compressed{};
    bool frame_finished{};
};

} // namespace Common::Compression
