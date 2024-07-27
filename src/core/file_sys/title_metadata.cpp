// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cryptopp/sha.h>
#include "common/alignment.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/cia_common.h"
#include "core/file_sys/title_metadata.h"
#include "core/loader/loader.h"

namespace FileSys {

Loader::ResultStatus TitleMetadata::Load(const std::string& file_path) {
    FileUtil::IOFile file(file_path, "rb");
    if (!file.IsOpen())
        return Loader::ResultStatus::Error;

    std::vector<u8> file_data(file.GetSize());

    if (!file.ReadBytes(file_data.data(), file.GetSize()))
        return Loader::ResultStatus::Error;

    Loader::ResultStatus result = Load(file_data);
    if (result != Loader::ResultStatus::Success)
        LOG_ERROR(Service_FS, "Failed to load TMD from file {}!", file_path);

    return result;
}

Loader::ResultStatus TitleMetadata::Load(std::span<const u8> file_data, std::size_t offset) {
    std::size_t total_size = static_cast<std::size_t>(file_data.size() - offset);
    if (total_size < sizeof(u32_be)) {
        return Loader::ResultStatus::Error;
    }

    std::memcpy(&signature_type, &file_data[offset], sizeof(u32_be));

    // Signature lengths are variable, and the body follows the signature
    u32 signature_size = GetSignatureSize(signature_type);
    if (signature_size == 0) {
        return Loader::ResultStatus::Error;
    }

    // The TMD body start position is rounded to the nearest 0x40 after the signature
    std::size_t body_start = Common::AlignUp(signature_size + sizeof(u32), 0x40);
    std::size_t body_end = body_start + sizeof(Body);

    if (total_size < body_end) {
        return Loader::ResultStatus::Error;
    }

    // Read signature + TMD body, then load the amount of ContentChunks specified
    tmd_signature.resize(signature_size);
    std::memcpy(tmd_signature.data(), &file_data[offset + sizeof(u32_be)], signature_size);
    std::memcpy(&tmd_body, &file_data[offset + body_start], sizeof(TitleMetadata::Body));

    std::size_t expected_size =
        body_start + sizeof(Body) + static_cast<u16>(tmd_body.content_count) * sizeof(ContentChunk);
    if (total_size < expected_size) {
        LOG_ERROR(Service_FS, "Malformed TMD, expected size 0x{:x}, got 0x{:x}!", expected_size,
                  total_size);
        return Loader::ResultStatus::ErrorInvalidFormat;
    }

    for (u16 i = 0; i < tmd_body.content_count; i++) {
        ContentChunk chunk;

        std::memcpy(&chunk, &file_data[offset + body_end + (i * sizeof(ContentChunk))],
                    sizeof(ContentChunk));
        tmd_chunks.push_back(chunk);
    }

    return Loader::ResultStatus::Success;
}

Loader::ResultStatus TitleMetadata::Save(const std::string& file_path) {
    FileUtil::IOFile file(file_path, "wb");
    if (!file.IsOpen())
        return Loader::ResultStatus::Error;

    if (!file.WriteBytes(&signature_type, sizeof(u32_be)))
        return Loader::ResultStatus::Error;

    // Signature lengths are variable, and the body follows the signature
    u32 signature_size = GetSignatureSize(signature_type);
    if (signature_size == 0) {
        return Loader::ResultStatus::Error;
    }

    if (!file.WriteBytes(tmd_signature.data(), signature_size))
        return Loader::ResultStatus::Error;

    // The TMD body start position is rounded to the nearest 0x40 after the signature
    std::size_t body_start = Common::AlignUp(signature_size + sizeof(u32), 0x40);
    file.Seek(body_start, SEEK_SET);

    // Update our TMD body values and hashes
    tmd_body.content_count = static_cast<u16>(tmd_chunks.size());

    // TODO(shinyquagsire23): Do TMDs with more than one contentinfo exist?
    // For now we'll just adjust the first index to hold all content chunks
    // and ensure that no further content info data exists.
    tmd_body.contentinfo = {};
    tmd_body.contentinfo[0].index = 0;
    tmd_body.contentinfo[0].command_count = static_cast<u16>(tmd_chunks.size());

    CryptoPP::SHA256 chunk_hash;
    for (u16 i = 0; i < tmd_body.content_count; i++) {
        chunk_hash.Update(reinterpret_cast<u8*>(&tmd_chunks[i]), sizeof(ContentChunk));
    }
    chunk_hash.Final(tmd_body.contentinfo[0].hash.data());

    CryptoPP::SHA256 contentinfo_hash;
    for (std::size_t i = 0; i < tmd_body.contentinfo.size(); i++) {
        chunk_hash.Update(reinterpret_cast<u8*>(&tmd_body.contentinfo[i]), sizeof(ContentInfo));
    }
    chunk_hash.Final(tmd_body.contentinfo_hash.data());

    // Write our TMD body, then write each of our ContentChunks
    if (file.WriteBytes(&tmd_body, sizeof(TitleMetadata::Body)) != sizeof(TitleMetadata::Body))
        return Loader::ResultStatus::Error;

    for (u16 i = 0; i < tmd_body.content_count; i++) {
        ContentChunk chunk = tmd_chunks[i];
        if (file.WriteBytes(&chunk, sizeof(ContentChunk)) != sizeof(ContentChunk))
            return Loader::ResultStatus::Error;
    }

    return Loader::ResultStatus::Success;
}

u64 TitleMetadata::GetTitleID() const {
    return tmd_body.title_id;
}

u32 TitleMetadata::GetTitleType() const {
    return tmd_body.title_type;
}

u16 TitleMetadata::GetTitleVersion() const {
    return tmd_body.title_version;
}

u64 TitleMetadata::GetSystemVersion() const {
    return tmd_body.system_version;
}

size_t TitleMetadata::GetContentCount() const {
    return tmd_chunks.size();
}

u32 TitleMetadata::GetBootContentID() const {
    return tmd_chunks[TMDContentIndex::Main].id;
}

u32 TitleMetadata::GetManualContentID() const {
    return tmd_chunks[TMDContentIndex::Manual].id;
}

u32 TitleMetadata::GetDLPContentID() const {
    return tmd_chunks[TMDContentIndex::DLP].id;
}

u32 TitleMetadata::GetContentIDByIndex(std::size_t index) const {
    return tmd_chunks[index].id;
}

u16 TitleMetadata::GetContentTypeByIndex(std::size_t index) const {
    return tmd_chunks[index].type;
}

u64 TitleMetadata::GetContentSizeByIndex(std::size_t index) const {
    return tmd_chunks[index].size;
}

std::array<u8, 16> TitleMetadata::GetContentCTRByIndex(std::size_t index) const {
    std::array<u8, 16> ctr{};
    std::memcpy(ctr.data(), &tmd_chunks[index].index, sizeof(u16));
    return ctr;
}

bool TitleMetadata::HasEncryptedContent() const {
    return std::any_of(tmd_chunks.begin(), tmd_chunks.end(), [](auto& chunk) {
        return (static_cast<u16>(chunk.type) & FileSys::TMDContentTypeFlag::Encrypted) != 0;
    });
}

void TitleMetadata::SetTitleID(u64 title_id) {
    tmd_body.title_id = title_id;
}

void TitleMetadata::SetTitleType(u32 type) {
    tmd_body.title_type = type;
}

void TitleMetadata::SetTitleVersion(u16 version) {
    tmd_body.title_version = version;
}

void TitleMetadata::SetSystemVersion(u64 version) {
    tmd_body.system_version = version;
}

void TitleMetadata::AddContentChunk(const ContentChunk& chunk) {
    tmd_chunks.push_back(chunk);
}

void TitleMetadata::Print() const {
    LOG_DEBUG(Service_FS, "{} chunks", static_cast<u32>(tmd_body.content_count));

    // Content info describes ranges of content chunks
    LOG_DEBUG(Service_FS, "Content info:");
    for (std::size_t i = 0; i < tmd_body.contentinfo.size(); i++) {
        if (tmd_body.contentinfo[i].command_count == 0)
            break;

        LOG_DEBUG(Service_FS, "    Index {:04X}, Command Count {:04X}",
                  static_cast<u32>(tmd_body.contentinfo[i].index),
                  static_cast<u32>(tmd_body.contentinfo[i].command_count));
    }

    // For each content info, print their content chunk range
    for (std::size_t i = 0; i < tmd_body.contentinfo.size(); i++) {
        u16 index = static_cast<u16>(tmd_body.contentinfo[i].index);
        u16 count = static_cast<u16>(tmd_body.contentinfo[i].command_count);

        if (count == 0)
            continue;

        LOG_DEBUG(Service_FS, "Content chunks for content info index {}:", i);
        for (u16 j = index; j < index + count; j++) {
            // Don't attempt to print content we don't have
            if (j > tmd_body.content_count)
                break;

            const ContentChunk& chunk = tmd_chunks[j];
            LOG_DEBUG(Service_FS, "    ID {:08X}, Index {:04X}, Type {:04x}, Size {:016X}",
                      static_cast<u32>(chunk.id), static_cast<u32>(chunk.index),
                      static_cast<u32>(chunk.type), static_cast<u64>(chunk.size));
        }
    }
}
} // namespace FileSys
