// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <fmt/format.h>
#include "common/alignment.h"
#include "common/archives.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"
#include "core/hle/service/am/am_net.h"
#include "core/hle/service/am/am_sys.h"
#include "core/hle/service/am/am_u.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/loader/loader.h"
#include "core/loader/smdh.h"
#include "core/nus_download.h"

SERIALIZE_EXPORT_IMPL(Service::AM::Module)
SERVICE_CONSTRUCT_IMPL(Service::AM::Module)

namespace Service::AM {

constexpr u16 PLATFORM_CTR = 0x0004;
constexpr u16 CATEGORY_SYSTEM = 0x0010;
constexpr u16 CATEGORY_DLP = 0x0001;
constexpr u8 VARIATION_SYSTEM = 0x02;
constexpr u32 TID_HIGH_UPDATE = 0x0004000E;
constexpr u32 TID_HIGH_DLC = 0x0004008C;

struct TitleInfo {
    u64_le tid;
    u64_le size;
    u16_le version;
    u16_le unused;
    u32_le type;
};

static_assert(sizeof(TitleInfo) == 0x18, "Title info structure size is wrong");

constexpr u8 OWNERSHIP_DOWNLOADED = 0x01;
constexpr u8 OWNERSHIP_OWNED = 0x02;

struct ContentInfo {
    u16_le index;
    u16_le type;
    u32_le content_id;
    u64_le size;
    u8 ownership;
    INSERT_PADDING_BYTES(0x7);
};

static_assert(sizeof(ContentInfo) == 0x18, "Content info structure size is wrong");

struct TicketInfo {
    u64_le title_id;
    u64_le ticket_id;
    u16_le version;
    u16_le unused;
    u32_le size;
};

static_assert(sizeof(TicketInfo) == 0x18, "Ticket info structure size is wrong");

class CIAFile::DecryptionState {
public:
    std::vector<CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption> content;
};

CIAFile::CIAFile(Core::System& system_, Service::FS::MediaType media_type)
    : system(system_), media_type(media_type),
      decryption_state(std::make_unique<DecryptionState>()) {}

CIAFile::~CIAFile() {
    Close();
}

ResultVal<std::size_t> CIAFile::Read(u64 offset, std::size_t length, u8* buffer) const {
    UNIMPLEMENTED();
    return length;
}

Result CIAFile::WriteTicket() {
    auto load_result = container.LoadTicket(data, container.GetTicketOffset());
    if (load_result != Loader::ResultStatus::Success) {
        LOG_ERROR(Service_AM, "Could not read ticket from CIA.");
        // TODO: Correct result code.
        return {ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                ErrorLevel::Permanent};
    }

    // TODO: Write out .tik files to nand?

    install_state = CIAInstallState::TicketLoaded;
    return ResultSuccess;
}

Result CIAFile::WriteTitleMetadata() {
    auto load_result = container.LoadTitleMetadata(data, container.GetTitleMetadataOffset());
    if (load_result != Loader::ResultStatus::Success) {
        LOG_ERROR(Service_AM, "Could not read title metadata from CIA.");
        // TODO: Correct result code.
        return {ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                ErrorLevel::Permanent};
    }

    FileSys::TitleMetadata tmd = container.GetTitleMetadata();
    tmd.Print();

    // If a TMD already exists for this app (ie 00000000.tmd), the incoming TMD
    // will be the same plus one, (ie 00000001.tmd), both will be kept until
    // the install is finalized and old contents can be discarded.
    if (FileUtil::Exists(GetTitleMetadataPath(media_type, tmd.GetTitleID()))) {
        is_update = true;
    }

    std::string tmd_path = GetTitleMetadataPath(media_type, tmd.GetTitleID(), is_update);

    // Create content/ folder if it doesn't exist
    std::string tmd_folder;
    Common::SplitPath(tmd_path, &tmd_folder, nullptr, nullptr);
    FileUtil::CreateFullPath(tmd_folder);

    // Save TMD so that we can start getting new .app paths
    if (tmd.Save(tmd_path) != Loader::ResultStatus::Success) {
        LOG_ERROR(Service_AM, "Failed to install title metadata file from CIA.");
        // TODO: Correct result code.
        return FileSys::ResultFileNotFound;
    }

    // Create any other .app folders which may not exist yet
    std::string app_folder;
    auto main_content_path = GetTitleContentPath(media_type, tmd.GetTitleID(),
                                                 FileSys::TMDContentIndex::Main, is_update);
    Common::SplitPath(main_content_path, &app_folder, nullptr, nullptr);
    FileUtil::CreateFullPath(app_folder);

    auto content_count = container.GetTitleMetadata().GetContentCount();
    content_written.resize(content_count);

    content_files.clear();
    for (std::size_t i = 0; i < content_count; i++) {
        auto path = GetTitleContentPath(media_type, tmd.GetTitleID(), i, is_update);
        auto& file = content_files.emplace_back(path, "wb");
        if (!file.IsOpen()) {
            LOG_ERROR(Service_AM, "Could not open output file '{}' for content {}.", path, i);
            // TODO: Correct error code.
            return FileSys::ResultFileNotFound;
        }
    }

    if (container.GetTitleMetadata().HasEncryptedContent()) {
        if (auto title_key = container.GetTicket().GetTitleKey()) {
            decryption_state->content.resize(content_count);
            for (std::size_t i = 0; i < content_count; ++i) {
                auto ctr = tmd.GetContentCTRByIndex(i);
                decryption_state->content[i].SetKeyWithIV(title_key->data(), title_key->size(),
                                                          ctr.data());
            }
        } else {
            LOG_ERROR(Service_AM, "Could not read title key from ticket for encrypted CIA.");
            // TODO: Correct error code.
            return FileSys::ResultFileNotFound;
        }
    } else {
        LOG_INFO(Service_AM,
                 "Title has no encrypted content, skipping initializing decryption state.");
    }

    install_state = CIAInstallState::TMDLoaded;

    return ResultSuccess;
}

ResultVal<std::size_t> CIAFile::WriteContentData(u64 offset, std::size_t length, const u8* buffer) {
    // Data is not being buffered, so we have to keep track of how much of each <ID>.app
    // has been written since we might get a written buffer which contains multiple .app
    // contents or only part of a larger .app's contents.
    const u64 offset_max = offset + length;
    for (std::size_t i = 0; i < content_written.size(); i++) {
        if (content_written[i] < container.GetContentSize(i)) {
            // The size, minimum unwritten offset, and maximum unwritten offset of this content
            const u64 size = container.GetContentSize(i);
            const u64 range_min = container.GetContentOffset(i) + content_written[i];
            const u64 range_max = container.GetContentOffset(i) + size;

            // The unwritten range for this content is beyond the buffered data we have
            // or comes before the buffered data we have, so skip this content ID.
            if (range_min > offset_max || range_max < offset) {
                continue;
            }

            // Figure out how much of this content ID we have just recieved/can write out
            const u64 available_to_write = std::min(offset_max, range_max) - range_min;

            // Since the incoming TMD has already been written, we can use GetTitleContentPath
            // to get the content paths to write to.
            FileSys::TitleMetadata tmd = container.GetTitleMetadata();
            auto& file = content_files[i];

            std::vector<u8> temp(buffer + (range_min - offset),
                                 buffer + (range_min - offset) + available_to_write);

            if ((tmd.GetContentTypeByIndex(i) & FileSys::TMDContentTypeFlag::Encrypted) != 0) {
                decryption_state->content[i].ProcessData(temp.data(), temp.data(), temp.size());
            }

            file.WriteBytes(temp.data(), temp.size());

            // Keep tabs on how much of this content ID has been written so new range_min
            // values can be calculated.
            content_written[i] += available_to_write;
            LOG_DEBUG(Service_AM, "Wrote {:x} to content {}, total {:x}", available_to_write, i,
                      content_written[i]);
        }
    }

    return length;
}

ResultVal<std::size_t> CIAFile::Write(u64 offset, std::size_t length, bool flush,
                                      const u8* buffer) {
    written += length;

    // TODO(shinyquagsire23): Can we assume that things will only be written in sequence?
    // Does AM send an error if we write to things out of order?
    // Or does it just ignore offsets and assume a set sequence of incoming data?

    // The data in CIAs is always stored CIA Header > Cert > Ticket > TMD > Content > Meta.
    // The CIA Header describes Cert, Ticket, TMD, total content sizes, and TMD is needed for
    // content sizes so it ends up becoming a problem of keeping track of how much has been
    // written and what we have been able to pick up.
    if (install_state == CIAInstallState::InstallStarted) {
        std::size_t buf_copy_size = std::min(length, FileSys::CIA_HEADER_SIZE);
        std::size_t buf_max_size =
            std::min(static_cast<std::size_t>(offset + length), FileSys::CIA_HEADER_SIZE);
        data.resize(buf_max_size);
        std::memcpy(data.data() + offset, buffer, buf_copy_size);

        // We have enough data to load a CIA header and parse it.
        if (written >= FileSys::CIA_HEADER_SIZE) {
            container.LoadHeader(data);
            container.Print();
            install_state = CIAInstallState::HeaderLoaded;
        }
    }

    // If we don't have a header yet, we can't pull offsets of other sections
    if (install_state == CIAInstallState::InstallStarted) {
        return length;
    }

    // If we have been given data before (or including) .app content, pull it into
    // our buffer, but only pull *up to* the content offset, no further.
    if (offset < container.GetContentOffset()) {
        std::size_t buf_loaded = data.size();
        std::size_t copy_offset = std::max(static_cast<std::size_t>(offset), buf_loaded);
        std::size_t buf_offset = buf_loaded - offset;
        std::size_t buf_copy_size =
            std::min(length, static_cast<std::size_t>(container.GetContentOffset() - offset)) -
            buf_offset;
        std::size_t buf_max_size = std::min(offset + length, container.GetContentOffset());
        data.resize(buf_max_size);
        std::memcpy(data.data() + copy_offset, buffer + buf_offset, buf_copy_size);
    }

    // The end of our TMD is at the beginning of Content data, so ensure we have that much
    // buffered before trying to parse.
    if (written >= container.GetContentOffset() && install_state != CIAInstallState::TMDLoaded) {
        auto result = WriteTicket();
        if (result.IsError()) {
            return result;
        }

        result = WriteTitleMetadata();
        if (result.IsError()) {
            return result;
        }
    }

    // Content data sizes can only be retrieved from TMD data
    if (install_state != CIAInstallState::TMDLoaded) {
        return length;
    }

    // From this point forward, data will no longer be buffered in data
    auto result = WriteContentData(offset, length, buffer);
    if (result.Failed()) {
        return result;
    }

    return length;
}

u64 CIAFile::GetSize() const {
    return written;
}

bool CIAFile::SetSize(u64 size) const {
    return false;
}

bool CIAFile::Close() const {
    bool complete =
        install_state >= CIAInstallState::TMDLoaded &&
        content_written.size() == container.GetTitleMetadata().GetContentCount() &&
        std::all_of(content_written.begin(), content_written.end(),
                    [this, i = 0](auto& bytes_written) mutable {
                        return bytes_written >= container.GetContentSize(static_cast<u16>(i++));
                    });

    // Install aborted
    if (!complete) {
        LOG_ERROR(Service_AM, "CIAFile closed prematurely, aborting install...");
        FileUtil::DeleteDir(GetTitlePath(media_type, container.GetTitleMetadata().GetTitleID()));
        return true;
    }

    // Clean up older content data if we installed newer content on top
    std::string old_tmd_path =
        GetTitleMetadataPath(media_type, container.GetTitleMetadata().GetTitleID(), false);
    std::string new_tmd_path =
        GetTitleMetadataPath(media_type, container.GetTitleMetadata().GetTitleID(), true);
    if (FileUtil::Exists(new_tmd_path) && old_tmd_path != new_tmd_path) {
        FileSys::TitleMetadata old_tmd;
        FileSys::TitleMetadata new_tmd;

        old_tmd.Load(old_tmd_path);
        new_tmd.Load(new_tmd_path);

        // For each content ID in the old TMD, check if there is a matching ID in the new
        // TMD. If a CIA contains (and wrote to) an identical ID, it should be kept while
        // IDs which only existed for the old TMD should be deleted.
        for (std::size_t old_index = 0; old_index < old_tmd.GetContentCount(); old_index++) {
            bool abort = false;
            for (std::size_t new_index = 0; new_index < new_tmd.GetContentCount(); new_index++) {
                if (old_tmd.GetContentIDByIndex(old_index) ==
                    new_tmd.GetContentIDByIndex(new_index)) {
                    abort = true;
                }
            }
            if (abort) {
                break;
            }

            // If the file to delete is the current launched rom, signal the system to delete
            // the current rom instead of deleting it now, once all the handles to the file
            // are closed.
            std::string to_delete =
                GetTitleContentPath(media_type, old_tmd.GetTitleID(), old_index);
            if (!system.IsPoweredOn() || !system.SetSelfDelete(to_delete)) {
                FileUtil::Delete(to_delete);
            }
        }

        FileUtil::Delete(old_tmd_path);
    }
    return true;
}

void CIAFile::Flush() const {}

TicketFile::TicketFile() {}

TicketFile::~TicketFile() {
    Close();
}

ResultVal<std::size_t> TicketFile::Read(u64 offset, std::size_t length, u8* buffer) const {
    UNIMPLEMENTED();
    return length;
}

ResultVal<std::size_t> TicketFile::Write(u64 offset, std::size_t length, bool flush,
                                         const u8* buffer) {
    written += length;
    data.resize(written);
    std::memcpy(data.data() + offset, buffer, length);
    return length;
}

u64 TicketFile::GetSize() const {
    return written;
}

bool TicketFile::SetSize(u64 size) const {
    return false;
}

bool TicketFile::Close() const {
    FileSys::Ticket ticket;
    if (ticket.Load(data, 0) == Loader::ResultStatus::Success) {
        LOG_WARNING(Service_AM, "Discarding ticket for {:#016X}.", ticket.GetTitleID());
    } else {
        LOG_ERROR(Service_AM, "Invalid ticket provided to TicketFile.");
    }
    return true;
}

void TicketFile::Flush() const {}

InstallStatus InstallCIA(const std::string& path,
                         std::function<ProgressCallback>&& update_callback) {
    LOG_INFO(Service_AM, "Installing {}...", path);

    if (!FileUtil::Exists(path)) {
        LOG_ERROR(Service_AM, "File {} does not exist!", path);
        return InstallStatus::ErrorFileNotFound;
    }

    FileSys::CIAContainer container;
    if (container.Load(path) == Loader::ResultStatus::Success) {
        Service::AM::CIAFile installFile(
            Core::System::GetInstance(),
            Service::AM::GetTitleMediaType(container.GetTitleMetadata().GetTitleID()));

        bool title_key_available = container.GetTicket().GetTitleKey().has_value();
        if (!title_key_available && container.GetTitleMetadata().HasEncryptedContent()) {
            LOG_ERROR(Service_AM, "File {} is encrypted and no title key is available! Aborting...",
                      path);
            return InstallStatus::ErrorEncrypted;
        }

        FileUtil::IOFile file(path, "rb");
        if (!file.IsOpen()) {
            LOG_ERROR(Service_AM, "Could not open CIA file '{}'.", path);
            return InstallStatus::ErrorFailedToOpenFile;
        }

        std::array<u8, 0x10000> buffer;
        auto file_size = file.GetSize();
        std::size_t total_bytes_read = 0;
        while (total_bytes_read != file_size) {
            std::size_t bytes_read = file.ReadBytes(buffer.data(), buffer.size());
            auto result = installFile.Write(static_cast<u64>(total_bytes_read), bytes_read, true,
                                            static_cast<u8*>(buffer.data()));

            if (update_callback) {
                update_callback(total_bytes_read, file_size);
            }
            if (result.Failed()) {
                LOG_ERROR(Service_AM, "CIA file installation aborted with error code {:08x}",
                          result.Code().raw);
                return InstallStatus::ErrorAborted;
            }
            total_bytes_read += bytes_read;
        }
        installFile.Close();

        LOG_INFO(Service_AM, "Installed {} successfully.", path);

        const FileUtil::DirectoryEntryCallable callback =
            [&callback](u64* num_entries_out, const std::string& directory,
                        const std::string& virtual_name) -> bool {
            const std::string physical_name = directory + DIR_SEP + virtual_name;
            const bool is_dir = FileUtil::IsDirectory(physical_name);
            if (!is_dir) {
                std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(physical_name);
                if (!loader) {
                    return true;
                }

                bool executable = false;
                const auto res = loader->IsExecutable(executable);
                if (res == Loader::ResultStatus::ErrorEncrypted) {
                    return false;
                }
                return true;
            } else {
                return FileUtil::ForeachDirectoryEntry(nullptr, physical_name, callback);
            }
        };
        if (!FileUtil::ForeachDirectoryEntry(
                nullptr,
                GetTitlePath(
                    Service::AM::GetTitleMediaType(container.GetTitleMetadata().GetTitleID()),
                    container.GetTitleMetadata().GetTitleID()),
                callback)) {
            LOG_ERROR(Service_AM, "CIA {} contained encrypted files.", path);
            return InstallStatus::ErrorEncrypted;
        }
        return InstallStatus::Success;
    }

    LOG_ERROR(Service_AM, "CIA file {} is invalid!", path);
    return InstallStatus::ErrorInvalid;
}

InstallStatus InstallFromNus(u64 title_id, int version) {
    LOG_DEBUG(Service_AM, "Downloading {:X}", title_id);

    CIAFile install_file{Core::System::GetInstance(), GetTitleMediaType(title_id)};

    std::string path = fmt::format("/ccs/download/{:016X}/tmd", title_id);
    if (version != -1) {
        path += fmt::format(".{}", version);
    }
    auto tmd_response = Core::NUS::Download(path);
    if (!tmd_response) {
        LOG_ERROR(Service_AM, "Failed to download tmd for {:016X}", title_id);
        return InstallStatus::ErrorFileNotFound;
    }
    FileSys::TitleMetadata tmd;
    tmd.Load(*tmd_response);

    path = fmt::format("/ccs/download/{:016X}/cetk", title_id);
    auto cetk_response = Core::NUS::Download(path);
    if (!cetk_response) {
        LOG_ERROR(Service_AM, "Failed to download cetk for {:016X}", title_id);
        return InstallStatus::ErrorFileNotFound;
    }

    std::vector<u8> content;
    const auto content_count = tmd.GetContentCount();
    for (std::size_t i = 0; i < content_count; ++i) {
        const std::string filename = fmt::format("{:08x}", tmd.GetContentIDByIndex(i));
        path = fmt::format("/ccs/download/{:016X}/{}", title_id, filename);
        const auto temp_response = Core::NUS::Download(path);
        if (!temp_response) {
            LOG_ERROR(Service_AM, "Failed to download content for {:016X}", title_id);
            return InstallStatus::ErrorFileNotFound;
        }
        content.insert(content.end(), temp_response->begin(), temp_response->end());
    }

    FileSys::CIAContainer::Header fake_header{
        .header_size = sizeof(FileSys::CIAContainer::Header),
        .type = 0,
        .version = 0,
        .cert_size = 0,
        .tik_size = static_cast<u32_le>(cetk_response->size()),
        .tmd_size = static_cast<u32_le>(tmd_response->size()),
        .meta_size = 0,
    };
    for (u16 i = 0; i < content_count; ++i) {
        fake_header.SetContentPresent(i);
    }
    std::vector<u8> header_data(sizeof(fake_header));
    std::memcpy(header_data.data(), &fake_header, sizeof(fake_header));

    std::size_t current_offset = 0;
    const auto write_to_cia_file_aligned = [&install_file, &current_offset](std::vector<u8>& data) {
        const u64 offset =
            Common::AlignUp(current_offset + data.size(), FileSys::CIA_SECTION_ALIGNMENT);
        data.resize(offset - current_offset, 0);
        const auto result = install_file.Write(current_offset, data.size(), true, data.data());
        if (result.Failed()) {
            LOG_ERROR(Service_AM, "CIA file installation aborted with error code {:08x}",
                      result.Code().raw);
            return InstallStatus::ErrorAborted;
        }
        current_offset += data.size();
        return InstallStatus::Success;
    };

    auto result = write_to_cia_file_aligned(header_data);
    if (result != InstallStatus::Success) {
        return result;
    }

    result = write_to_cia_file_aligned(*cetk_response);
    if (result != InstallStatus::Success) {
        return result;
    }

    result = write_to_cia_file_aligned(*tmd_response);
    if (result != InstallStatus::Success) {
        return result;
    }

    result = write_to_cia_file_aligned(content);
    if (result != InstallStatus::Success) {
        return result;
    }
    return InstallStatus::Success;
}

u64 GetTitleUpdateId(u64 title_id) {
    // Real services seem to just discard and replace the whole high word.
    return (title_id & 0xFFFFFFFF) | (static_cast<u64>(TID_HIGH_UPDATE) << 32);
}

Service::FS::MediaType GetTitleMediaType(u64 titleId) {
    u16 platform = static_cast<u16>(titleId >> 48);
    u16 category = static_cast<u16>((titleId >> 32) & 0xFFFF);
    u8 variation = static_cast<u8>(titleId & 0xFF);

    if (platform != PLATFORM_CTR)
        return Service::FS::MediaType::NAND;

    if (category & CATEGORY_SYSTEM || category & CATEGORY_DLP || variation & VARIATION_SYSTEM)
        return Service::FS::MediaType::NAND;

    return Service::FS::MediaType::SDMC;
}

std::string GetTitleMetadataPath(Service::FS::MediaType media_type, u64 tid, bool update) {
    std::string content_path = GetTitlePath(media_type, tid) + "content/";

    if (media_type == Service::FS::MediaType::GameCard) {
        LOG_ERROR(Service_AM, "Invalid request for nonexistent gamecard title metadata!");
        return "";
    }

    // The TMD ID is usually held in the title databases, which we don't implement.
    // For now, just scan for any .tmd files which exist, the smallest will be the
    // base ID and the largest will be the (currently installing) update ID.
    constexpr u32 MAX_TMD_ID = 0xFFFFFFFF;
    u32 base_id = MAX_TMD_ID;
    u32 update_id = 0;
    FileUtil::FSTEntry entries;
    FileUtil::ScanDirectoryTree(content_path, entries);
    for (const FileUtil::FSTEntry& entry : entries.children) {
        std::string filename_filename, filename_extension;
        Common::SplitPath(entry.virtualName, nullptr, &filename_filename, &filename_extension);

        if (filename_extension == ".tmd") {
            const u32 id = static_cast<u32>(std::stoul(filename_filename, nullptr, 16));
            base_id = std::min(base_id, id);
            update_id = std::max(update_id, id);
        }
    }

    // If we didn't find anything, default to 00000000.tmd for it to be created.
    if (base_id == MAX_TMD_ID)
        base_id = 0;

    // Update ID should be one more than the last, if it hasn't been created yet.
    if (base_id == update_id)
        update_id++;

    return content_path + fmt::format("{:08x}.tmd", (update ? update_id : base_id));
}

std::string GetTitleContentPath(Service::FS::MediaType media_type, u64 tid, std::size_t index,
                                bool update) {

    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO(B3N30): check if TID matches
        auto fs_user =
            Core::System::GetInstance().ServiceManager().GetService<Service::FS::FS_USER>(
                "fs:USER");
        return fs_user->GetCurrentGamecardPath();
    }

