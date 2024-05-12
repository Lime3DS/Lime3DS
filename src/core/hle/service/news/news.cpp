// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/assert.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/result.h"
#include "core/hle/service/fs/fs_user.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/news/news_s.h"
#include "core/hle/service/news/news_u.h"
#include "core/hle/service/service.h"

SERVICE_CONSTRUCT_IMPL(Service::NEWS::Module)

namespace Service::NEWS {

namespace ErrCodes {
enum {
    /// This error is returned if either the NewsDB header or the header for a notification ID is
    /// invalid
    InvalidHeader = 5,
};
}

constexpr Result ErrorInvalidHeader = // 0xC8A12805
    Result(ErrCodes::InvalidHeader, ErrorModule::News, ErrorSummary::InvalidState,
           ErrorLevel::Status);

constexpr std::array<u8, 8> news_system_savedata_id{
    0x00, 0x00, 0x00, 0x00, 0x35, 0x00, 0x01, 0x00,
};

template <class Archive>
void Module::serialize(Archive& ar, const unsigned int) {
    ar & db;
    ar & notification_ids;
    ar & automatic_sync_flag;
    ar & news_system_save_data_archive;
}
SERIALIZE_IMPL(Module)

void Module::Interface::AddNotificationImpl(Kernel::HLERequestContext& ctx, bool news_s) {
    IPC::RequestParser rp(ctx);
    const u32 header_size = rp.Pop<u32>();
    const u32 message_size = rp.Pop<u32>();
    const u32 image_size = rp.Pop<u32>();

    u32 process_id;
    if (!news_s) {
        process_id = rp.PopPID();
        LOG_INFO(Service_NEWS,
                 "called header_size=0x{:x}, message_size=0x{:x}, image_size=0x{:x}, process_id={}",
                 header_size, message_size, image_size, process_id);
    } else {
        LOG_INFO(Service_NEWS, "called header_size=0x{:x}, message_size=0x{:x}, image_size=0x{:x}",
                 header_size, message_size, image_size);
    }

    auto header_buffer = rp.PopMappedBuffer();
    auto message_buffer = rp.PopMappedBuffer();
    auto image_buffer = rp.PopMappedBuffer();

    NotificationHeader header{};
    header_buffer.Read(&header, 0,
                       std::min(sizeof(NotificationHeader), static_cast<std::size_t>(header_size)));

    std::vector<u8> message(message_size);
    message_buffer.Read(message.data(), 0, message.size());

    std::vector<u8> image(image_size);
    image_buffer.Read(image.data(), 0, image.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 6);
    SCOPE_EXIT({
        rb.PushMappedBuffer(header_buffer);
        rb.PushMappedBuffer(message_buffer);
        rb.PushMappedBuffer(image_buffer);
    });

    if (!news_s) {
        // Set the program_id using the input process ID
        auto fs_user = news->system.ServiceManager().GetService<Service::FS::FS_USER>("fs:USER");
        ASSERT_MSG(fs_user != nullptr, "fs:USER service is missing.");

        auto program_info_result = fs_user->GetProgramLaunchInfo(process_id);
        if (program_info_result.Failed()) {
            rb.Push(program_info_result.Code());
            return;
        }

        header.program_id = program_info_result.Unwrap().program_id;

        // The date_time is set by the sysmodule on news:u requests
        auto& share_page = news->system.Kernel().GetSharedPageHandler();
        header.date_time = share_page.GetSystemTimeSince2000();
    }

    const auto save_result = news->SaveNotification(&header, header_size, message, image);
    if (R_FAILED(save_result)) {
        rb.Push(save_result);
        return;
    }

    // Mark the DB header new notification flag
    if ((news->db.header.flags & 1) == 0) {
        news->db.header.flags |= 1;
        const auto db_result = news->SaveNewsDBSavedata();
        if (R_FAILED(db_result)) {
            rb.Push(db_result);
            return;
        }
    }

    rb.Push(ResultSuccess);
}

void Module::Interface::AddNotification(Kernel::HLERequestContext& ctx) {
    AddNotificationImpl(ctx, false);
}

void Module::Interface::AddNotificationSystem(Kernel::HLERequestContext& ctx) {
    AddNotificationImpl(ctx, true);
}

void Module::Interface::ResetNotifications(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NEWS, "called");

