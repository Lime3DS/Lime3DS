// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <span>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/file_sys/layered_fs.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/patch.h"
#include "core/file_sys/seed_db.h"
#include "core/hw/aes/key.h"
#include "core/loader/loader.h"

namespace FileSys {

static const int kMaxSections = 8;   ///< Maximum number of sections (files) in an ExeFs
static const int kBlockSize = 0x200; ///< Size of ExeFS blocks (in bytes)

u64 GetModId(u64 program_id) {
    constexpr u64 UPDATE_MASK = 0x0000000e'00000000;
    if ((program_id & 0x000000ff'00000000) == UPDATE_MASK) { // Apply the mods to updates
        return program_id & ~UPDATE_MASK;
    }
    return program_id;
}

/**
 * Get the decompressed size of an LZSS compressed ExeFS file
 * @param buffer Buffer of compressed file
 * @param size Size of compressed buffer
 * @return Size of decompressed buffer
 */
static std::size_t LZSS_GetDecompressedSize(std::span<const u8> buffer) {
    u32 offset_size;
    std::memcpy(&offset_size, buffer.data() + buffer.size() - sizeof(u32), sizeof(u32));
    return offset_size + buffer.size();
}

/**
 * Decompress ExeFS file (compressed with LZSS)
 * @param compressed Compressed buffer
 * @param compressed_size Size of compressed buffer
 * @param decompressed Decompressed buffer
 * @param decompressed_size Size of decompressed buffer
 * @return True on success, otherwise false
 */
static bool LZSS_Decompress(std::span<const u8> compressed, std::span<u8> decompressed) {
    const u8* footer = compressed.data() + compressed.size() - 8;

    u32 buffer_top_and_bottom;
    std::memcpy(&buffer_top_and_bottom, footer, sizeof(u32));

    std::size_t out = decompressed.size();
    std::size_t index = compressed.size() - ((buffer_top_and_bottom >> 24) & 0xFF);
    std::size_t stop_index = compressed.size() - (buffer_top_and_bottom & 0xFFFFFF);

    std::memset(decompressed.data(), 0, decompressed.size());
    std::memcpy(decompressed.data(), compressed.data(), compressed.size());

    while (index > stop_index) {
        u8 control = compressed[--index];

        for (unsigned i = 0; i < 8; i++) {
            if (index <= stop_index)
                break;
            if (index <= 0)
                break;
            if (out <= 0)
                break;

            if (control & 0x80) {
                // Check if compression is out of bounds
                if (index < 2)
                    return false;
                index -= 2;

                u32 segment_offset = compressed[index] | (compressed[index + 1] << 8);
                u32 segment_size = ((segment_offset >> 12) & 15) + 3;
                segment_offset &= 0x0FFF;
                segment_offset += 2;

                // Check if compression is out of bounds
                if (out < segment_size)
                    return false;

                for (unsigned j = 0; j < segment_size; j++) {
                    // Check if compression is out of bounds
                    if (out + segment_offset >= decompressed.size())
                        return false;

                    u8 data = decompressed[out + segment_offset];
                    decompressed[--out] = data;
                }
            } else {
                // Check if compression is out of bounds
                if (out < 1)
                    return false;
                decompressed[--out] = compressed[--index];
            }
            control <<= 1;
        }
    }
    return true;
}

NCCHContainer::NCCHContainer(const std::string& filepath, u32 ncch_offset, u32 partition)
    : ncch_offset(ncch_offset), partition(partition), filepath(filepath) {
    file = FileUtil::IOFile(filepath, "rb");
}

Loader::ResultStatus NCCHContainer::OpenFile(const std::string& filepath_, u32 ncch_offset_,
                                             u32 partition_) {
    filepath = filepath_;
    ncch_offset = ncch_offset_;
    partition = partition_;
    file = FileUtil::IOFile(filepath_, "rb");

    if (!file.IsOpen()) {
        LOG_WARNING(Service_FS, "Failed to open {}", filepath);
        return Loader::ResultStatus::Error;
    }

    LOG_DEBUG(Service_FS, "Opened {}", filepath);
    return Loader::ResultStatus::Success;
}

Loader::ResultStatus NCCHContainer::LoadHeader() {
    if (has_header) {
        return Loader::ResultStatus::Success;
    }
    if (!file.IsOpen()) {
        return Loader::ResultStatus::Error;
    }

    // Reset read pointer in case this file has been read before.
    file.Seek(ncch_offset, SEEK_SET);

    if (file.ReadBytes(&ncch_header, sizeof(NCCH_Header)) != sizeof(NCCH_Header)) {
        return Loader::ResultStatus::Error;
    }

    // Skip NCSD header and load first NCCH (NCSD is just a container of NCCH files)...
    if (Loader::MakeMagic('N', 'C', 'S', 'D') == ncch_header.magic) {
        NCSD_Header ncsd_header;
        file.Seek(ncch_offset, SEEK_SET);
        file.ReadBytes(&ncsd_header, sizeof(NCSD_Header));
        ASSERT(Loader::MakeMagic('N', 'C', 'S', 'D') == ncsd_header.magic);
        ASSERT(partition < 8);
        ncch_offset = ncsd_header.partitions[partition].offset * kBlockSize;
        LOG_ERROR(Service_FS, "{}", ncch_offset);
        file.Seek(ncch_offset, SEEK_SET);
        file.ReadBytes(&ncch_header, sizeof(NCCH_Header));
    }

    // Verify we are loading the correct file type...
    if (Loader::MakeMagic('N', 'C', 'C', 'H') != ncch_header.magic) {
        return Loader::ResultStatus::ErrorInvalidFormat;
    }

    has_header = true;
    return Loader::ResultStatus::Success;
}

Loader::ResultStatus NCCHContainer::Load() {
    if (is_loaded)
        return Loader::ResultStatus::Success;

    if (file.IsOpen()) {
        // Reset read pointer in case this file has been read before.
        file.Seek(ncch_offset, SEEK_SET);

        if (file.ReadBytes(&ncch_header, sizeof(NCCH_Header)) != sizeof(NCCH_Header))
            return Loader::ResultStatus::Error;

        // Skip NCSD header and load first NCCH (NCSD is just a container of NCCH files)...
        if (Loader::MakeMagic('N', 'C', 'S', 'D') == ncch_header.magic) {
            NCSD_Header ncsd_header;
            file.Seek(ncch_offset, SEEK_SET);
            file.ReadBytes(&ncsd_header, sizeof(NCSD_Header));
            ASSERT(Loader::MakeMagic('N', 'C', 'S', 'D') == ncsd_header.magic);
            ASSERT(partition < 8);
            ncch_offset = ncsd_header.partitions[partition].offset * kBlockSize;
            file.Seek(ncch_offset, SEEK_SET);
            file.ReadBytes(&ncch_header, sizeof(NCCH_Header));
        }

        // Verify we are loading the correct file type...
        if (Loader::MakeMagic('N', 'C', 'C', 'H') != ncch_header.magic)
            return Loader::ResultStatus::ErrorInvalidFormat;

        has_header = true;
        bool failed_to_decrypt = false;
        if (!ncch_header.no_crypto) {
            is_encrypted = true;

            // Find primary and secondary keys
            if (ncch_header.fixed_key) {
                LOG_DEBUG(Service_FS, "Fixed-key crypto");
                primary_key.fill(0);
                secondary_key.fill(0);
            } else {
                using namespace HW::AES;
                InitKeys();
                std::array<u8, 16> key_y_primary, key_y_secondary;

                std::copy(ncch_header.signature, ncch_header.signature + key_y_primary.size(),
                          key_y_primary.begin());

                if (!ncch_header.seed_crypto) {
                    key_y_secondary = key_y_primary;
                } else {
                    auto opt{FileSys::GetSeed(ncch_header.program_id)};
                    if (!opt.has_value()) {
                        LOG_ERROR(Service_FS, "Seed for program {:016X} not found",
                                  ncch_header.program_id);
                        failed_to_decrypt = true;
                    } else {
                        auto seed{*opt};
                        std::array<u8, 32> input;
                        std::memcpy(input.data(), key_y_primary.data(), key_y_primary.size());
                        std::memcpy(input.data() + key_y_primary.size(), seed.data(), seed.size());
                        CryptoPP::SHA256 sha;
                        std::array<u8, CryptoPP::SHA256::DIGESTSIZE> hash;
                        sha.CalculateDigest(hash.data(), input.data(), input.size());
                        std::memcpy(key_y_secondary.data(), hash.data(), key_y_secondary.size());
                    }
                }

                SetKeyY(KeySlotID::NCCHSecure1, key_y_primary);
                if (!IsNormalKeyAvailable(KeySlotID::NCCHSecure1)) {
                    LOG_ERROR(Service_FS, "Secure1 KeyX missing");
                    failed_to_decrypt = true;
                }
                primary_key = GetNormalKey(KeySlotID::NCCHSecure1);

                switch (ncch_header.secondary_key_slot) {
                case 0:
                    LOG_DEBUG(Service_FS, "Secure1 crypto");
                    SetKeyY(KeySlotID::NCCHSecure1, key_y_secondary);
                    if (!IsNormalKeyAvailable(KeySlotID::NCCHSecure1)) {
                        LOG_ERROR(Service_FS, "Secure1 KeyX missing");
                        failed_to_decrypt = true;
                    }
                    secondary_key = GetNormalKey(KeySlotID::NCCHSecure1);
                    break;
                case 1:
                    LOG_DEBUG(Service_FS, "Secure2 crypto");
                    SetKeyY(KeySlotID::NCCHSecure2, key_y_secondary);
                    if (!IsNormalKeyAvailable(KeySlotID::NCCHSecure2)) {
                        LOG_ERROR(Service_FS, "Secure2 KeyX missing");
                        failed_to_decrypt = true;
                    }
                    secondary_key = GetNormalKey(KeySlotID::NCCHSecure2);
                    break;
                case 10:
                    LOG_DEBUG(Service_FS, "Secure3 crypto");
                    SetKeyY(KeySlotID::NCCHSecure3, key_y_secondary);
                    if (!IsNormalKeyAvailable(KeySlotID::NCCHSecure3)) {
                        LOG_ERROR(Service_FS, "Secure3 KeyX missing");
                        failed_to_decrypt = true;
                    }
                    secondary_key = GetNormalKey(KeySlotID::NCCHSecure3);
                    break;
                case 11:
                    LOG_DEBUG(Service_FS, "Secure4 crypto");
                    SetKeyY(KeySlotID::NCCHSecure4, key_y_secondary);
                    if (!IsNormalKeyAvailable(KeySlotID::NCCHSecure4)) {
                        LOG_ERROR(Service_FS, "Secure4 KeyX missing");
                        failed_to_decrypt = true;
                    }
                    secondary_key = GetNormalKey(KeySlotID::NCCHSecure4);
                    break;
                }
            }

            // Find CTR for each section
            // Written with reference to
            // https://github.com/d0k3/GodMode9/blob/99af6a73be48fa7872649aaa7456136da0df7938/arm9/source/game/ncch.c#L34-L52
            if (ncch_header.version == 0 || ncch_header.version == 2) {
                LOG_DEBUG(Loader, "NCCH version 0/2");
                // In this version, CTR for each section is a magic number prefixed by partition ID
                // (reverse order)
                std::reverse_copy(ncch_header.partition_id, ncch_header.partition_id + 8,
                                  exheader_ctr.begin());
                exefs_ctr = romfs_ctr = exheader_ctr;
                exheader_ctr[8] = 1;
                exefs_ctr[8] = 2;
                romfs_ctr[8] = 3;
            } else if (ncch_header.version == 1) {
                LOG_DEBUG(Loader, "NCCH version 1");
                // In this version, CTR for each section is the section offset prefixed by partition
                // ID, as if the entire NCCH image is encrypted using a single CTR stream.
                std::copy(ncch_header.partition_id, ncch_header.partition_id + 8,
                          exheader_ctr.begin());
                exefs_ctr = romfs_ctr = exheader_ctr;
                auto u32ToBEArray = [](u32 value) -> std::array<u8, 4> {
                    return std::array<u8, 4>{
                        static_cast<u8>(value >> 24),
                        static_cast<u8>((value >> 16) & 0xFF),
                        static_cast<u8>((value >> 8) & 0xFF),
                        static_cast<u8>(value & 0xFF),
                    };
                };
                auto offset_exheader = u32ToBEArray(0x200); // exheader offset
                auto offset_exefs = u32ToBEArray(ncch_header.exefs_offset * kBlockSize);
                auto offset_romfs = u32ToBEArray(ncch_header.romfs_offset * kBlockSize);
                std::copy(offset_exheader.begin(), offset_exheader.end(),
                          exheader_ctr.begin() + 12);
                std::copy(offset_exefs.begin(), offset_exefs.end(), exefs_ctr.begin() + 12);
                std::copy(offset_romfs.begin(), offset_romfs.end(), romfs_ctr.begin() + 12);
            } else {
                LOG_ERROR(Service_FS, "Unknown NCCH version {}", ncch_header.version);
                failed_to_decrypt = true;
            }
        } else {
            LOG_DEBUG(Service_FS, "No crypto");
            is_encrypted = false;
        }

        // System archives and DLC don't have an extended header but have RomFS
        if (ncch_header.extended_header_size) {
            auto read_exheader = [this](FileUtil::IOFile& file) {
                const std::size_t size = sizeof(exheader_header);
                return file && file.ReadBytes(&exheader_header, size) == size;
            };

            if (!read_exheader(file)) {
                return Loader::ResultStatus::Error;
            }

            if (is_encrypted) {
                // This ID check is masked to low 32-bit as a toleration to ill-formed ROM created
                // by merging games and its updates.
                if ((exheader_header.system_info.jump_id & 0xFFFFFFFF) ==
                    (ncch_header.program_id & 0xFFFFFFFF)) {
                    LOG_WARNING(Service_FS, "NCCH is marked as encrypted but with decrypted "
                                            "exheader. Force no crypto scheme.");
                    is_encrypted = false;
                } else {
                    if (failed_to_decrypt) {
                        LOG_ERROR(Service_FS, "Failed to decrypt");
                        return Loader::ResultStatus::ErrorEncrypted;
                    }
                    CryptoPP::byte* data = reinterpret_cast<CryptoPP::byte*>(&exheader_header);
                    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption(
                        primary_key.data(), primary_key.size(), exheader_ctr.data())
                        .ProcessData(data, data, sizeof(exheader_header));
                }
            }

            const auto mods_path =
                fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                            GetModId(ncch_header.program_id));
            const std::array<std::string, 2> exheader_override_paths{{
                mods_path + "exheader.bin",
                filepath + ".exheader",
            }};

            bool has_exheader_override = false;
            for (const auto& path : exheader_override_paths) {
                FileUtil::IOFile exheader_override_file{path, "rb"};
                if (read_exheader(exheader_override_file)) {
                    has_exheader_override = true;
                    break;
                }
            }
            if (has_exheader_override) {
                if (exheader_header.system_info.jump_id !=
                    exheader_header.arm11_system_local_caps.program_id) {
                    LOG_WARNING(Service_FS, "Jump ID and Program ID don't match. "
                                            "The override exheader might not be decrypted.");
                }
                is_tainted = true;
            }

            is_compressed = (exheader_header.codeset_info.flags.flag & 1) == 1;
            u32 entry_point = exheader_header.codeset_info.text.address;
            u32 code_size = exheader_header.codeset_info.text.code_size;
            u32 stack_size = exheader_header.codeset_info.stack_size;
            u32 bss_size = exheader_header.codeset_info.bss_size;
            u32 core_version = exheader_header.arm11_system_local_caps.core_version;
            u8 priority = exheader_header.arm11_system_local_caps.priority;
            u8 resource_limit_category =
                exheader_header.arm11_system_local_caps.resource_limit_category;

            LOG_DEBUG(Service_FS, "Name:                        {}",
                      reinterpret_cast<const char*>(exheader_header.codeset_info.name));
            LOG_DEBUG(Service_FS, "Program ID:                  {:016X}", ncch_header.program_id);
            LOG_DEBUG(Service_FS, "Code compressed:             {}", is_compressed ? "yes" : "no");
            LOG_DEBUG(Service_FS, "Entry point:                 0x{:08X}", entry_point);
            LOG_DEBUG(Service_FS, "Code size:                   0x{:08X}", code_size);
            LOG_DEBUG(Service_FS, "Stack size:                  0x{:08X}", stack_size);
            LOG_DEBUG(Service_FS, "Bss size:                    0x{:08X}", bss_size);
            LOG_DEBUG(Service_FS, "Core version:                {}", core_version);
            LOG_DEBUG(Service_FS, "Thread priority:             0x{:X}", priority);
            LOG_DEBUG(Service_FS, "Resource limit category:     {}", resource_limit_category);
            LOG_DEBUG(Service_FS, "System Mode:                 {}",
                      static_cast<int>(exheader_header.arm11_system_local_caps.system_mode));

            has_exheader = true;
        }

        // DLC can have an ExeFS and a RomFS but no extended header
        if (ncch_header.exefs_size) {
            exefs_offset = ncch_header.exefs_offset * kBlockSize;
            u32 exefs_size = ncch_header.exefs_size * kBlockSize;

            LOG_DEBUG(Service_FS, "ExeFS offset:                0x{:08X}", exefs_offset);
            LOG_DEBUG(Service_FS, "ExeFS size:                  0x{:08X}", exefs_size);

            file.Seek(exefs_offset + ncch_offset, SEEK_SET);
            if (file.ReadBytes(&exefs_header, sizeof(ExeFs_Header)) != sizeof(ExeFs_Header))
                return Loader::ResultStatus::Error;

            if (is_encrypted) {
                CryptoPP::byte* data = reinterpret_cast<CryptoPP::byte*>(&exefs_header);
                CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption(primary_key.data(),
                                                              primary_key.size(), exefs_ctr.data())
                    .ProcessData(data, data, sizeof(exefs_header));
            }

            exefs_file = FileUtil::IOFile(filepath, "rb");
            has_exefs = true;
        }

        if (ncch_header.romfs_offset != 0 && ncch_header.romfs_size != 0)
            has_romfs = true;
    }

    LoadOverrides();

    // We need at least one of these or overrides, practically
    if (!(has_exefs || has_romfs || is_tainted))
        return Loader::ResultStatus::Error;

    is_loaded = true;
    return Loader::ResultStatus::Success;
}

Loader::ResultStatus NCCHContainer::LoadOverrides() {
    // Check for split-off files, mark the archive as tainted if we will use them
    std::string romfs_override = filepath + ".romfs";
    if (FileUtil::Exists(romfs_override)) {
        is_tainted = true;
    }

    // If we have a split-off exefs file/folder, it takes priority
    std::string exefs_override = filepath + ".exefs";
    std::string exefsdir_override = filepath + ".exefsdir/";
    if (FileUtil::Exists(exefs_override)) {
        exefs_file = FileUtil::IOFile(exefs_override, "rb");

        if (exefs_file.ReadBytes(&exefs_header, sizeof(ExeFs_Header)) == sizeof(ExeFs_Header)) {
            LOG_DEBUG(Service_FS, "Loading ExeFS section from {}", exefs_override);
            exefs_offset = 0;
            is_tainted = true;
            has_exefs = true;
        } else {
            exefs_file = FileUtil::IOFile(filepath, "rb");
        }
    } else if (FileUtil::Exists(exefsdir_override) && FileUtil::IsDirectory(exefsdir_override)) {
        is_tainted = true;
    }

    if (is_tainted)
        LOG_WARNING(Service_FS,
                    "Loaded NCCH {} is tainted, application behavior may not be as expected!",
                    filepath);

    return Loader::ResultStatus::Success;
}

Loader::ResultStatus NCCHContainer::LoadSectionExeFS(const char* name, std::vector<u8>& buffer) {
    Loader::ResultStatus result = Load();
    if (result != Loader::ResultStatus::Success)
        return result;

    // Check if we have files that can drop-in and replace
    result = LoadOverrideExeFSSection(name, buffer);
    if (result == Loader::ResultStatus::Success || !has_exefs)
        return result;

    // As of firmware 5.0.0-11 the logo is stored between the access descriptor and the plain region
    // instead of the ExeFS.
    if (std::strcmp(name, "logo") == 0) {
        if (ncch_header.logo_region_offset && ncch_header.logo_region_size) {
            std::size_t logo_offset = ncch_header.logo_region_offset * kBlockSize;
            std::size_t logo_size = ncch_header.logo_region_size * kBlockSize;

            buffer.resize(logo_size);
            file.Seek(ncch_offset + logo_offset, SEEK_SET);

            if (file.ReadBytes(buffer.data(), logo_size) != logo_size) {
                LOG_ERROR(Service_FS, "Could not read NCCH logo");
                return Loader::ResultStatus::Error;
            }
            return Loader::ResultStatus::Success;
        } else {
            LOG_INFO(Service_FS, "Attempting to load logo from the ExeFS");
        }
    }

    // If we don't have any separate files, we'll need a full ExeFS
    if (!exefs_file.IsOpen())
        return Loader::ResultStatus::Error;

    LOG_DEBUG(Service_FS, "{} sections:", kMaxSections);
    // Iterate through the ExeFs archive until we find a section with the specified name...
    for (unsigned section_number = 0; section_number < kMaxSections; section_number++) {
        const auto& section = exefs_header.section[section_number];

        // Load the specified section...
        if (strcmp(section.name, name) == 0) {
            LOG_DEBUG(Service_FS, "{} - offset: 0x{:08X}, size: 0x{:08X}, name: {}", section_number,
                      section.offset, section.size, section.name);

            s64 section_offset =
                (section.offset + exefs_offset + sizeof(ExeFs_Header) + ncch_offset);
            exefs_file.Seek(section_offset, SEEK_SET);

            std::array<u8, 16> key;
            if (strcmp(section.name, "icon") == 0 || strcmp(section.name, "banner") == 0) {
                key = primary_key;
            } else {
                key = secondary_key;
            }

            CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec(key.data(), key.size(),
                                                              exefs_ctr.data());
            dec.Seek(section.offset + sizeof(ExeFs_Header));

            if (strcmp(section.name, ".code") == 0 && is_compressed) {
                // Section is compressed, read compressed .code section...
                std::vector<u8> temp_buffer(section.size);
                if (exefs_file.ReadBytes(temp_buffer.data(), temp_buffer.size()) !=
                    temp_buffer.size())
                    return Loader::ResultStatus::Error;

                if (is_encrypted) {
                    dec.ProcessData(&temp_buffer[0], &temp_buffer[0], section.size);
                }

                // Decompress .code section...
                buffer.resize(LZSS_GetDecompressedSize(temp_buffer));
                if (!LZSS_Decompress(temp_buffer, buffer)) {
                    return Loader::ResultStatus::ErrorInvalidFormat;
                }
            } else {
                // Section is uncompressed...
                buffer.resize(section.size);
                if (exefs_file.ReadBytes(buffer.data(), section.size) != section.size)
                    return Loader::ResultStatus::Error;
                if (is_encrypted) {
                    dec.ProcessData(buffer.data(), buffer.data(), section.size);
                }
            }

            return Loader::ResultStatus::Success;
        }
    }
    return Loader::ResultStatus::ErrorNotUsed;
}

Loader::ResultStatus NCCHContainer::ApplyCodePatch(std::vector<u8>& code) const {
    struct PatchLocation {
        std::string path;
        bool (*patch_fn)(const std::vector<u8>& patch, std::vector<u8>& code);
    };

    const auto mods_path =
        fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                    GetModId(ncch_header.program_id));