    std::string content_path = GetTitlePath(media_type, tid) + "content/";

    std::string tmd_path = GetTitleMetadataPath(media_type, tid, update);

    u32 content_id = 0;
    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
        if (index < tmd.GetContentCount()) {
            content_id = tmd.GetContentIDByIndex(index);
        } else {
            LOG_ERROR(Service_AM, "Attempted to get path for non-existent content index {:04x}.",
                      index);
            return "";
        }

        // TODO(shinyquagsire23): how does DLC actually get this folder on hardware?
        // For now, check if the second (index 1) content has the optional flag set, for most
        // apps this is usually the manual and not set optional, DLC has it set optional.
        // All .apps (including index 0) will be in the 00000000/ folder for DLC.
        if (tmd.GetContentCount() > 1 &&
            tmd.GetContentTypeByIndex(1) & FileSys::TMDContentTypeFlag::Optional) {
            content_path += "00000000/";
        }
    }

    return fmt::format("{}{:08x}.app", content_path, content_id);
}

std::string GetTitlePath(Service::FS::MediaType media_type, u64 tid) {
    // TODO(PabloMK7) TWL titles should be in TWL Nand. Assuming CTR Nand for now.

    u32 high = static_cast<u32>(tid >> 32);
    u32 low = static_cast<u32>(tid & 0xFFFFFFFF);

    if (media_type == Service::FS::MediaType::NAND || media_type == Service::FS::MediaType::SDMC)
        return fmt::format("{}{:08x}/{:08x}/", GetMediaTitlePath(media_type), high, low);

    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO(B3N30): check if TID matches
        auto fs_user =
            Core::System::GetInstance().ServiceManager().GetService<Service::FS::FS_USER>(
                "fs:USER");
        return fs_user->GetCurrentGamecardPath();
    }

    return "";
}

