// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/frd/frd_a.h"

SERIALIZE_EXPORT_IMPL(Service::FRD::FRD_A)

namespace Service::FRD {

FRD_A::FRD_A(std::shared_ptr<Module> frd) : Module::Interface(std::move(frd), "frd:a", 8) {
    static const FunctionInfo functions[] = {
        {0x0001, &FRD_A::HasLoggedIn, "HasLoggedIn"},
        {0x0002, &FRD_A::IsOnline, "IsOnline"},
        {0x0003, &FRD_A::Login, "Login"},
        {0x0004, nullptr, "Logout"},
        {0x0005, &FRD_A::GetMyFriendKey, "GetMyFriendKey"},
        {0x0006, &FRD_A::GetMyPreference, "GetMyPreference"},
        {0x0007, &FRD_A::GetMyProfile, "GetMyProfile"},
        {0x0008, &FRD_A::GetMyPresence, "GetMyPresence"},
        {0x0009, &FRD_A::GetMyScreenName, "GetMyScreenName"},
        {0x000A, &FRD_A::GetMyMii, "GetMyMii"},
        {0x000B, nullptr, "GetMyLocalAccountId"},
        {0x000C, &FRD_A::GetMyPlayingGame, "GetMyPlayingGame"},
        {0x000D, &FRD_A::GetMyFavoriteGame, "GetMyFavoriteGame"},
        {0x000E, nullptr, "GetMyNcPrincipalId"},
        {0x000F, &FRD_A::GetMyComment, "GetMyComment"},
        {0x0010, nullptr, "GetMyPassword"},
        {0x0011, &FRD_A::GetFriendKeyList, "GetFriendKeyList"},
        {0x0012, nullptr, "GetFriendPresence"},
        {0x0013, nullptr, "GetFriendScreenName"},
        {0x0014, nullptr, "GetFriendMii"},
        {0x0015, &FRD_A::GetFriendProfile, "GetFriendProfile"},
        {0x0016, nullptr, "GetFriendRelationship"},
        {0x0017, &FRD_A::GetFriendAttributeFlags, "GetFriendAttributeFlags"},
        {0x0018, nullptr, "GetFriendPlayingGame"},
        {0x0019, nullptr, "GetFriendFavoriteGame"},
        {0x001A, nullptr, "GetFriendInfo"},
        {0x001B, nullptr, "IsIncludedInFriendList"},
        {0x001C, &FRD_A::UnscrambleLocalFriendCode, "UnscrambleLocalFriendCode"},
        {0x001D, nullptr, "UpdateGameModeDescription"},
        {0x001E, nullptr, "UpdateGameMode"},
        {0x001F, nullptr, "SendInvitation"},
        {0x0020, nullptr, "AttachToEventNotification"},
        {0x0021, nullptr, "SetNotificationMask"},
        {0x0022, nullptr, "GetEventNotification"},
        {0x0023, &FRD_A::GetLastResponseResult, "GetLastResponseResult"},
        {0x0024, nullptr, "PrincipalIdToFriendCode"},
        {0x0025, nullptr, "FriendCodeToPrincipalId"},
        {0x0026, nullptr, "IsValidFriendCode"},
        {0x0027, nullptr, "ResultToErrorCode"},
        {0x0028, nullptr, "RequestGameAuthentication"},
        {0x0029, nullptr, "GetGameAuthenticationData"},
        {0x002A, nullptr, "RequestServiceLocator"},
        {0x002B, nullptr, "GetServiceLocatorData"},
        {0x002C, nullptr, "DetectNatProperties"},
        {0x002D, nullptr, "GetNatProperties"},
        {0x002E, nullptr, "GetServerTimeInterval"},
        {0x002F, nullptr, "AllowHalfAwake"},
        {0x0030, nullptr, "GetServerTypes"},
        {0x0031, nullptr, "GetFriendComment"},
        {0x0032, &FRD_A::SetClientSdkVersion, "SetClientSdkVersion"},
        {0x0033, nullptr, "GetMyApproachContext"},
        {0x0034, nullptr, "AddFriendWithApproach"},
        {0x0035, nullptr, "DecryptApproachContext"},
        {0x0405, nullptr, "SaveData"},
        {0x0406, nullptr, "AddFriendOnline"},
        {0x0409, nullptr, "RemoveFriend"},
        {0x040a, nullptr, "UpdatePlayingGame"},
        {0x040b, nullptr, "UpdatePreference"},
        {0x040c, nullptr, "UpdateMii"},
        {0x040d, nullptr, "UpdateFavoriteGame"},
        {0x040e, nullptr, "UpdateNcPrincipalId"},
        {0x040f, nullptr, "UpdateComment"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::FRD