    constexpr u32 system_module_tid_high = 0x00040130;

    std::string luma_ips_location;
    if ((static_cast<u32>(ncch_header.program_id >> 32) & system_module_tid_high) ==
        system_module_tid_high) {
        luma_ips_location =
            fmt::format("{}luma/sysmodules/{:016X}.ips",
                        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), ncch_header.program_id);
    } else {
        luma_ips_location =
            fmt::format("{}luma/titles/{:016X}/code.ips",
                        FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), ncch_header.program_id);
    }
    const std::array<PatchLocation, 7> patch_paths{{
        {mods_path + "exefs/code.ips", Patch::ApplyIpsPatch},
        {mods_path + "exefs/code.bps", Patch::ApplyBpsPatch},
        {mods_path + "code.ips", Patch::ApplyIpsPatch},
        {mods_path + "code.bps", Patch::ApplyBpsPatch},
        {luma_ips_location, Patch::ApplyIpsPatch},
        {filepath + ".exefsdir/code.ips", Patch::ApplyIpsPatch},
        {filepath + ".exefsdir/code.bps", Patch::ApplyBpsPatch},
    }};

    for (const PatchLocation& info : patch_paths) {
        FileUtil::IOFile patch_file{info.path, "rb"};
        if (!patch_file)
            continue;

        std::vector<u8> patch(patch_file.GetSize());
        if (patch_file.ReadBytes(patch.data(), patch.size()) != patch.size())
            return Loader::ResultStatus::Error;

        LOG_INFO(Service_FS, "File {} patching code.bin", info.path);
        if (!info.patch_fn(patch, code))
            return Loader::ResultStatus::Error;

        return Loader::ResultStatus::Success;
    }
    return Loader::ResultStatus::ErrorNotUsed;
}