std::string GetMediaTitlePath(Service::FS::MediaType media_type) {
    if (media_type == Service::FS::MediaType::NAND)
        return fmt::format("{}{}/title/", FileUtil::GetUserPath(FileUtil::UserPath::NANDDir),
                           SYSTEM_ID);

    if (media_type == Service::FS::MediaType::SDMC)
        return fmt::format("{}Nintendo 3DS/{}/{}/title/",
                           FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir), SYSTEM_ID,
                           SDCARD_ID);

    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO(B3N30): check if TID matchess
        auto fs_user =
            Core::System::GetInstance().ServiceManager().GetService<Service::FS::FS_USER>(
                "fs:USER");
        return fs_user->GetCurrentGamecardPath();
    }

    return "";
}

void Module::ScanForTitles(Service::FS::MediaType media_type) {
    am_title_list[static_cast<u32>(media_type)].clear();

    std::string title_path = GetMediaTitlePath(media_type);

    FileUtil::FSTEntry entries;
    FileUtil::ScanDirectoryTree(title_path, entries, 1);
    for (const FileUtil::FSTEntry& tid_high : entries.children) {
        for (const FileUtil::FSTEntry& tid_low : tid_high.children) {
            std::string tid_string = tid_high.virtualName + tid_low.virtualName;

            if (tid_string.length() == TITLE_ID_VALID_LENGTH) {
                const u64 tid = std::stoull(tid_string, nullptr, 16);

                if (tid & TWL_TITLE_ID_FLAG) {
                    // TODO(PabloMK7) Move to TWL Nand, for now only check that
                    // the contents exists in CTR Nand as this is a SRL file
                    // instead of NCCH.
                    if (FileUtil::Exists(GetTitleContentPath(media_type, tid))) {
                        am_title_list[static_cast<u32>(media_type)].push_back(tid);
                    }
                } else {
                    FileSys::NCCHContainer container(GetTitleContentPath(media_type, tid));
                    if (container.Load() == Loader::ResultStatus::Success) {
                        am_title_list[static_cast<u32>(media_type)].push_back(tid);
                    }
                }
            }
        }
    }
}

