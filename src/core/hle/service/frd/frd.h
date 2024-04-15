// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
}

namespace Service::FRD {

struct FriendKey {
    u32 friend_id;
    u32 unknown;
    u64 friend_code;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & friend_id;
        ar & unknown;
        ar & friend_code;
    }
    friend class boost::serialization::access;
};

struct MyPresence {
    u8 unknown[0x12C];

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & unknown;
    }
    friend class boost::serialization::access;
};

struct Profile {
    u8 region;
    u8 country;
    u8 area;
    u8 language;
    u8 platform;
    INSERT_PADDING_BYTES(0x3);
};
static_assert(sizeof(Profile) == 0x8, "Profile has incorrect size");

struct Game {
    u64 title_id;
    u16 version;
    INSERT_PADDING_BYTES(0x6);
};
static_assert(sizeof(Game) == 0x10, "Game has inccorect size");

struct ScreenName {
    // 20 bytes according to 3dbrew
    std::array<char16_t, 10> name;
};
static_assert(sizeof(ScreenName) == 0x14, "ScreenName has inccorect size");

struct Comment {
    // 32 bytes according to 3dbrew
    std::array<char16_t, 16> name;
};
static_assert(sizeof(Comment) == 0x20, "Comment has inccorect size");

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> frd, const char* name, u32 max_session);
        ~Interface();

    protected:
        /**
         * FRD::GetMyPresence service function
         *  Inputs:
         *      64 : sizeof (MyPresence) << 14 | 2
         *      65 : Address of MyPresence structure
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetMyPresence(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetFriendKeyList service function
         *  Inputs:
         *      1 : Unknown
         *      2 : Max friends count
         *      65 : Address of FriendKey List
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : FriendKey count filled
         */
        void GetFriendKeyList(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetFriendProfile service function
         *  Inputs:
         *      1 : Friends count
         *      2 : Friends count << 18 | 2
         *      3 : Address of FriendKey List
         *      64 : (count * sizeof (Profile)) << 10 | 2
         *      65 : Address of Profiles List
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetFriendProfile(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetFriendAttributeFlags service function
         *  Inputs:
         *      1 : Friends count
         *      2 : Friends count << 18 | 2
         *      3 : Address of FriendKey List
         *      65 : Address of AttributeFlags
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetFriendAttributeFlags(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyFriendKey service function
         *  Inputs:
         *      none
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2-5 : FriendKey
         */
        void GetMyFriendKey(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyScreenName service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : UTF16 encoded name (max 11 symbols)
         */
        void GetMyScreenName(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyMii service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : MiiStoreData structure
         */
        void GetMyMii(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyProfile service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *    2-3 : Profile structure
         */
        void GetMyProfile(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyComment service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : UTF16 encoded comment (max 16 symbols)
         */
        void GetMyComment(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyFavoriteGame service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *    2-3 : Game structure
         */
        void GetMyFavoriteGame(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyPlayingGame service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *    2-3 : Game structure
         */
        void GetMyPlayingGame(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetMyPreference service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Public mode (byte, 0 = private, non-zero = public)
         *      3 : Show current game (byte, 0 = don't show, non-zero = show)
         *      4 : Show game history (byte, 0 = don't show, non-zero = show)
         */
        void GetMyPreference(Kernel::HLERequestContext& ctx);

        /**
         * FRD::UnscrambleLocalFriendCode service function
         *  Inputs:
         *      1 : Friend code count
         *      2 : ((count * 12) << 14) | 0x402
         *      3 : Pointer to encoded friend codes. Each is 12 bytes large
         *      64 : ((count * 8) << 14) | 2
         *      65 : Pointer to write decoded local friend codes to. Each is 8 bytes large.
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void UnscrambleLocalFriendCode(Kernel::HLERequestContext& ctx);

        /**
         * FRD::SetClientSdkVersion service function
         *  Inputs:
         *      1 : Used SDK Version
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetClientSdkVersion(Kernel::HLERequestContext& ctx);

        /**
         * FRD::Login service function
         *  Inputs:
         *      65 : Address of unknown event
         *  Outputs:
         *      1  : Result of function, 0 on success, otherwise error code
         */
        void Login(Kernel::HLERequestContext& ctx);

        /**
         * FRD::IsOnline service function
         *  Inputs:
         *      none
         *  Outputs:
         *      1  : Result of function, 0 on success, otherwise error code
         *      2  : Online state (8-bit, 0 = not online, non-zero = online)
         */
        void IsOnline(Kernel::HLERequestContext& ctx);

        /**
         * FRD::HasLoggedIn service function
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : If the user has logged in 1, otherwise 0
         */
        void HasLoggedIn(Kernel::HLERequestContext& ctx);

        /**
         * FRD::GetLastResponseResult service function
         *  Inputs:
         *      none
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetLastResponseResult(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> frd;
    };

private:
    FriendKey my_friend_key = {0, 0, 0ull};
    MyPresence my_presence = {};
    bool logged_in = false;
    std::shared_ptr<Kernel::Event> login_event;
    Core::TimingEventType* login_delay_event;
    Core::System& system;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & my_friend_key;
        ar & my_presence;
        ar & logged_in;
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::FRD

SERVICE_CONSTRUCT(Service::FRD::Module)