Loader::ResultStatus NCCHContainer::LoadOverrideExeFSSection(const char* name,
                                                             std::vector<u8>& buffer) {
    std::string override_name;

    // Map our section name to the extracted equivalent
    if (!strcmp(name, ".code"))
        override_name = "code.bin";
    else if (!strcmp(name, "icon"))
        override_name = "icon.bin";
    else if (!strcmp(name, "banner"))
        override_name = "banner.bnr";
    else if (!strcmp(name, "logo"))
        override_name = "logo.bcma.lz";
    else
        return Loader::ResultStatus::Error;

    const auto mods_path =
        fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                    GetModId(ncch_header.program_id));
    const std::array<std::string, 3> override_paths{{
        mods_path + "exefs/" + override_name,
        mods_path + override_name,
        filepath + ".exefsdir/" + override_name,
    }};

    for (const auto& path : override_paths) {
        FileUtil::IOFile section_file(path, "rb");

        if (section_file.IsOpen()) {
            auto section_size = section_file.GetSize();
            buffer.resize(section_size);

            section_file.Seek(0, SEEK_SET);
            if (section_file.ReadBytes(buffer.data(), section_size) == section_size) {
                LOG_WARNING(Service_FS, "File {} overriding built-in ExeFS file", path);
                return Loader::ResultStatus::Success;
            }
        }
    }
    return Loader::ResultStatus::ErrorNotUsed;
}