void Module::ScanForAllTitles() {
    ScanForTitles(Service::FS::MediaType::NAND);
    ScanForTitles(Service::FS::MediaType::SDMC);
}

Module::Interface::Interface(std::shared_ptr<Module> am, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), am(std::move(am)) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetNumPrograms(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 media_type = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(am->am_title_list[media_type].size()));
}

void Module::Interface::FindDLCContentInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u64 title_id = rp.Pop<u64>();
    u32 content_count = rp.Pop<u32>();
    auto& content_requested_in = rp.PopMappedBuffer();
    auto& content_info_out = rp.PopMappedBuffer();

    // Validate that only DLC TIDs are passed in
    u32 tid_high = static_cast<u32>(title_id >> 32);
    if (tid_high != TID_HIGH_DLC) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
        rb.Push(Result(ErrCodes::InvalidTIDInList, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Usage));
        rb.PushMappedBuffer(content_requested_in);
        rb.PushMappedBuffer(content_info_out);
        return;
    }

    std::vector<u16_le> content_requested(content_count);
    content_requested_in.Read(content_requested.data(), 0, content_count * sizeof(u16));

    std::string tmd_path = GetTitleMetadataPath(media_type, title_id);

    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
        std::size_t write_offset = 0;
        // Get info for each content index requested
        for (std::size_t i = 0; i < content_count; i++) {
            if (content_requested[i] >= tmd.GetContentCount()) {
                LOG_ERROR(Service_AM,
                          "Attempted to get info for non-existent content index {:04x}.",
                          content_requested[i]);

                IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
                rb.Push<u32>(-1); // TODO(Steveice10): Find the right error code
                rb.PushMappedBuffer(content_requested_in);
                rb.PushMappedBuffer(content_info_out);
                return;
            }

            ContentInfo content_info = {};
            content_info.index = content_requested[i];
            content_info.type = tmd.GetContentTypeByIndex(content_requested[i]);
            content_info.content_id = tmd.GetContentIDByIndex(content_requested[i]);
            content_info.size = tmd.GetContentSizeByIndex(content_requested[i]);
            content_info.ownership =
                OWNERSHIP_OWNED; // TODO(Steveice10): Pull this from the ticket.

            if (FileUtil::Exists(GetTitleContentPath(media_type, title_id, content_requested[i]))) {
                content_info.ownership |= OWNERSHIP_DOWNLOADED;
            }

            content_info_out.Write(&content_info, write_offset, sizeof(ContentInfo));
            write_offset += sizeof(ContentInfo);
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(content_requested_in);
    rb.PushMappedBuffer(content_info_out);
}

