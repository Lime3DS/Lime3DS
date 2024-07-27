// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/news/news_s.h"

SERIALIZE_EXPORT_IMPL(Service::NEWS::NEWS_S)

namespace Service::NEWS {

NEWS_S::NEWS_S(std::shared_ptr<Module> news) : Module::Interface(std::move(news), "news:s", 2) {
    const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &NEWS_S::AddNotificationSystem, "AddNotification"},
        {0x0004, &NEWS_S::ResetNotifications, "ResetNotifications"},
        {0x0005, &NEWS_S::GetTotalNotifications, "GetTotalNotifications"},
        {0x0006, &NEWS_S::SetNewsDBHeader, "SetNewsDBHeader"},
        {0x0007, &NEWS_S::SetNotificationHeader, "SetNotificationHeader"},
        {0x0008, &NEWS_S::SetNotificationMessage, "SetNotificationMessage"},
        {0x0009, &NEWS_S::SetNotificationImage, "SetNotificationImage"},
        {0x000A, &NEWS_S::GetNewsDBHeader, "GetNewsDBHeader"},
        {0x000B, &NEWS_S::GetNotificationHeader, "GetNotificationHeader"},
        {0x000C, &NEWS_S::GetNotificationMessage, "GetNotificationMessage"},
        {0x000D, &NEWS_S::GetNotificationImage, "GetNotificationImage"},
        {0x000E, nullptr, "SetInfoLEDPattern"},
        {0x000F, nullptr, "SyncArrivedNotifications"},
        {0x0010, nullptr, "SyncOneArrivedNotification"},
        {0x0011, &NEWS_S::SetAutomaticSyncFlag, "SetAutomaticSyncFlag"},
        {0x0012, &NEWS_S::SetNotificationHeaderOther, "SetNotificationHeaderOther"},
        {0x0013, &NEWS_S::WriteNewsDBSavedata, "WriteNewsDBSavedata"},
        {0x0014, &NEWS_S::GetTotalArrivedNotifications, "GetTotalArrivedNotifications"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NEWS