    // Cleanup the sorted notification IDs
    for (u32 i = 0; i < MAX_NOTIFICATIONS; ++i) {
        news->notification_ids[i] = i;
    }

    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    FileSys::Path archive_path(news_system_savedata_id);

    // Format the SystemSaveData archive 0x00010035
    systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0, 0, 0);

    news->news_system_save_data_archive = systemsavedata_factory.Open(archive_path, 0).Unwrap();

    // NOTE: The original sysmodule doesn't clear the News DB in memory

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::GetTotalNotifications(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NEWS, "called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(news->GetTotalNotifications()));
}

void Module::Interface::SetNewsDBHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto input_buffer = rp.PopMappedBuffer();

    LOG_INFO(Service_NEWS, "called size=0x{:x}", size);

    NewsDBHeader header{};
    input_buffer.Read(&header, 0, std::min(sizeof(NewsDBHeader), static_cast<std::size_t>(size)));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(news->SetNewsDBHeader(&header, size));
    rb.PushMappedBuffer(input_buffer);
}

void Module::Interface::SetNotificationHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 notification_index = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto input_buffer = rp.PopMappedBuffer();

    LOG_INFO(Service_NEWS, "called notification_index={}, size=0x{:x}", notification_index, size);

    NotificationHeader header{};
    input_buffer.Read(&header, 0,
                      std::min(sizeof(NotificationHeader), static_cast<std::size_t>(size)));

    const auto result = news->SetNotificationHeader(notification_index, &header, size);

    // TODO(DaniElectra): If flag_boss == 1, the original sysmodule updates the optout status of the
    // source program on SpotPass with the boss:P command 0x00040600C0 (possibly named
    // SetOptoutFlagPrivileged?) using the program_id and flag_optout as parameter

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(result);
    rb.PushMappedBuffer(input_buffer);
}

void Module::Interface::SetNotificationMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 notification_index = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto input_buffer = rp.PopMappedBuffer();

    LOG_INFO(Service_NEWS, "called notification_index={}, size=0x{:x}", notification_index, size);

    std::vector<u8> data(size);
    input_buffer.Read(data.data(), 0, data.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(news->SetNotificationMessage(notification_index, data));
    rb.PushMappedBuffer(input_buffer);
}

void Module::Interface::SetNotificationImage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 notification_index = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto input_buffer = rp.PopMappedBuffer();

    LOG_INFO(Service_NEWS, "called notification_index={}, size=0x{:x}", notification_index, size);

    std::vector<u8> data(size);
    input_buffer.Read(data.data(), 0, data.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(news->SetNotificationImage(notification_index, data));
    rb.PushMappedBuffer(input_buffer);
}

void Module::Interface::GetNewsDBHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 size = rp.Pop<u32>();
    auto output_buffer = rp.PopMappedBuffer();

    LOG_INFO(Service_NEWS, "called size=0x{:x}", size);

    NewsDBHeader header{};
    const auto result = news->GetNewsDBHeader(&header, size);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);

    if (result.Failed()) {
        rb.Push(result.Code());
        rb.Push<u32>(0);
    } else {
        const auto copied_size = result.Unwrap();
        output_buffer.Write(&header, 0, copied_size);

        rb.Push(ResultSuccess);
        rb.Push<u32>(static_cast<u32>(copied_size));
    }

    rb.PushMappedBuffer(output_buffer);
}