void Module::Interface::ListDLCContentInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 content_count = rp.Pop<u32>();
    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u64 title_id = rp.Pop<u64>();
    u32 start_index = rp.Pop<u32>();
    auto& content_info_out = rp.PopMappedBuffer();

    // Validate that only DLC TIDs are passed in
    u32 tid_high = static_cast<u32>(title_id >> 32);
    if (tid_high != TID_HIGH_DLC) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
        rb.Push(Result(ErrCodes::InvalidTIDInList, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Usage));
        rb.Push<u32>(0);
        rb.PushMappedBuffer(content_info_out);
        return;
    }

    std::string tmd_path = GetTitleMetadataPath(media_type, title_id);

    u32 copied = 0;
    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
        u32 end_index =
            std::min(start_index + content_count, static_cast<u32>(tmd.GetContentCount()));
        std::size_t write_offset = 0;
        for (u32 i = start_index; i < end_index; i++) {
            ContentInfo content_info = {};
            content_info.index = static_cast<u16>(i);
            content_info.type = tmd.GetContentTypeByIndex(i);
            content_info.content_id = tmd.GetContentIDByIndex(i);
            content_info.size = tmd.GetContentSizeByIndex(i);
            content_info.ownership =
                OWNERSHIP_OWNED; // TODO(Steveice10): Pull this from the ticket.

            if (FileUtil::Exists(GetTitleContentPath(media_type, title_id, i))) {
                content_info.ownership |= OWNERSHIP_DOWNLOADED;
            }

            content_info_out.Write(&content_info, write_offset, sizeof(ContentInfo));
            write_offset += sizeof(ContentInfo);
            copied++;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push(copied);
    rb.PushMappedBuffer(content_info_out);
}

void Module::Interface::DeleteContents(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 media_type = rp.Pop<u8>();
    u64 title_id = rp.Pop<u64>();
    u32 content_count = rp.Pop<u32>();
    auto& content_ids_in = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(content_ids_in);
    LOG_WARNING(Service_AM, "(STUBBED) media_type={}, title_id=0x{:016x}, content_count={}",
                media_type, title_id, content_count);
}

void Module::Interface::GetProgramList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 count = rp.Pop<u32>();
    u8 media_type = rp.Pop<u8>();
    auto& title_ids_output = rp.PopMappedBuffer();

    if (media_type > 2) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
        rb.Push<u32>(-1); // TODO(shinyquagsire23): Find the right error code
        rb.Push<u32>(0);
        rb.PushMappedBuffer(title_ids_output);
        return;
    }

    u32 media_count = static_cast<u32>(am->am_title_list[media_type].size());
    u32 copied = std::min(media_count, count);

    title_ids_output.Write(am->am_title_list[media_type].data(), 0, copied * sizeof(u64));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push(copied);
    rb.PushMappedBuffer(title_ids_output);
}

Result GetTitleInfoFromList(std::span<const u64> title_id_list, Service::FS::MediaType media_type,
                            Kernel::MappedBuffer& title_info_out) {
    std::size_t write_offset = 0;
    for (u32 i = 0; i < title_id_list.size(); i++) {
        std::string tmd_path = GetTitleMetadataPath(media_type, title_id_list[i]);

        TitleInfo title_info = {};
        title_info.tid = title_id_list[i];

        FileSys::TitleMetadata tmd;
        if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
            // TODO(shinyquagsire23): This is the total size of all files this process owns,
            // including savefiles and other content. This comes close but is off.
            title_info.size = tmd.GetContentSizeByIndex(FileSys::TMDContentIndex::Main);
            title_info.version = tmd.GetTitleVersion();
            title_info.type = tmd.GetTitleType();
        } else {
            return Result(ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                          ErrorLevel::Permanent);
        }
        title_info_out.Write(&title_info, write_offset, sizeof(TitleInfo));
        write_offset += sizeof(TitleInfo);
    }

    return ResultSuccess;
}

void Module::Interface::GetProgramInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u32 title_count = rp.Pop<u32>();
    auto& title_id_list_buffer = rp.PopMappedBuffer();
    auto& title_info_out = rp.PopMappedBuffer();

    std::vector<u64> title_id_list(title_count);
    title_id_list_buffer.Read(title_id_list.data(), 0, title_count * sizeof(u64));

    Result result = GetTitleInfoFromList(title_id_list, media_type, title_info_out);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(title_id_list_buffer);
    rb.PushMappedBuffer(title_info_out);
}

void Module::Interface::GetProgramInfosIgnorePlatform(Kernel::HLERequestContext& ctx) {
    GetProgramInfos(ctx);
}

void Module::Interface::DeleteUserProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto media_type = rp.PopEnum<FS::MediaType>();
    u64 title_id = rp.Pop<u64>();
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    u16 category = static_cast<u16>((title_id >> 32) & 0xFFFF);
    u8 variation = static_cast<u8>(title_id & 0xFF);
    if (category & CATEGORY_SYSTEM || category & CATEGORY_DLP || variation & VARIATION_SYSTEM) {
        LOG_ERROR(Service_AM, "Trying to uninstall system app");
        rb.Push(Result(ErrCodes::TryingToUninstallSystemApp, ErrorModule::AM,
                       ErrorSummary::InvalidArgument, ErrorLevel::Usage));
        return;
    }
    LOG_INFO(Service_AM, "Deleting title 0x{:016x}", title_id);
    std::string path = GetTitlePath(media_type, title_id);
    if (!FileUtil::Exists(path)) {
        rb.Push(Result(ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                       ErrorLevel::Permanent));
        LOG_ERROR(Service_AM, "Title not found");
        return;
    }
    bool success = FileUtil::DeleteDirRecursively(path);
    am->ScanForAllTitles();
    rb.Push(ResultSuccess);
    if (!success)
        LOG_ERROR(Service_AM, "FileUtil::DeleteDirRecursively unexpectedly failed");
}

