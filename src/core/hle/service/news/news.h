// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/file_sys/archive_backend.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NEWS {
constexpr u32 MAX_NOTIFICATIONS = 100;

struct NewsDBHeader {
    u8 valid;
    u8 flags;
    INSERT_PADDING_BYTES(0xE);

    bool IsValid() const {
        return valid == 1;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & valid;
        ar & flags;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(NewsDBHeader) == 0x10, "News DB Header structure size is wrong");

struct NotificationHeader {
    u8 flag_valid;
    u8 flag_read;
    u8 flag_jpeg;
    u8 flag_boss;
    u8 flag_optout;
    u8 flag_url;
    u8 flag_unk0x6;
    INSERT_PADDING_BYTES(0x1);
    u64_le program_id;
    u32_le ns_data_id; // Only used in BOSS notifications
    u32_le version;    // Only used in BOSS notifications
    u64_le jump_param;
    INSERT_PADDING_BYTES(0x8);
    u64_le date_time;
    std::array<u16_le, 0x20> title;

    bool IsValid() const {
        return flag_valid == 1;
    }

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & flag_valid;
        ar & flag_read;
        ar & flag_jpeg;
        ar & flag_boss;
        ar & flag_optout;
        ar & flag_url;
        ar & flag_unk0x6;
        ar & program_id;
        ar & ns_data_id;
        ar & version;
        ar & jump_param;
        ar & date_time;
        ar & title;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(NotificationHeader) == 0x70, "Notification Header structure size is wrong");

struct NewsDB {
    NewsDBHeader header;
    std::array<NotificationHeader, MAX_NOTIFICATIONS> notifications;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & header;
        ar & notifications;
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(NewsDB) == 0x2BD0, "News DB structure size is wrong");

class Module final {
public:
    explicit Module(Core::System& system_);
    ~Module() = default;

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> news, const char* name, u32 max_session);
        ~Interface() = default;

    private:
        void AddNotificationImpl(Kernel::HLERequestContext& ctx, bool news_s);

    protected:
        /**
         * AddNotification NEWS:U service function.
         *  Inputs:
         *      0 : 0x000100C8
         *      1 : Header size
         *      2 : Message size
         *      3 : Image size
         *      4 : PID Translation Header (0x20)
         *      5 : Caller PID
         *      6 : Header Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      7 : Header Buffer Pointer
         *      8 : Message Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      9 : Message Buffer Pointer
         *     10 : Image Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *     11 : Image Buffer Pointer
         *  Outputs:
         *      0 : 0x00010046
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void AddNotification(Kernel::HLERequestContext& ctx);

        /**
         * AddNotification NEWS:S service function.
         *  Inputs:
         *      0 : 0x000100C6
         *      1 : Header size
         *      2 : Message size
         *      3 : Image size
         *      4 : Header Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      5 : Header Buffer Pointer
         *      6 : Message Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      7 : Message Buffer Pointer
         *      8 : Image Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      9 : Image Buffer Pointer
         *  Outputs:
         *      0 : 0x00010046
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void AddNotificationSystem(Kernel::HLERequestContext& ctx);

        /**
         * ResetNotifications service function.
         *  Inputs:
         *      0 : 0x00040000
         *  Outputs:
         *      0 : 0x00040040
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void ResetNotifications(Kernel::HLERequestContext& ctx);

        /**
         * GetTotalNotifications service function.
         *  Inputs:
         *      0 : 0x00050000
         *  Outputs:
         *      0 : 0x00050080
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Number of notifications
         */
        void GetTotalNotifications(Kernel::HLERequestContext& ctx);

