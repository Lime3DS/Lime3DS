// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <boost/crc.hpp>
#include "common/logging/log.h"
#include "core/file_sys/patch.h"

namespace FileSys::Patch {

bool ApplyIpsPatch(const std::vector<u8>& ips, std::vector<u8>& buffer) {
    std::size_t cursor = 5;
    std::size_t patch_length = ips.size() - 3;
    std::string ips_header(ips.begin(), ips.begin() + 5);

    if (ips_header != "PATCH") {
        LOG_INFO(Service_FS, "Attempted to load invalid IPS");
        return false;
    }

    while (cursor < patch_length) {
        std::string eof_check(ips.begin() + cursor, ips.begin() + cursor + 3);

        if (eof_check == "EOF")
            return false;

        std::size_t offset = ips[cursor] << 16 | ips[cursor + 1] << 8 | ips[cursor + 2];
        std::size_t length = ips[cursor + 3] << 8 | ips[cursor + 4];

        // check for an rle record
        if (length == 0) {
            length = ips[cursor + 5] << 8 | ips[cursor + 6];

            if (buffer.size() < offset + length)
                return false;

            for (u32 i = 0; i < length; ++i)
                buffer[offset + i] = ips[cursor + 7];

            cursor += 8;

            continue;
        }

        if (buffer.size() < offset + length)
            return false;

        std::memcpy(&buffer[offset], &ips[cursor + 5], length);
        cursor += length + 5;
    }
    return true;
}

namespace Bps {

// The BPS format uses variable length encoding for all integers.
// Realistically uint32s are more than enough for code patching.
using Number = u32;

constexpr std::size_t MagicSize = 4;
constexpr std::size_t FooterSize = 12;

// The BPS format uses CRC32 checksums.
static u32 crc32(const u8* data, std::size_t size) {
    boost::crc_32_type result;
    result.process_bytes(data, size);
    return result.checksum();
}

// Utility class to make keeping track of offsets and bound checks less error prone.
template <typename T>
class Stream {
public:
    Stream(T* ptr, std::size_t size) : m_ptr{ptr}, m_size{size} {}

    bool Read(void* buffer, std::size_t length) {
        if (m_offset + length > m_size)
            return false;
        std::memcpy(buffer, m_ptr + m_offset, length);
        m_offset += length;
        return true;
    }

    template <typename OtherType>
    bool CopyFrom(Stream<OtherType>& other, std::size_t length) {
        if (m_offset + length > m_size)
            return false;
        if (!other.Read(m_ptr + m_offset, length))
            return false;
        m_offset += length;
        return true;
    }

    template <typename ValueType>
    std::optional<ValueType> Read() {
        static_assert(std::is_trivial_v<ValueType>);
        ValueType val{};
        if (!Read(&val, sizeof(val)))
            return std::nullopt;
        return val;
    }

    Number ReadNumber() {
        Number data = 0, shift = 1;
        std::optional<u8> x;
        while ((x = Read<u8>())) {
            data += (*x & 0x7f) * shift;
            if (*x & 0x80)
                break;
            shift <<= 7;
            data += shift;
        }
        return data;
    }

    auto data() const {
        return m_ptr;
    }

    std::size_t size() const {
        return m_size;
    }

    std::size_t Tell() const {
        return m_offset;
    }

    bool Seek(std::size_t offset) {
        if (offset > m_size)
            return false;
        m_offset = offset;
        return true;
    }

private:
    T* m_ptr = nullptr;
    std::size_t m_size = 0;
    std::size_t m_offset = 0;
};

class PatchApplier {
public:
    PatchApplier(Stream<const u8> source, Stream<u8> target, Stream<const u8> patch)
        : m_source{source}, m_target{target}, m_patch{patch} {}