void Module::Interface::GetProductCode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    FS::MediaType media_type = rp.PopEnum<FS::MediaType>();
    u64 title_id = rp.Pop<u64>();
    std::string path = GetTitleContentPath(media_type, title_id);

    if (!FileUtil::Exists(path)) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                       ErrorLevel::Permanent));
    } else {
        struct ProductCode {
            u8 code[0x10];
        };

        ProductCode product_code;

        IPC::RequestBuilder rb = rp.MakeBuilder(6, 0);
        FileSys::NCCHContainer ncch(path);
        ncch.Load();
        std::memcpy(&product_code.code, &ncch.ncch_header.product_code, 0x10);
        rb.Push(ResultSuccess);
        rb.PushRaw(product_code);
    }
}

void Module::Interface::GetDLCTitleInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u32 title_count = rp.Pop<u32>();
    auto& title_id_list_buffer = rp.PopMappedBuffer();
    auto& title_info_out = rp.PopMappedBuffer();

    std::vector<u64> title_id_list(title_count);
    title_id_list_buffer.Read(title_id_list.data(), 0, title_count * sizeof(u64));

    Result result = ResultSuccess;

    // Validate that DLC TIDs were passed in
    for (u32 i = 0; i < title_count; i++) {
        u32 tid_high = static_cast<u32>(title_id_list[i] >> 32);
        if (tid_high != TID_HIGH_DLC) {
            result = Result(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                            ErrorSummary::InvalidArgument, ErrorLevel::Usage);
            break;
        }
    }

    if (result.IsSuccess()) {
        result = GetTitleInfoFromList(title_id_list, media_type, title_info_out);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(title_id_list_buffer);
    rb.PushMappedBuffer(title_info_out);
}

void Module::Interface::GetPatchTitleInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u32 title_count = rp.Pop<u32>();
    auto& title_id_list_buffer = rp.PopMappedBuffer();
    auto& title_info_out = rp.PopMappedBuffer();

    std::vector<u64> title_id_list(title_count);
    title_id_list_buffer.Read(title_id_list.data(), 0, title_count * sizeof(u64));

    Result result = ResultSuccess;

    // Validate that update TIDs were passed in
    for (u32 i = 0; i < title_count; i++) {
        u32 tid_high = static_cast<u32>(title_id_list[i] >> 32);
        if (tid_high != TID_HIGH_UPDATE) {
            result = Result(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                            ErrorSummary::InvalidArgument, ErrorLevel::Usage);
            break;
        }
    }

    if (result.IsSuccess()) {
        result = GetTitleInfoFromList(title_id_list, media_type, title_info_out);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(title_id_list_buffer);
    rb.PushMappedBuffer(title_info_out);
}

void Module::Interface::ListDataTitleTicketInfos(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 ticket_count = rp.Pop<u32>();
    u64 title_id = rp.Pop<u64>();
    u32 start_index = rp.Pop<u32>();
    auto& ticket_info_out = rp.PopMappedBuffer();

    std::size_t write_offset = 0;
    for (u32 i = 0; i < ticket_count; i++) {
        TicketInfo ticket_info = {};
        ticket_info.title_id = title_id;
        ticket_info.version = 0; // TODO
        ticket_info.size = 0;    // TODO

        ticket_info_out.Write(&ticket_info, write_offset, sizeof(TicketInfo));
        write_offset += sizeof(TicketInfo);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push(ticket_count);
    rb.PushMappedBuffer(ticket_info_out);

    LOG_WARNING(Service_AM,
                "(STUBBED) ticket_count=0x{:08X}, title_id=0x{:016x}, start_index=0x{:08X}",
                ticket_count, title_id, start_index);
}

void Module::Interface::GetDLCContentInfoCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u64 title_id = rp.Pop<u64>();

    // Validate that only DLC TIDs are passed in
    u32 tid_high = static_cast<u32>(title_id >> 32);
    if (tid_high != TID_HIGH_DLC) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
        rb.Push(Result(ErrCodes::InvalidTID, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Usage));
        rb.Push<u32>(0);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess); // No error

    std::string tmd_path = GetTitleMetadataPath(media_type, title_id);

    FileSys::TitleMetadata tmd;
    if (tmd.Load(tmd_path) == Loader::ResultStatus::Success) {
        rb.Push<u32>(static_cast<u32>(tmd.GetContentCount()));
    } else {
        rb.Push<u32>(1); // Number of content infos plus one
        LOG_WARNING(Service_AM, "(STUBBED) called media_type={}, title_id=0x{:016x}", media_type,
                    title_id);
    }
}

void Module::Interface::DeleteTicket(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u64 title_id = rp.Pop<u64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
    LOG_WARNING(Service_AM, "(STUBBED) called title_id=0x{:016x}", title_id);
}

void Module::Interface::GetNumTickets(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 ticket_count = 0;
    for (const auto& title_list : am->am_title_list) {
        ticket_count += static_cast<u32>(title_list.size());
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(ticket_count);
    LOG_WARNING(Service_AM, "(STUBBED) called ticket_count=0x{:08x}", ticket_count);
}

void Module::Interface::GetTicketList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 ticket_list_count = rp.Pop<u32>();
    u32 ticket_index = rp.Pop<u32>();
    auto& ticket_tids_out = rp.PopMappedBuffer();

    u32 tickets_written = 0;
    for (const auto& title_list : am->am_title_list) {
        const auto tickets_to_write =
            std::min(static_cast<u32>(title_list.size()), ticket_list_count - tickets_written);
        ticket_tids_out.Write(title_list.data(), tickets_written * sizeof(u64),
                              tickets_to_write * sizeof(u64));
        tickets_written += tickets_to_write;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push(tickets_written);
    rb.PushMappedBuffer(ticket_tids_out);
    LOG_WARNING(Service_AM, "(STUBBED) ticket_list_count=0x{:08x}, ticket_index=0x{:08x}",
                ticket_list_count, ticket_index);
}

void Module::Interface::NeedsCleanup(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto media_type = rp.Pop<u8>();

    LOG_DEBUG(Service_AM, "(STUBBED) media_type=0x{:02x}", media_type);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<bool>(false);
}

void Module::Interface::DoCleanup(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto media_type = rp.Pop<u8>();

    LOG_DEBUG(Service_AM, "(STUBBED) called, media_type={:#02x}", media_type);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::QueryAvailableTitleDatabase(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 media_type = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess); // No error
    rb.Push(true);

    LOG_WARNING(Service_AM, "(STUBBED) media_type={}", media_type);
}

void Module::Interface::GetPersonalizedTicketInfoList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] u32 ticket_count = rp.Pop<u32>();
    [[maybe_unused]] auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess); // No error
    rb.Push(0);

    LOG_WARNING(Service_AM, "(STUBBED) called, ticket_count={}", ticket_count);
}