Loader::ResultStatus NCCHContainer::ReadRomFS(std::shared_ptr<RomFSReader>& romfs_file,
                                              bool use_layered_fs) {
    Loader::ResultStatus result = Load();
    if (result != Loader::ResultStatus::Success)
        return result;

    if (ReadOverrideRomFS(romfs_file) == Loader::ResultStatus::Success)
        return Loader::ResultStatus::Success;

    if (!has_romfs) {
        LOG_DEBUG(Service_FS, "RomFS requested from NCCH which has no RomFS");
        return Loader::ResultStatus::ErrorNotUsed;
    }

    if (!file.IsOpen())
        return Loader::ResultStatus::Error;

    u32 romfs_offset = ncch_offset + (ncch_header.romfs_offset * kBlockSize) + 0x1000;
    u32 romfs_size = (ncch_header.romfs_size * kBlockSize) - 0x1000;

    LOG_DEBUG(Service_FS, "RomFS offset:           0x{:08X}", romfs_offset);
    LOG_DEBUG(Service_FS, "RomFS size:             0x{:08X}", romfs_size);

    if (file.GetSize() < romfs_offset + romfs_size)
        return Loader::ResultStatus::Error;

    // We reopen the file, to allow its position to be independent from file's
    FileUtil::IOFile romfs_file_inner(filepath, "rb");
    if (!romfs_file_inner.IsOpen())
        return Loader::ResultStatus::Error;

    std::shared_ptr<RomFSReader> direct_romfs;
    if (is_encrypted) {
        direct_romfs =
            std::make_shared<DirectRomFSReader>(std::move(romfs_file_inner), romfs_offset,
                                                romfs_size, secondary_key, romfs_ctr, 0x1000);
    } else {
        direct_romfs = std::make_shared<DirectRomFSReader>(std::move(romfs_file_inner),
                                                           romfs_offset, romfs_size);
    }

    const auto path =
        fmt::format("{}mods/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::LoadDir),
                    GetModId(ncch_header.program_id));
    if (use_layered_fs &&
        (FileUtil::Exists(path + "romfs/") || FileUtil::Exists(path + "romfs_ext/"))) {

        romfs_file = std::make_shared<LayeredFS>(std::move(direct_romfs), path + "romfs/",
                                                 path + "romfs_ext/");
    } else {
        romfs_file = std::move(direct_romfs);
    }

    return Loader::ResultStatus::Success;
}