        /**
         * SetNewsDBHeader service function.
         *  Inputs:
         *      0 : 0x00060042
         *      1 : Size
         *      2 : Input Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      3 : Input Buffer Pointer
         *  Outputs:
         *      0 : 0x00060042
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNewsDBHeader(Kernel::HLERequestContext& ctx);

        /**
         * SetNotificationHeader service function.
         *  Inputs:
         *      0 : 0x00070082
         *      1 : Notification index
         *      2 : Size
         *      3 : Input Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      4 : Input Buffer Pointer
         *  Outputs:
         *      0 : 0x00070042
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNotificationHeader(Kernel::HLERequestContext& ctx);

        /**
         * SetNotificationMessage service function.
         *  Inputs:
         *      0 : 0x00080082
         *      1 : Notification index
         *      2 : Size
         *      3 : Input Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      4 : Input Buffer Pointer
         *  Outputs:
         *      0 : 0x00080042
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNotificationMessage(Kernel::HLERequestContext& ctx);

        /**
         * SetNotificationImage service function.
         *  Inputs:
         *      0 : 0x00090082
         *      1 : Notification index
         *      2 : Size
         *      3 : Input Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      4 : Input Buffer Pointer
         *  Outputs:
         *      0 : 0x00090042
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNotificationImage(Kernel::HLERequestContext& ctx);

        /**
         * GetNewsDBHeader service function.
         *  Inputs:
         *      0 : 0x000A0042
         *      1 : Size
         *      2 : Output Buffer Mapping Translation Header ((Size << 4) | 0xC)
         *      3 : Output Buffer Pointer
         *  Outputs:
         *      0 : 0x000A0082
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Actual Size
         */
        void GetNewsDBHeader(Kernel::HLERequestContext& ctx);

        /**
         * GetNotificationHeader service function.
         *  Inputs:
         *      0 : 0x000B0082
         *      1 : Notification index
         *      2 : Size
         *      3 : Output Buffer Mapping Translation Header ((Size << 4) | 0xC)
         *      4 : Output Buffer Pointer
         *  Outputs:
         *      0 : 0x000B0082
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Actual Size
         */
        void GetNotificationHeader(Kernel::HLERequestContext& ctx);

        /**
         * GetNotificationMessage service function.
         *  Inputs:
         *      0 : 0x000C0082
         *      1 : Notification index
         *      2 : Size
         *      3 : Output Buffer Mapping Translation Header ((Size << 4) | 0xC)
         *      4 : Output Buffer Pointer
         *  Outputs:
         *      0 : 0x000C0082
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Actual Size
         */
        void GetNotificationMessage(Kernel::HLERequestContext& ctx);

        /**
         * GetNotificationImage service function.
         *  Inputs:
         *      0 : 0x000D0082
         *      1 : Notification index
         *      2 : Size
         *      3 : Output Buffer Mapping Translation Header ((Size << 4) | 0xC)
         *      4 : Output Buffer Pointer
         *  Outputs:
         *      0 : 0x000D0082
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Actual Size
         */
        void GetNotificationImage(Kernel::HLERequestContext& ctx);

        /**
         * SetAutomaticSyncFlag service function.
         *  Inputs:
         *      0 : 0x00110040
         *      1 : Flag
         *  Outputs:
         *      0 : 0x00110040
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetAutomaticSyncFlag(Kernel::HLERequestContext& ctx);

        /**
         * SetNotificationHeaderOther service function.
         *  Inputs:
         *      0 : 0x00120082
         *      1 : Notification index
         *      2 : Size
         *      3 : Input Buffer Mapping Translation Header ((Size << 4) | 0xA)
         *      4 : Input Buffer Pointer
         *  Outputs:
         *      0 : 0x00120042
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetNotificationHeaderOther(Kernel::HLERequestContext& ctx);

        /**
         * WriteNewsDBSavedata service function.
         *  Inputs:
         *      0 : 0x00130000
         *  Outputs:
         *      0 : 0x00130040
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void WriteNewsDBSavedata(Kernel::HLERequestContext& ctx);

        /**
         * GetTotalArrivedNotifications service function.
         *  Inputs:
         *      0 : 0x00140000
         *  Outputs:
         *      0 : 0x00140080
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Number of pending notifications to be synced
         */
        void GetTotalArrivedNotifications(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> news;
    };

private:
    /**
     * Gets the total number of notifications
     * @returns Number of notifications
     */
    std::size_t GetTotalNotifications();

    /**
     * Loads the News DB into the given buffer
     * @param header The header buffer
     * @param size The size of the header buffer
     * @returns Number of bytes read, or error code
     */
    ResultVal<std::size_t> GetNewsDBHeader(NewsDBHeader* header, const std::size_t size);