    bool Apply() {
        const auto magic = *m_patch.Read<std::array<char, MagicSize>>();
        if (std::string_view(magic.data(), magic.size()) != "BPS1") {
            LOG_ERROR(Service_FS, "Invalid BPS magic");
            return false;
        }

        const Bps::Number source_size = m_patch.ReadNumber();
        const Bps::Number target_size = m_patch.ReadNumber();
        const Bps::Number metadata_size = m_patch.ReadNumber();
        if (source_size > m_source.size() || target_size > m_target.size() || metadata_size != 0) {
            LOG_ERROR(Service_FS, "Invalid sizes");
            return false;
        }

        const std::size_t command_start_offset = m_patch.Tell();
        const std::size_t command_end_offset = m_patch.size() - FooterSize;
        m_patch.Seek(command_end_offset);
        const u32 source_crc32 = *m_patch.Read<u32>();
        const u32 target_crc32 = *m_patch.Read<u32>();
        m_patch.Seek(command_start_offset);

        if (crc32(m_source.data(), source_size) != source_crc32) {
            LOG_ERROR(Service_FS, "Unexpected source hash");
            return false;
        }

        // Process all patch commands.
        std::memset(m_target.data(), 0, m_target.size());
        while (m_patch.Tell() < command_end_offset) {
            if (!HandleCommand())
                return false;
        }

        if (crc32(m_target.data(), target_size) != target_crc32) {
            LOG_ERROR(Service_FS, "Unexpected target hash");
            return false;
        }

        return true;
    }

private:
    bool HandleCommand() {
        const std::size_t offset = m_patch.Tell();
        const Number data = m_patch.ReadNumber();
        const Number command = data & 3;
        const Number length = (data >> 2) + 1;

        const bool ok = [&] {
            switch (command) {
            case 0:
                return SourceRead(length);
            case 1:
                return TargetRead(length);
            case 2:
                return SourceCopy(length);
            case 3:
                return TargetCopy(length);
            default:
                return false;
            }
        }();
        if (!ok)
            LOG_ERROR(Service_FS, "Failed to process command {} at 0x{:x}", command, offset);
        return ok;
    }

    bool SourceRead(Number length) {
        return m_source.Seek(m_target.Tell()) && m_target.CopyFrom(m_source, length);
    }

    bool TargetRead(Number length) {
        return m_target.CopyFrom(m_patch, length);
    }

    bool SourceCopy(Number length) {
        const Number data = m_patch.ReadNumber();
        m_source_relative_offset += (data & 1 ? -1 : +1) * int(data >> 1);
        if (!m_source.Seek(m_source_relative_offset) || !m_target.CopyFrom(m_source, length))
            return false;
        m_source_relative_offset += length;
        return true;
    }

    bool TargetCopy(Number length) {
        const Number data = m_patch.ReadNumber();
        m_target_relative_offset += (data & 1 ? -1 : +1) * int(data >> 1);
        if (m_target.Tell() + length > m_target.size())
            return false;
        if (m_target_relative_offset + length > m_target.size())
            return false;
        // Byte by byte copy.
        for (std::size_t i = 0; i < length; ++i)
            m_target.data()[m_target.Tell() + i] = m_target.data()[m_target_relative_offset++];
        m_target.Seek(m_target.Tell() + length);
        return true;
    }

    std::size_t m_source_relative_offset = 0;
    std::size_t m_target_relative_offset = 0;
    Stream<const u8> m_source;
    Stream<u8> m_target;
    Stream<const u8> m_patch;
};

} // namespace Bps

bool ApplyBpsPatch(const std::vector<u8>& patch, std::vector<u8>& buffer) {
    Bps::Stream patch_stream{patch.data(), patch.size()};

    // Move the offset past the file format marker (i.e. "BPS1")
    patch_stream.Seek(Bps::MagicSize);

    const Bps::Number source_size = patch_stream.ReadNumber();
    const Bps::Number target_size = patch_stream.ReadNumber();

    if (target_size > source_size) {
        LOG_INFO(Service_FS, "Resizing target to {}", target_size);
        buffer.resize(target_size);
    }

    patch_stream.Seek(0);

    const std::vector<u8> source = buffer;
    Bps::Stream source_stream{source.data(), source.size()};
    Bps::Stream target_stream{buffer.data(), buffer.size()};
    Bps::PatchApplier applier{source_stream, target_stream, patch_stream};
    return applier.Apply();
}

} // namespace FileSys::Patch