Loader::ResultStatus NCCHContainer::DumpRomFS(const std::string& target_path) {
    std::shared_ptr<RomFSReader> direct_romfs;
    Loader::ResultStatus result = ReadRomFS(direct_romfs, false);
    if (result != Loader::ResultStatus::Success)
        return result;

    std::shared_ptr<LayeredFS> layered_fs =
        std::make_shared<LayeredFS>(std::move(direct_romfs), "", "", false);

    if (!layered_fs->DumpRomFS(target_path)) {
        return Loader::ResultStatus::Error;
    }
    return Loader::ResultStatus::Success;
}

Loader::ResultStatus NCCHContainer::ReadOverrideRomFS(std::shared_ptr<RomFSReader>& romfs_file) {
    // Check for RomFS overrides
    std::string split_filepath = filepath + ".romfs";
    if (FileUtil::Exists(split_filepath)) {
        FileUtil::IOFile romfs_file_inner(split_filepath, "rb");
        if (romfs_file_inner.IsOpen()) {
            LOG_WARNING(Service_FS, "File {} overriding built-in RomFS; LayeredFS not enabled",
                        split_filepath);
            romfs_file = std::make_shared<DirectRomFSReader>(std::move(romfs_file_inner), 0,
                                                             romfs_file_inner.GetSize());
            return Loader::ResultStatus::Success;
        }
    }

    return Loader::ResultStatus::ErrorNotUsed;
}

