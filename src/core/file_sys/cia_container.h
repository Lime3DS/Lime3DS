// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/file_sys/ticket.h"
#include "core/file_sys/title_metadata.h"

namespace Loader {
enum class ResultStatus;
}

namespace FileSys {

class FileBackend;

constexpr std::size_t CIA_CONTENT_MAX_COUNT = 0x10000;
constexpr std::size_t CIA_CONTENT_BITS_SIZE = (CIA_CONTENT_MAX_COUNT / 8);
constexpr std::size_t CIA_HEADER_SIZE = 0x2020;
constexpr std::size_t CIA_DEPENDENCY_SIZE = 0x300;
constexpr std::size_t CIA_METADATA_SIZE = 0x400;
constexpr u32 CIA_SECTION_ALIGNMENT = 0x40;

/**
 * Helper which implements an interface to read and write CTR Installable Archive (CIA) files.
 * Data can either be loaded from a FileBackend, a string path, or from a data array. Data can
 * also be partially loaded for CIAs which are downloading/streamed in and need some metadata
 * read out.
 */
class CIAContainer {
public:
    // Load whole CIAs outright
    Loader::ResultStatus Load(const FileBackend& backend);
    Loader::ResultStatus Load(const std::string& filepath);
    Loader::ResultStatus Load(std::span<const u8> header_data);

    // Load parts of CIAs (for CIAs streamed in)
    Loader::ResultStatus LoadHeader(std::span<const u8> header_data, std::size_t offset = 0);
    Loader::ResultStatus LoadTicket(std::span<const u8> ticket_data, std::size_t offset = 0);
    Loader::ResultStatus LoadTitleMetadata(std::span<const u8> tmd_data, std::size_t offset = 0);
    Loader::ResultStatus LoadMetadata(std::span<const u8> meta_data, std::size_t offset = 0);

    const Ticket& GetTicket() const;
    const TitleMetadata& GetTitleMetadata() const;
    std::array<u64, 0x30>& GetDependencies();
    u32 GetCoreVersion() const;

    u64 GetCertificateOffset() const;
    u64 GetTicketOffset() const;
    u64 GetTitleMetadataOffset() const;
    u64 GetMetadataOffset() const;
    u64 GetContentOffset(std::size_t index = 0) const;

    u32 GetCertificateSize() const;
    u32 GetTicketSize() const;
    u32 GetTitleMetadataSize() const;
    u32 GetMetadataSize() const;
    u64 GetTotalContentSize() const;
    u64 GetContentSize(std::size_t index = 0) const;

    void Print() const;

    struct Header {
        u32_le header_size;
        u16_le type;
        u16_le version;
        u32_le cert_size;
        u32_le tik_size;
        u32_le tmd_size;
        u32_le meta_size;
        u64_le content_size;
        std::array<u8, CIA_CONTENT_BITS_SIZE> content_present;

        bool IsContentPresent(std::size_t index) const {
            // The content_present is a bit array which defines which content in the TMD
            // is included in the CIA, so check the bit for this index and add if set.
            // The bits in the content index are arranged w/ index 0 as the MSB, 7 as the LSB, etc.
            return (content_present[index >> 3] & (0x80 >> (index & 7))) != 0;
        }
        void SetContentPresent(u16 index) {
            content_present[index >> 3] |= (0x80 >> (index & 7));
        }
    };

    static_assert(sizeof(Header) == CIA_HEADER_SIZE, "CIA Header structure size is wrong");

private:
    struct Metadata {
        std::array<u64_le, 0x30> dependencies;
        std::array<u8, 0x180> reserved;
        u32_le core_version;
        std::array<u8, 0xfc> reserved_2;
    };

    static_assert(sizeof(Metadata) == CIA_METADATA_SIZE, "CIA Metadata structure size is wrong");

    Header cia_header;
    Metadata cia_metadata;
    Ticket cia_ticket;
    TitleMetadata cia_tmd;
};

} // namespace FileSys