void Module::Interface::GetNumImportTitleContextsFiltered(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 media_type = rp.Pop<u8>();
    u8 filter = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess); // No error
    rb.Push(0);

    LOG_WARNING(Service_AM, "(STUBBED) called, media_type={}, filter={}", media_type, filter);
}

void Module::Interface::GetImportTitleContextListFiltered(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const u32 count = rp.Pop<u32>();
    const u8 media_type = rp.Pop<u8>();
    const u8 filter = rp.Pop<u8>();
    auto& buffer = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess); // No error
    rb.Push(0);
    rb.PushMappedBuffer(buffer);

    LOG_WARNING(Service_AM, "(STUBBED) called, media_type={}, filter={}", media_type, filter);
}

void Module::Interface::CheckContentRights(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u64 tid = rp.Pop<u64>();
    u16 content_index = rp.Pop<u16>();

    // TODO(shinyquagsire23): Read tickets for this instead?
    bool has_rights =
        FileUtil::Exists(GetTitleContentPath(Service::FS::MediaType::NAND, tid, content_index)) ||
        FileUtil::Exists(GetTitleContentPath(Service::FS::MediaType::SDMC, tid, content_index));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess); // No error
    rb.Push(has_rights);

    LOG_WARNING(Service_AM, "(STUBBED) tid={:016x}, content_index={}", tid, content_index);
}

void Module::Interface::CheckContentRightsIgnorePlatform(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u64 tid = rp.Pop<u64>();
    u16 content_index = rp.Pop<u16>();

    // TODO(shinyquagsire23): Read tickets for this instead?
    bool has_rights =
        FileUtil::Exists(GetTitleContentPath(Service::FS::MediaType::SDMC, tid, content_index));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess); // No error
    rb.Push(has_rights);

    LOG_WARNING(Service_AM, "(STUBBED) tid={:016x}, content_index={}", tid, content_index);
}

void Module::Interface::BeginImportProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());

    if (am->cia_installing) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::CIACurrentlyInstalling, ErrorModule::AM,
                       ErrorSummary::InvalidState, ErrorLevel::Permanent));
        return;
    }

    // Create our CIAFile handle for the app to write to, and while the app writes
    // Citra will store contents out to sdmc/nand
    const FileSys::Path cia_path = {};
    auto file = std::make_shared<Service::FS::File>(
        am->system.Kernel(), std::make_unique<CIAFile>(am->system, media_type), cia_path);

    am->cia_installing = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess); // No error
    rb.PushCopyObjects(file->Connect());

    LOG_WARNING(Service_AM, "(STUBBED) media_type={}", media_type);
}

void Module::Interface::BeginImportProgramTemporarily(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    if (am->cia_installing) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::CIACurrentlyInstalling, ErrorModule::AM,
                       ErrorSummary::InvalidState, ErrorLevel::Permanent));
        return;
    }

    // Note: This function should register the title in the temp_i.db database, but we can get away
    // with not doing that because we traverse the file system to detect installed titles.
    // Create our CIAFile handle for the app to write to, and while the app writes Citra will store
    // contents out to sdmc/nand
    const FileSys::Path cia_path = {};
    auto file = std::make_shared<Service::FS::File>(
        am->system.Kernel(), std::make_unique<CIAFile>(am->system, FS::MediaType::NAND), cia_path);

    am->cia_installing = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess); // No error
    rb.PushCopyObjects(file->Connect());

    LOG_WARNING(Service_AM, "(STUBBED)");
}

void Module::Interface::EndImportProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const auto cia = rp.PopObject<Kernel::ClientSession>();

    am->ScanForAllTitles();

    am->cia_installing = false;
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::EndImportProgramWithoutCommit(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const auto cia = rp.PopObject<Kernel::ClientSession>();

    // Note: This function is basically a no-op for us since we don't use title.db or ticket.db
    // files to keep track of installed titles.
    am->ScanForAllTitles();

    am->cia_installing = false;
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::CommitImportPrograms(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const auto media_type = static_cast<FS::MediaType>(rp.Pop<u8>());
    [[maybe_unused]] const u32 title_count = rp.Pop<u32>();
    [[maybe_unused]] const u8 database = rp.Pop<u8>();
    const auto buffer = rp.PopMappedBuffer();

    // Note: This function is basically a no-op for us since we don't use title.db or ticket.db
    // files to keep track of installed titles.
    am->ScanForAllTitles();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(buffer);
}

/// Wraps all File operations to allow adding an offset to them.
class AMFileWrapper : public FileSys::FileBackend {
public:
    AMFileWrapper(std::shared_ptr<Service::FS::File> file, std::size_t offset, std::size_t size)
        : file(std::move(file)), file_offset(offset), file_size(size) {}

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override {
        return file->backend->Read(offset + file_offset, length, buffer);
    }

    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush,
                                 const u8* buffer) override {
        return file->backend->Write(offset + file_offset, length, flush, buffer);
    }

    u64 GetSize() const override {
        return file_size;
    }
    bool SetSize(u64 size) const override {
        return false;
    }
    bool Close() const override {
        return false;
    }
    void Flush() const override {}

private:
    std::shared_ptr<Service::FS::File> file;
    std::size_t file_offset;
    std::size_t file_size;
};

ResultVal<std::unique_ptr<AMFileWrapper>> GetFileFromSession(
    std::shared_ptr<Kernel::ClientSession> file_session) {
    // Step up the chain from ClientSession->ServerSession and then
    // cast to File. For AM on 3DS, invalid handles actually hang the system.

    if (file_session->parent == nullptr) {
        LOG_WARNING(Service_AM, "Invalid file handle!");
        return Kernel::ResultInvalidHandle;
    }

    std::shared_ptr<Kernel::ServerSession> server =
        Kernel::SharedFrom(file_session->parent->server);
    if (server == nullptr) {
        LOG_WARNING(Service_AM, "File handle ServerSession disconnected!");
        return Kernel::ResultSessionClosed;
    }

    if (server->hle_handler != nullptr) {
        auto file = std::dynamic_pointer_cast<Service::FS::File>(server->hle_handler);

        // TODO(shinyquagsire23): This requires RTTI, use service calls directly instead?
        if (file != nullptr) {
            // Grab the session file offset in case we were given a subfile opened with
            // File::OpenSubFile
            std::size_t offset = file->GetSessionFileOffset(server);
            std::size_t size = file->GetSessionFileSize(server);
            return std::make_unique<AMFileWrapper>(file, offset, size);
        }

        LOG_ERROR(Service_AM, "Failed to cast handle to FSFile!");
        return Kernel::ResultInvalidHandle;
    }

    // Probably the best bet if someone is LLEing the fs service is to just have them LLE AM
    // while they're at it, so not implemented.
    LOG_ERROR(Service_AM, "Given file handle does not have an HLE handler!");
    return Kernel::ResultNotImplemented;
}