void Module::Interface::GetNotificationHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 notification_index = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto output_buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_NEWS, "called notification_index={}, size=0x{:x}", notification_index, size);

    NotificationHeader header{};
    const auto result = news->GetNotificationHeader(notification_index, &header, size);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    SCOPE_EXIT({ rb.PushMappedBuffer(output_buffer); });

    if (result.Failed()) {
        rb.Push(result.Code());
        rb.Push<u32>(0);
        return;
    }

    // TODO(DaniElectra): If flag_boss == 1, the original sysmodule updates the optout flag of the
    // header with the result of boss:P command 0x0004070080 (possibly named
    // GetOptoutFlagPrivileged?) using the program_id as parameter

    const auto copied_size = result.Unwrap();
    output_buffer.Write(&header, 0, copied_size);

    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(copied_size));
}

void Module::Interface::GetNotificationMessage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 notification_index = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto output_buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_NEWS, "called notification_index={}, size=0x{:x}", notification_index, size);

    std::vector<u8> message(size);
    const auto result = news->GetNotificationMessage(notification_index, message);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    SCOPE_EXIT({ rb.PushMappedBuffer(output_buffer); });

    if (result.Failed()) {
        rb.Push(result.Code());
        rb.Push<u32>(0);
        return;
    }

    const auto copied_size = result.Unwrap();
    output_buffer.Write(message.data(), 0, copied_size);

    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(copied_size));
}

void Module::Interface::GetNotificationImage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 notification_index = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto output_buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_NEWS, "called notification_index={}, size=0x{:x}", notification_index, size);

    std::vector<u8> image(size);
    const auto result = news->GetNotificationImage(notification_index, image);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    SCOPE_EXIT({ rb.PushMappedBuffer(output_buffer); });

    if (result.Failed()) {
        rb.Push(result.Code());
        rb.Push<u32>(0);
        return;
    }

    const auto copied_size = result.Unwrap();
    output_buffer.Write(image.data(), 0, copied_size);

    rb.Push(ResultSuccess);
    rb.Push<u32>(static_cast<u32>(copied_size));
}

