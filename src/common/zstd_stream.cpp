// Copyright 2026 Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <fmt/format.h>
#include <zstd.h>
#include "common/file_util.h"
#include "common/zstd_stream.h"

namespace Common::Compression {

ZstdOutputStreamBuf::ZstdOutputStreamBuf(FileUtil::IOFile& file_, int compression_level)
    : file{file_} {
    cstream = ZSTD_createCStream();
    if (!cstream) {
        throw std::runtime_error("Could not create ZSTD compression stream");
    }
    compression_level = std::clamp(compression_level, ZSTD_minCLevel(), ZSTD_maxCLevel());
    ZSTD_CCtx_setParameter(cstream, ZSTD_c_compressionLevel, compression_level);
    // Memory content is mostly runs of identical bytes; checksums are cheap insurance against a
    // truncated or corrupted state file being deserialized into garbage.
    ZSTD_CCtx_setParameter(cstream, ZSTD_c_checksumFlag, 1);
    in_buf.resize(ZSTD_CStreamInSize());
    out_buf.resize(ZSTD_CStreamOutSize());
    setp(in_buf.data(), in_buf.data() + in_buf.size());
}

ZstdOutputStreamBuf::~ZstdOutputStreamBuf() {
    ZSTD_freeCStream(cstream);
}

void ZstdOutputStreamBuf::Finish() {
    FlushPending();
    ZSTD_inBuffer input{nullptr, 0, 0};
    std::size_t remaining = 0;
    do {
        ZSTD_outBuffer output{out_buf.data(), out_buf.size(), 0};
        remaining = ZSTD_compressStream2(cstream, &output, &input, ZSTD_e_end);
        if (ZSTD_isError(remaining)) {
            throw std::runtime_error(
                fmt::format("ZSTD_compressStream2 (end) failed: {}", ZSTD_getErrorName(remaining)));
        }
        WriteOut(output.pos);
    } while (remaining != 0);
}

ZstdOutputStreamBuf::int_type ZstdOutputStreamBuf::overflow(int_type ch) {
    FlushPending();
    if (!traits_type::eq_int_type(ch, traits_type::eof())) {
        *pptr() = traits_type::to_char_type(ch);
        pbump(1);
    }
    return traits_type::not_eof(ch);
}

std::streamsize ZstdOutputStreamBuf::xsputn(const char* s, std::streamsize n) {
    if (n <= 0) {
        return 0;
    }
    const auto count = static_cast<std::size_t>(n);
    // Large writes (e.g. whole FCRAM) are fed to zstd directly instead of being copied through
    // the staging buffer first.
    if (count >= in_buf.size()) {
        FlushPending();
        Compress(s, count);
        return n;
    }
    if (static_cast<std::size_t>(epptr() - pptr()) < count) {
        FlushPending();
    }
    std::memcpy(pptr(), s, count);
    pbump(static_cast<int>(count));
    return n;
}

int ZstdOutputStreamBuf::sync() {
    FlushPending();
    return 0;
}

void ZstdOutputStreamBuf::FlushPending() {
    const auto pending = static_cast<std::size_t>(pptr() - pbase());
    if (pending != 0) {
        Compress(pbase(), pending);
    }
    setp(in_buf.data(), in_buf.data() + in_buf.size());
}

void ZstdOutputStreamBuf::Compress(const char* data, std::size_t size) {
    ZSTD_inBuffer input{data, size, 0};
    while (input.pos < input.size) {
        ZSTD_outBuffer output{out_buf.data(), out_buf.size(), 0};
        const std::size_t ret = ZSTD_compressStream2(cstream, &output, &input, ZSTD_e_continue);
        if (ZSTD_isError(ret)) {
            throw std::runtime_error(
                fmt::format("ZSTD_compressStream2 failed: {}", ZSTD_getErrorName(ret)));
        }
        WriteOut(output.pos);
    }
}

void ZstdOutputStreamBuf::WriteOut(std::size_t size) {
    if (size != 0 && file.WriteBytes(out_buf.data(), size) != size) {
        throw std::runtime_error("Could not write to file " + file.Filename());
    }
}

ZstdInputStreamBuf::ZstdInputStreamBuf(FileUtil::IOFile& file_, u64 compressed_size)
    : file{file_}, remaining_compressed{compressed_size} {
    dstream = ZSTD_createDStream();
    if (!dstream) {
        throw std::runtime_error("Could not create ZSTD decompression stream");
    }
    ZSTD_initDStream(dstream);
    in_buf.resize(ZSTD_DStreamInSize());
    out_buf.resize(ZSTD_DStreamOutSize());
    setg(out_buf.data(), out_buf.data(), out_buf.data());
}

ZstdInputStreamBuf::~ZstdInputStreamBuf() {
    ZSTD_freeDStream(dstream);
}

ZstdInputStreamBuf::int_type ZstdInputStreamBuf::underflow() {
    if (gptr() < egptr()) {
        return traits_type::to_int_type(*gptr());
    }
    const std::size_t got = Decompress(out_buf.data(), out_buf.size());
    if (got == 0) {
        return traits_type::eof();
    }
    setg(out_buf.data(), out_buf.data(), out_buf.data() + got);
    return traits_type::to_int_type(*gptr());
}

std::streamsize ZstdInputStreamBuf::xsgetn(char* s, std::streamsize n) {
    if (n <= 0) {
        return 0;
    }
    std::size_t total = 0;
    const auto want_total = static_cast<std::size_t>(n);
    while (total < want_total) {
        const auto buffered = static_cast<std::size_t>(egptr() - gptr());
        if (buffered != 0) {
            const std::size_t take = std::min(buffered, want_total - total);
            std::memcpy(s + total, gptr(), take);
            gbump(static_cast<int>(take));
            total += take;
            continue;
        }
        const std::size_t want = want_total - total;
        if (want >= out_buf.size()) {
            // Large reads (e.g. whole FCRAM) are decompressed straight into the destination.
            const std::size_t got = Decompress(s + total, want);
            if (got == 0) {
                break;
            }
            total += got;
        } else if (traits_type::eq_int_type(underflow(), traits_type::eof())) {
            break;
        }
    }
    return static_cast<std::streamsize>(total);
}

std::size_t ZstdInputStreamBuf::Decompress(char* dst, std::size_t size) {
    ZSTD_outBuffer output{dst, size, 0};
    while (output.pos < output.size && !frame_finished) {
        if (in_pos == in_size) {
            if (remaining_compressed == 0) {
                break;
            }
            const auto to_read = static_cast<std::size_t>(
                std::min<u64>(static_cast<u64>(in_buf.size()), remaining_compressed));
            const std::size_t read = file.ReadBytes(in_buf.data(), to_read);
            if (read == 0) {
                break;
            }
            remaining_compressed -= read;
            in_pos = 0;
            in_size = read;
        }
        ZSTD_inBuffer input{in_buf.data(), in_size, in_pos};
        const std::size_t ret = ZSTD_decompressStream(dstream, &output, &input);
        in_pos = input.pos;
        if (ZSTD_isError(ret)) {
            throw std::runtime_error(
                fmt::format("ZSTD_decompressStream failed: {}", ZSTD_getErrorName(ret)));
        }
        if (ret == 0) {
            frame_finished = true;
        }
    }
    return output.pos;
}

} // namespace Common::Compression