void Module::Interface::GetProgramInfoFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const auto media_type = static_cast<FS::MediaType>(rp.Pop<u8>());
    auto cia = rp.PopObject<Kernel::ClientSession>();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(file_res.Code());
        return;
    }

    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        return;
    }

    const FileSys::TitleMetadata& tmd = container.GetTitleMetadata();
    TitleInfo title_info = {};
    container.Print();

    // TODO(shinyquagsire23): Sizes allegedly depend on the mediatype, and will double
    // on some mediatypes. Since this is more of a required install size we'll report
    // what Citra needs, but it would be good to be more accurate here.
    title_info.tid = tmd.GetTitleID();
    title_info.size = tmd.GetContentSizeByIndex(FileSys::TMDContentIndex::Main);
    title_info.version = tmd.GetTitleVersion();
    title_info.type = tmd.GetTitleType();

    IPC::RequestBuilder rb = rp.MakeBuilder(8, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw<TitleInfo>(title_info);
}

void Module::Interface::GetSystemMenuDataFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto cia = rp.PopObject<Kernel::ClientSession>();
    auto& output_buffer = rp.PopMappedBuffer();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(file_res.Code());
        rb.PushMappedBuffer(output_buffer);
        return;
    }

    std::size_t output_buffer_size = std::min(output_buffer.GetSize(), sizeof(Loader::SMDH));

    auto file = std::move(file_res.Unwrap());
    FileSys::CIAContainer container;
    if (container.Load(*file) != Loader::ResultStatus::Success) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        rb.PushMappedBuffer(output_buffer);
        return;
    }
    std::vector<u8> temp(output_buffer_size);

    //  Read from the Meta offset + 0x400 for the 0x36C0-large SMDH
    auto read_result = file->Read(container.GetMetadataOffset() + FileSys::CIA_METADATA_SIZE,
                                  temp.size(), temp.data());
    if (read_result.Failed() || *read_result != temp.size()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        rb.PushMappedBuffer(output_buffer);
        return;
    }

    output_buffer.Write(temp.data(), 0, temp.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(output_buffer);
}

void Module::Interface::GetDependencyListFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto cia = rp.PopObject<Kernel::ClientSession>();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(file_res.Code());
        return;
    }

    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        return;
    }

    std::vector<u8> buffer(FileSys::CIA_DEPENDENCY_SIZE);
    std::memcpy(buffer.data(), container.GetDependencies().data(), buffer.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushStaticBuffer(std::move(buffer), 0);
}

void Module::Interface::GetTransferSizeFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto cia = rp.PopObject<Kernel::ClientSession>();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(file_res.Code());
        return;
    }

    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push(container.GetMetadataOffset());
}

void Module::Interface::GetCoreVersionFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto cia = rp.PopObject<Kernel::ClientSession>();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(file_res.Code());
        return;
    }

    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(container.GetCoreVersion());
}

void Module::Interface::GetRequiredSizeFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const auto media_type = static_cast<FS::MediaType>(rp.Pop<u8>());
    auto cia = rp.PopObject<Kernel::ClientSession>();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(file_res.Code());
        return;
    }

    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        return;
    }

    // TODO(shinyquagsire23): Sizes allegedly depend on the mediatype, and will double
    // on some mediatypes. Since this is more of a required install size we'll report
    // what Citra needs, but it would be good to be more accurate here.
    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push(container.GetTitleMetadata().GetContentSizeByIndex(FileSys::TMDContentIndex::Main));
}

Result UninstallProgram(const FS::MediaType media_type, const u64 title_id) {
    // Use the content folder so we don't delete the user's save data.
    const auto path = GetTitlePath(media_type, title_id) + "content/";
    if (!FileUtil::Exists(path)) {
        return {ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                ErrorLevel::Permanent};
    }
    if (!FileUtil::DeleteDirRecursively(path)) {
        // TODO: Determine the right error code for this.
        return {ErrorDescription::NotFound, ErrorModule::AM, ErrorSummary::InvalidState,
                ErrorLevel::Permanent};
    }
    return ResultSuccess;
}

void Module::Interface::DeleteProgram(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto media_type = rp.PopEnum<FS::MediaType>();
    const auto title_id = rp.Pop<u64>();

    LOG_INFO(Service_AM, "called, title={:016x}", title_id);

    const auto result = UninstallProgram(media_type, title_id);
    am->ScanForAllTitles();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);
}

void Module::Interface::GetSystemUpdaterMutex(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(am->system_updater_mutex);
}

void Module::Interface::GetMetaSizeFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    auto cia = rp.PopObject<Kernel::ClientSession>();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(file_res.Code());
        return;
    }

    FileSys::CIAContainer container;
    if (container.Load(*file_res.Unwrap()) != Loader::ResultStatus::Success) {

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(container.GetMetadataSize());
}

void Module::Interface::GetMetaDataFromCia(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 output_size = rp.Pop<u32>();
    auto cia = rp.PopObject<Kernel::ClientSession>();
    auto& output_buffer = rp.PopMappedBuffer();

    auto file_res = GetFileFromSession(cia);
    if (!file_res.Succeeded()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(file_res.Code());
        rb.PushMappedBuffer(output_buffer);
        return;
    }
    // Don't write beyond the actual static buffer size.
    output_size = std::min(static_cast<u32>(output_buffer.GetSize()), output_size);

    auto file = std::move(file_res.Unwrap());
    FileSys::CIAContainer container;
    if (container.Load(*file) != Loader::ResultStatus::Success) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        rb.PushMappedBuffer(output_buffer);
        return;
    }

    //  Read from the Meta offset for the specified size
    std::vector<u8> temp(output_size);
    auto read_result = file->Read(container.GetMetadataOffset(), output_size, temp.data());
    if (read_result.Failed() || *read_result != output_size) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(Result(ErrCodes::InvalidCIAHeader, ErrorModule::AM, ErrorSummary::InvalidArgument,
                       ErrorLevel::Permanent));
        return;
    }

    output_buffer.Write(temp.data(), 0, output_size);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(output_buffer);
}

void Module::Interface::BeginImportTicket(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    // Create our TicketFile handle for the app to write to
    auto file = std::make_shared<Service::FS::File>(
        am->system.Kernel(), std::make_unique<TicketFile>(), FileSys::Path{});

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess); // No error
    rb.PushCopyObjects(file->Connect());

    LOG_WARNING(Service_AM, "(STUBBED) called");
}

void Module::Interface::EndImportTicket(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    [[maybe_unused]] const auto ticket = rp.PopObject<Kernel::ClientSession>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_AM, "(STUBBED) called");
}

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int) {
    ar & cia_installing;
    ar & am_title_list;
    ar & system_updater_mutex;
}
SERIALIZE_IMPL(Module)

Module::Module(Core::System& system) : system(system) {
    ScanForAllTitles();
    system_updater_mutex = system.Kernel().CreateMutex(false, "AM::SystemUpdaterMutex");
}

Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto am = std::make_shared<Module>(system);
    std::make_shared<AM_APP>(am)->InstallAsService(service_manager);
    std::make_shared<AM_NET>(am)->InstallAsService(service_manager);
    std::make_shared<AM_SYS>(am)->InstallAsService(service_manager);
    std::make_shared<AM_U>(am)->InstallAsService(service_manager);
}

} // namespace Service::AM