void Module::Interface::SetAutomaticSyncFlag(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u8 flag = rp.Pop<u8>();

    LOG_INFO(Service_NEWS, "called flag=0x{:x}", flag);

    news->automatic_sync_flag = flag;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::SetNotificationHeaderOther(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 notification_index = rp.Pop<u32>();
    const u32 size = rp.Pop<u32>();
    auto output_buffer = rp.PopMappedBuffer();

    LOG_INFO(Service_NEWS, "called notification_index={}, size=0x{:x}", notification_index, size);

    NotificationHeader header{};
    output_buffer.Read(&header, 0,
                       std::min(sizeof(NotificationHeader), static_cast<std::size_t>(size)));

    const auto result = news->SetNotificationHeaderOther(notification_index, &header, size);

    // TODO(DaniElectra): If flag_boss == 1, the original sysmodule updates the optout status of the
    // source program on SpotPass with the boss:P command 0x00040600C0 (possibly named
    // SetOptoutFlagPrivileged?) using the program_id and flag_optout as parameter

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(result);
    rb.PushMappedBuffer(output_buffer);
}

void Module::Interface::WriteNewsDBSavedata(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_INFO(Service_NEWS, "called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(news->SaveNewsDBSavedata());
}

void Module::Interface::GetTotalArrivedNotifications(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_WARNING(Service_NEWS, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(0); // Total number of pending BOSS notifications to be synced
}

std::size_t Module::GetTotalNotifications() {
    return std::count_if(
        notification_ids.begin(), notification_ids.end(),
        [this](const u32 notification_id) { return db.notifications[notification_id].IsValid(); });
}

ResultVal<std::size_t> Module::GetNewsDBHeader(NewsDBHeader* header, const std::size_t size) {
    if (!db.header.IsValid()) {
        return ErrorInvalidHeader;
    }

    const std::size_t copy_size = std::min(sizeof(NewsDBHeader), size);
    std::memcpy(header, &db.header, copy_size);
    return copy_size;
}

ResultVal<std::size_t> Module::GetNotificationHeader(const u32 notification_index,
                                                     NotificationHeader* header,
                                                     const std::size_t size) {
    if (!db.header.IsValid()) {
        return ErrorInvalidHeader;
    }

    if (notification_index >= MAX_NOTIFICATIONS) {
        return ErrorInvalidHeader;
    }

    const u32 notification_id = notification_ids[notification_index];
    if (!db.notifications[notification_id].IsValid()) {
        return ErrorInvalidHeader;
    }

    const std::size_t copy_size = std::min(sizeof(NotificationHeader), size);
    std::memcpy(header, &db.notifications[notification_id], copy_size);
    return copy_size;
}

ResultVal<std::size_t> Module::GetNotificationMessage(const u32 notification_index,
                                                      std::span<u8> message) {
    if (!db.header.IsValid()) {
        return ErrorInvalidHeader;
    }

    if (notification_index >= MAX_NOTIFICATIONS) {
        return ErrorInvalidHeader;
    }

    const u32 notification_id = notification_ids[notification_index];
    if (!db.notifications[notification_id].IsValid()) {
        return ErrorInvalidHeader;
    }

    std::string message_file = fmt::format("/news{:03d}.txt", notification_id);
    const auto result = LoadFileFromSavedata(message_file, message);
    if (result.Failed()) {
        return result.Code();
    }

    return result.Unwrap();
}

ResultVal<std::size_t> Module::GetNotificationImage(const u32 notification_index,
                                                    std::span<u8> image) {
    if (!db.header.IsValid()) {
        return ErrorInvalidHeader;
    }

    if (notification_index >= MAX_NOTIFICATIONS) {
        return ErrorInvalidHeader;
    }

    const u32 notification_id = notification_ids[notification_index];
    if (!db.notifications[notification_id].IsValid()) {
        return ErrorInvalidHeader;
    }

    std::string image_file = fmt::format("/news{:03d}.mpo", notification_id);
    const auto result = LoadFileFromSavedata(image_file, image);
    if (result.Failed()) {
        return result.Code();
    }

    return result.Unwrap();
}

Result Module::SetNewsDBHeader(const NewsDBHeader* header, const std::size_t size) {
    const std::size_t copy_size = std::min(sizeof(NewsDBHeader), static_cast<std::size_t>(size));
    std::memcpy(&db.header, header, copy_size);
    return SaveNewsDBSavedata();
}

Result Module::SetNotificationHeader(const u32 notification_index, const NotificationHeader* header,
                                     const std::size_t size) {
    if (notification_index >= MAX_NOTIFICATIONS) {
        return ErrorInvalidHeader;
    }

    const u32 notification_id = notification_ids[notification_index];
    const std::size_t copy_size = std::min(sizeof(NotificationHeader), size);
    std::memcpy(&db.notifications[notification_id], header, copy_size);
    return SaveNewsDBSavedata();
}

Result Module::SetNotificationHeaderOther(const u32 notification_index,
                                          const NotificationHeader* header,
                                          const std::size_t size) {
    if (notification_index >= MAX_NOTIFICATIONS) {
        return ErrorInvalidHeader;
    }

    const u32 notification_id = notification_ids[notification_index];
    const std::size_t copy_size = std::min(sizeof(NotificationHeader), size);
    std::memcpy(&db.notifications[notification_id], header, copy_size);
    return ResultSuccess;
}

Result Module::SetNotificationMessage(const u32 notification_index, std::span<const u8> message) {
    if (notification_index >= MAX_NOTIFICATIONS) {
        return ErrorInvalidHeader;
    }

    const u32 notification_id = notification_ids[notification_index];
    const std::string message_file = fmt::format("/news{:03d}.txt", notification_id);
    return SaveFileToSavedata(message_file, message);
}

Result Module::SetNotificationImage(const u32 notification_index, std::span<const u8> image) {
    if (notification_index >= MAX_NOTIFICATIONS) {
        return ErrorInvalidHeader;
    }

    const u32 notification_id = notification_ids[notification_index];
    const std::string image_file = fmt::format("/news{:03d}.mpo", notification_id);
    return SaveFileToSavedata(image_file, image);
}

Result Module::SaveNotification(const NotificationHeader* header, const std::size_t header_size,
                                std::span<const u8> message, std::span<const u8> image) {
    if (!db.header.IsValid()) {
        return ErrorInvalidHeader;
    }

    if (!header->IsValid()) {
        return ErrorInvalidHeader;
    }

    u32 notification_count = static_cast<u32>(GetTotalNotifications());

    // If we have reached the limit of 100 notifications, delete the oldest one
    if (notification_count >= MAX_NOTIFICATIONS) {
        LOG_WARNING(Service_NEWS,
                    "Notification limit has been reached. Deleting oldest notification ID: {}",
                    notification_ids[0]);
        R_TRY(DeleteNotification(notification_ids[0]));

        notification_count--;
    }

    // Check if there is enough space for storing the new notification data. The header is already
    // allocated with the News DB
    const u64 needed_space = static_cast<u64>(message.size() + image.size());
    while (notification_count > 0) {
        const u64 free_space = news_system_save_data_archive->GetFreeBytes();
        if (needed_space <= free_space) {
            break;
        }

        LOG_WARNING(Service_NEWS, "Not enough space available. Deleting oldest notification ID: {}",
                    notification_ids[0]);

        // If we don't have space, delete old notifications until we do
        R_TRY(DeleteNotification(notification_ids[0]));

        notification_count--;
    }

    LOG_DEBUG(Service_NEWS, "New notification: notification_id={}, title={}",
              notification_ids[notification_count], Common::UTF16BufferToUTF8(header->title));

    if (!image.empty()) {
        R_TRY(SetNotificationImage(notification_count, image));
    }

    if (!message.empty()) {
        R_TRY(SetNotificationMessage(notification_count, message));
    }

    R_TRY(SetNotificationHeader(notification_count, header, header_size));

    // Sort the notifications after saving
    std::sort(notification_ids.begin(), notification_ids.end(),
              [this](const u32 first_id, const u32 second_id) -> bool {
                  return CompareNotifications(first_id, second_id);
              });

    return ResultSuccess;
}

Result Module::DeleteNotification(const u32 notification_id) {
    bool deleted = false;

    // Check if the input notification ID exists, and clear it
    if (db.notifications[notification_id].IsValid()) {
        db.notifications[notification_id] = {};

        R_TRY(SaveNewsDBSavedata());

        deleted = true;
    }

    // Cleanup images and messages for invalid notifications
    for (u32 i = 0; i < MAX_NOTIFICATIONS; ++i) {
        if (!db.notifications[i].IsValid()) {
            const std::string image_file = fmt::format("/news{:03d}.mpo", i);
            auto result = news_system_save_data_archive->DeleteFile(image_file);
            if (R_FAILED(result) && result != FileSys::ResultFileNotFound) {
                return result;
            }

            const std::string message_file = fmt::format("/news{:03d}.txt", i);
            result = news_system_save_data_archive->DeleteFile(message_file);
            if (R_FAILED(result) && result != FileSys::ResultFileNotFound) {
                return result;
            }
        }
    }

    // If the input notification ID was deleted, reorder the notification IDs list
    if (deleted) {
        std::sort(notification_ids.begin(), notification_ids.end(),
                  [this](const u32 first_id, const u32 second_id) -> bool {
                      return CompareNotifications(first_id, second_id);
                  });
    }

    return ResultSuccess;
}

Result Module::LoadNewsDBSavedata() {
    const std::string& nand_directory = FileUtil::GetUserPath(FileUtil::UserPath::NANDDir);
    FileSys::ArchiveFactory_SystemSaveData systemsavedata_factory(nand_directory);

    // Open the SystemSaveData archive 0x00010035
    FileSys::Path archive_path(news_system_savedata_id);
    auto archive_result = systemsavedata_factory.Open(archive_path, 0);

    // If the archive didn't exist, create the files inside
    if (archive_result.Code() == FileSys::ResultNotFound) {
        // Format the archive to create the directories
        systemsavedata_factory.Format(archive_path, FileSys::ArchiveFormatInfo(), 0, 0, 0);

        // Open it again to get a valid archive now that the folder exists
        news_system_save_data_archive = systemsavedata_factory.Open(archive_path, 0).Unwrap();
    } else {
        ASSERT_MSG(archive_result.Succeeded(), "Could not open the NEWS SystemSaveData archive!");

        news_system_save_data_archive = std::move(archive_result).Unwrap();
    }

    const std::string news_db_file = "/news.db";
    auto news_result =
        LoadFileFromSavedata(news_db_file, std::span{reinterpret_cast<u8*>(&db), sizeof(NewsDB)});

    // Read the file if it already exists
    if (news_result.Failed()) {
        // Create the file immediately if it doesn't exist
        db.header = {.valid = 1};
        news_result = SaveFileToSavedata(
            news_db_file, std::span{reinterpret_cast<const u8*>(&db), sizeof(NewsDB)});
    } else {
        // Sort the notifications from the file
        std::sort(notification_ids.begin(), notification_ids.end(),
                  [this](const u32 first_id, const u32 second_id) -> bool {
                      return CompareNotifications(first_id, second_id);
                  });
    }

    return news_result.Code();
}

Result Module::SaveNewsDBSavedata() {
    return SaveFileToSavedata("/news.db",
                              std::span{reinterpret_cast<const u8*>(&db), sizeof(NewsDB)});
}

ResultVal<std::size_t> Module::LoadFileFromSavedata(std::string filename, std::span<u8> buffer) {
    FileSys::Mode mode = {};
    mode.read_flag.Assign(1);

    FileSys::Path path(filename);

    auto result = news_system_save_data_archive->OpenFile(path, mode);
    if (result.Failed()) {
        return result.Code();
    }

    auto file = std::move(result).Unwrap();
    const auto bytes_read = file->Read(0, buffer.size(), buffer.data());
    file->Close();

    ASSERT_MSG(bytes_read.Succeeded(), "could not read file");

    return bytes_read.Unwrap();
}

Result Module::SaveFileToSavedata(std::string filename, std::span<const u8> buffer) {
    FileSys::Mode mode = {};
    mode.write_flag.Assign(1);
    mode.create_flag.Assign(1);

    FileSys::Path path(filename);

    auto result = news_system_save_data_archive->OpenFile(path, mode);
    ASSERT_MSG(result.Succeeded(), "could not open file");

    auto file = std::move(result).Unwrap();
    file->Write(0, buffer.size(), true, false, buffer.data());
    file->Close();

    return ResultSuccess;
}

bool Module::CompareNotifications(const u32 first_id, const u32 second_id) {
    // Notification IDs are sorted by date time, with valid notifications being first.
    // This is done so that other system applications like the News applet can easily
    // iterate over the notifications with an incrementing index.
    ASSERT(first_id < MAX_NOTIFICATIONS && second_id < MAX_NOTIFICATIONS);

    if (!db.notifications[first_id].IsValid()) {
        return false;
    }

    if (!db.notifications[second_id].IsValid()) {
        return true;
    }

    return db.notifications[first_id].date_time < db.notifications[second_id].date_time;
}

Module::Interface::Interface(std::shared_ptr<Module> news, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), news(std::move(news)) {}

Module::Module(Core::System& system_) : system(system_) {
    for (u32 i = 0; i < MAX_NOTIFICATIONS; ++i) {
        notification_ids[i] = i;
    }

    LoadNewsDBSavedata();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto news = std::make_shared<Module>(system);
    std::make_shared<NEWS_S>(news)->InstallAsService(service_manager);
    std::make_shared<NEWS_U>(news)->InstallAsService(service_manager);
}

} // namespace Service::NEWS