    /**
     * Loads the header for a notification ID into the given buffer
     * @param notification_index The index of the notification ID
     * @param header The header buffer
     * @param size The size of the header buffer
     * @returns Number of bytes read, or error code
     */
    ResultVal<std::size_t> GetNotificationHeader(const u32 notification_index,
                                                 NotificationHeader* header,
                                                 const std::size_t size);

    /**
     * Opens the message file for a notification ID and loads it to the message buffer
     * @param notification_index The index of the notification ID
     * @param mesasge The message buffer
     * @returns Number of bytes read, or error code
     */
    ResultVal<std::size_t> GetNotificationMessage(const u32 notification_index,
                                                  std::span<u8> message);

    /**
     * Opens the image file for a notification ID and loads it to the image buffer
     * @param notification_index The index of the notification ID
     * @param image The image buffer
     * @returns Number of bytes read, or error code
     */
    ResultVal<std::size_t> GetNotificationImage(const u32 notification_index, std::span<u8> image);

    /**
     * Modifies the header for the News DB in memory and saves the News DB file
     * @param header The database header
     * @param size The amount of bytes to copy from the header
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SetNewsDBHeader(const NewsDBHeader* header, const std::size_t size);

    /**
     * Modifies the header for a notification ID on memory and saves the News DB file
     * @param notification_index The index of the notification ID
     * @param header The notification header
     * @param size The amount of bytes to copy from the header
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SetNotificationHeader(const u32 notification_index, const NotificationHeader* header,
                                 const std::size_t size);

    /**
     * Modifies the header for a notification ID on memory. The News DB file isn't updated
     * @param notification_index The index of the notification ID
     * @param header The notification header
     * @param size The amount of bytes to copy from the header
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SetNotificationHeaderOther(const u32 notification_index,
                                      const NotificationHeader* header, const std::size_t size);

    /**
     * Sets a given message to a notification ID
     * @param notification_index The index of the notification ID
     * @param message The notification message
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SetNotificationMessage(const u32 notification_index, std::span<const u8> message);

    /**
     * Sets a given image to a notification ID
     * @param notification_index The index of the notification ID
     * @param image The notification image
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SetNotificationImage(const u32 notification_index, std::span<const u8> image);

    /**
     * Creates a new notification with the given data and saves all the contents
     * @param header The notification header
     * @param header_size The amount of bytes to copy from the header
     * @param message The notification message
     * @param image The notification image
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SaveNotification(const NotificationHeader* header, const std::size_t header_size,
                            std::span<const u8> message, std::span<const u8> image);

    /**
     * Deletes the given notification ID from the database
     * @param notification_id The notification ID to delete
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result DeleteNotification(const u32 notification_id);

    /**
     * Opens the news.db savedata file and load it to the memory buffer. If the file or the savedata
     * don't exist, they are created
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result LoadNewsDBSavedata();

    /**
     * Writes the news.db savedata file to the the NEWS system savedata
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SaveNewsDBSavedata();

    /**
     * Opens the file with the given filename inside the NEWS system savedata
     * @param filename The file to open
     * @param buffer The buffer to output the contents on
     * @returns Number of bytes read, or error code
     */
    ResultVal<std::size_t> LoadFileFromSavedata(std::string filename, std::span<u8> buffer);

    /**
     * Writes the file with the given filename inside the NEWS system savedata
     * @param filename The output file
     * @param buffer The buffer to read the contents from
     * @returns Result indicating the result of the operation, 0 on success
     */
    Result SaveFileToSavedata(std::string filename, std::span<const u8> buffer);

    bool CompareNotifications(const u32 first_id, const u32 second_id);

private:
    Core::System& system;

    NewsDB db{};
    std::array<u32, MAX_NOTIFICATIONS> notification_ids; // Notifications ordered by date time
    u8 automatic_sync_flag;
    std::unique_ptr<FileSys::ArchiveBackend> news_system_save_data_archive;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::NEWS

SERVICE_CONSTRUCT(Service::NEWS::Module)
BOOST_CLASS_EXPORT_KEY(Service::NEWS::Module)
