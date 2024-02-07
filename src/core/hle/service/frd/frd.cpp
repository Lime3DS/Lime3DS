// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include <vector>
#include "common/archives.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/applets/mii_selector.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/mii.h"
#include "core/hle/result.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/frd/frd.h"
#include "core/hle/service/frd/frd_a.h"
#include "core/hle/service/frd/frd_u.h"

SERVICE_CONSTRUCT_IMPL(Service::FRD::Module)

namespace Service::FRD {

Module::Interface::Interface(std::shared_ptr<Module> frd, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), frd(std::move(frd)) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetMyPresence(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    std::vector<u8> buffer(sizeof(MyPresence));
    std::memcpy(buffer.data(), &frd->my_presence, buffer.size());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetFriendKeyList(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 unknown = rp.Pop<u32>();
    const u32 frd_count = rp.Pop<u32>();

    std::vector<u8> buffer(sizeof(FriendKey) * frd_count, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push<u32>(0); // 0 friends
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, unknown={}, frd_count={}", unknown, frd_count);
}

void Module::Interface::GetFriendProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 count = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    std::vector<u8> buffer(sizeof(Profile) * count, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::GetFriendAttributeFlags(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 count = rp.Pop<u32>();
    const std::vector<u8> frd_keys = rp.PopStaticBuffer();
    ASSERT(frd_keys.size() == count * sizeof(FriendKey));

    // TODO:(mailwl) figure out AttributeFlag size and zero all buffer. Assume 1 byte
    std::vector<u8> buffer(1 * count, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushStaticBuffer(std::move(buffer), 0);

    LOG_WARNING(Service_FRD, "(STUBBED) called, count={}", count);
}

void Module::Interface::GetMyFriendKey(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    rb.Push(ResultSuccess);
    rb.PushRaw(frd->my_friend_key);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetMyScreenName(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(7, 0);

    auto cfg = Service::CFG::GetModule(frd->system);
    auto username = cfg->GetUsername();
    ASSERT_MSG(username.length() <= 10, "Username longer than expected!");
    ScreenName screen_name{};
    std::memcpy(screen_name.name.data(), username.data(), username.length() * sizeof(char16_t));

    rb.Push(ResultSuccess);
    rb.PushRaw(screen_name);
    rb.Push(0);

    LOG_DEBUG(Service_FRD, "called");
}

void Module::Interface::GetMyComment(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(10, 0);

    constexpr Comment comment{.name = {u'H', u'e', u'y', '!'}};

    rb.Push(ResultSuccess);
    rb.PushRaw<Comment>(comment);
    rb.Push(0);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetMyMii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(0x19, 0);

    const auto mii_data = HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data;
    Mii::ChecksummedMiiData mii{};
    mii.SetMiiData(mii_data);

    rb.Push(ResultSuccess);
    rb.PushRaw<Mii::ChecksummedMiiData>(mii);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetMyProfile(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);

    constexpr Profile profile{.region = 1, .country = 1, .area = 1, .language = 1, .platform = 1};

    rb.Push(ResultSuccess);
    rb.PushRaw<Profile>(profile);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetMyFavoriteGame(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);

    constexpr Game game{.title_id = 0x0004000E00030700, .version = 1};

    rb.Push(ResultSuccess);
    rb.PushRaw<Game>(game);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetMyPlayingGame(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);

    constexpr Game game{.title_id = 0x0004000E00030700, .version = 1};

    rb.Push(ResultSuccess);
    rb.PushRaw<Game>(game);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::GetMyPreference(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(4, 0);

    constexpr u32 is_public = 1;
    constexpr u32 show_game = 1;
    constexpr u32 show_history = 0;

    rb.Push(ResultSuccess);
    rb.Push<u32>(is_public);
    rb.Push<u32>(show_game);
    rb.Push<u32>(show_history);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::UnscrambleLocalFriendCode(Kernel::HLERequestContext& ctx) {
    const std::size_t scrambled_friend_code_size = 12;
    const std::size_t friend_code_size = 8;

    IPC::RequestParser rp(ctx);
    const u32 friend_code_count = rp.Pop<u32>();
    const std::vector<u8> scrambled_friend_codes = rp.PopStaticBuffer();
    ASSERT_MSG(scrambled_friend_codes.size() == (friend_code_count * scrambled_friend_code_size),
               "Wrong input buffer size");

    std::vector<u8> unscrambled_friend_codes(friend_code_count * friend_code_size, 0);
    // TODO(B3N30): Unscramble the codes and compare them against the friend list
    //              Only write 0 if the code isn't in friend list, otherwise write the
    //              unscrambled one
    //
    // Code for unscrambling (should be compared to HW):
    // std::array<u16, 6> scambled_friend_code;
    // Memory::ReadBlock(scrambled_friend_codes+(current*scrambled_friend_code_size),
    // scambled_friend_code.data(), scrambled_friend_code_size); std::array<u16, 4>
    // unscrambled_friend_code; unscrambled_friend_code[0] = scambled_friend_code[0] ^
    // scambled_friend_code[5]; unscrambled_friend_code[1] = scambled_friend_code[1] ^
    // scambled_friend_code[5]; unscrambled_friend_code[2] = scambled_friend_code[2] ^
    // scambled_friend_code[5]; unscrambled_friend_code[3] = scambled_friend_code[3] ^
    // scambled_friend_code[5];

    LOG_WARNING(Service_FRD, "(STUBBED) called");
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushStaticBuffer(std::move(unscrambled_friend_codes), 0);
}

void Module::Interface::SetClientSdkVersion(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 version = rp.Pop<u32>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_FRD, "(STUBBED) called, version: 0x{:08X}", version);
}

void Module::Interface::IsOnline(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(ResultSuccess);
    rb.Push(frd->logged_in);

    LOG_WARNING(Service_FRD, "(STUBBED) called");
}

void Module::Interface::HasLoggedIn(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_FRD, "(STUBBED) called");

    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push(frd->logged_in);
}

void Module::Interface::Login(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_FRD, "(STUBBED) called");

    IPC::RequestParser rp(ctx);
    frd->login_event = rp.PopObject<Kernel::Event>();

    constexpr auto login_delay_ms = 500;
    frd->login_delay_event = frd->system.CoreTiming().RegisterEvent(
        "frd::login_event",
        // Simulate a small login delay
        [this](u64 thread_id, s64 cycle_late) {
            frd->logged_in = true;
            frd->login_event->Signal();
            frd->system.CoreTiming().RemoveEvent(frd->login_delay_event);
        });
    frd->system.CoreTiming().ScheduleEvent(msToCycles(login_delay_ms), frd->login_delay_event);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void Module::Interface::GetLastResponseResult(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_FRD, "(STUBBED) called");

    IPC::RequestParser rp(ctx);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

Module::Module(Core::System& system) : system(system){};
Module::~Module() = default;

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    auto frd = std::make_shared<Module>(system);
    std::make_shared<FRD_U>(frd)->InstallAsService(service_manager);
    std::make_shared<FRD_A>(frd)->InstallAsService(service_manager);
}

} // namespace Service::FRD