Loader::ResultStatus NCCHContainer::ReadProgramId(u64_le& program_id) {
    Loader::ResultStatus result = LoadHeader();
    if (result != Loader::ResultStatus::Success)
        return result;

    if (!has_header)
        return Loader::ResultStatus::ErrorNotUsed;

    program_id = ncch_header.program_id;
    return Loader::ResultStatus::Success;
}

Loader::ResultStatus NCCHContainer::ReadExtdataId(u64& extdata_id) {
    Loader::ResultStatus result = Load();
    if (result != Loader::ResultStatus::Success)
        return result;

    if (!has_exheader)
        return Loader::ResultStatus::ErrorNotUsed;

    if (exheader_header.arm11_system_local_caps.storage_info.other_attributes >> 1) {
        // Using extended save data access
        // There would be multiple possible extdata IDs in this case. The best we can do for now is
        // guessing that the first one would be the main save.
        const std::array<u64, 6> extdata_ids{{
            exheader_header.arm11_system_local_caps.storage_info.extdata_id0.Value(),
            exheader_header.arm11_system_local_caps.storage_info.extdata_id1.Value(),
            exheader_header.arm11_system_local_caps.storage_info.extdata_id2.Value(),
            exheader_header.arm11_system_local_caps.storage_info.extdata_id3.Value(),
            exheader_header.arm11_system_local_caps.storage_info.extdata_id4.Value(),
            exheader_header.arm11_system_local_caps.storage_info.extdata_id5.Value(),
        }};
        for (u64 id : extdata_ids) {
            if (id) {
                // Found a non-zero ID, use it
                extdata_id = id;
                return Loader::ResultStatus::Success;
            }
        }

        return Loader::ResultStatus::ErrorNotUsed;
    }

    extdata_id = exheader_header.arm11_system_local_caps.storage_info.ext_save_data_id;
    return Loader::ResultStatus::Success;
}

bool NCCHContainer::HasExeFS() {
    Loader::ResultStatus result = Load();
    if (result != Loader::ResultStatus::Success)
        return false;

    return has_exefs;
}

bool NCCHContainer::HasRomFS() {
    Loader::ResultStatus result = Load();
    if (result != Loader::ResultStatus::Success)
        return false;

    return has_romfs;
}

bool NCCHContainer::HasExHeader() {
    Loader::ResultStatus result = Load();
    if (result != Loader::ResultStatus::Success)
        return false;

    return has_exheader;
}

} // namespace FileSys
