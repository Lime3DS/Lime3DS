// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <cryptopp/base64.h>

#ifdef _WIN32
// windows.h needs to be included before shellapi.h
#include <windows.h>

#include <shellapi.h>
#endif

#include "common/common_paths.h"
#include "common/common_types.h"
#include "common/detached_tasks.h"
#include "common/file_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "network/announce_multiplayer_session.h"
#include "network/network.h"
#include "network/network_settings.h"
#include "network/room.h"
#include "network/verify_user.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/verify_user_jwt.h"
#endif

#undef _UNICODE
#include <getopt.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif

static void PrintHelp(const char* argv0) {
    std::cout
        << "Usage: " << argv0
        << " [options] <filename>\n"
           "--room-name         The name of the room\n"
           "--room-description  The room description\n"
           "--port              The port used for the room\n"
           "--max_members       The maximum number of players for this room\n"
           "--password          The password for the room\n"
           "--preferred-game    The preferred game for this room\n"
           "--preferred-game-id The preferred game-id for this room\n"
           "--username          The username used for announce\n"
           "--token             The token used for announce\n"
           "--web-api-url       Lime3DS Web API url\n"
           "--ban-list-file     The file for storing the room ban list\n"
           "--log-file          The file for storing the room log\n"
           "--enable-lime3ds-mods Allow Lime3DS Community Moderators to moderate on your room\n"
           "-h, --help          Display this help and exit\n"
           "-v, --version       Output version information and exit\n";
}

static void PrintVersion() {
    std::cout << "Lime3DS dedicated room " << Common::g_scm_branch << " " << Common::g_scm_desc
              << " Libnetwork: " << Network::network_version << std::endl;
}

/// The magic text at the beginning of a lime-room ban list file.
static constexpr char BanListMagic[] = "Lime3DSRoom-BanList-1";

static constexpr char token_delimiter{':'};

static std::string UsernameFromDisplayToken(const std::string& display_token) {
    std::string unencoded_display_token;
    CryptoPP::StringSource ss(
        display_token, true,
        new CryptoPP::Base64Decoder(new CryptoPP::StringSink(unencoded_display_token)));
    return unencoded_display_token.substr(0, unencoded_display_token.find(token_delimiter));
}

static std::string TokenFromDisplayToken(const std::string& display_token) {
    std::string unencoded_display_token;
    CryptoPP::StringSource ss(
        display_token, true,
        new CryptoPP::Base64Decoder(new CryptoPP::StringSink(unencoded_display_token)));
    return unencoded_display_token.substr(unencoded_display_token.find(token_delimiter) + 1);
}

static Network::Room::BanList LoadBanList(const std::string& path) {
    std::ifstream file;
    OpenFStream(file, path, std::ios_base::in);
    if (!file || file.eof()) {
        std::cout << "Could not open ban list!\n\n";
        return {};
    }
    std::string magic;
    std::getline(file, magic);
    if (magic != BanListMagic) {
        std::cout << "Ban list is not valid!\n\n";
        return {};
    }

    // false = username ban list, true = ip ban list
    bool ban_list_type = false;
    Network::Room::UsernameBanList username_ban_list;
    Network::Room::IPBanList ip_ban_list;
    while (!file.eof()) {
        std::string line;
        std::getline(file, line);
        line.erase(std::remove(line.begin(), line.end(), '\0'), line.end());
        line = Common::StripSpaces(line);
        if (line.empty()) {
            // An empty line marks start of the IP ban list
            ban_list_type = true;
            continue;
        }
        if (ban_list_type) {
            ip_ban_list.emplace_back(line);
        } else {
            username_ban_list.emplace_back(line);
        }
    }

    return {username_ban_list, ip_ban_list};
}

static void SaveBanList(const Network::Room::BanList& ban_list, const std::string& path) {
    std::ofstream file;
    OpenFStream(file, path, std::ios_base::out);
    if (!file) {
        std::cout << "Could not save ban list!\n\n";
        return;
    }

    file << BanListMagic << "\n";

    // Username ban list
    for (const auto& username : ban_list.first) {
        file << username << "\n";
    }
    file << "\n";

    // IP ban list
    for (const auto& ip : ban_list.second) {
        file << ip << "\n";
    }

    file.flush();
}

static void InitializeLogging(const std::string& log_file) {
    Common::Log::Initialize(log_file);
    Common::Log::SetColorConsoleBackendEnabled(true);
    Common::Log::Start();
}

/// Application entry point
int main(int argc, char** argv) {
    Common::DetachedTasks detached_tasks;
    int option_index = 0;
    char* endarg;

    std::string room_name;
    std::string room_description;
    std::string password;
    std::string preferred_game;
    std::string username;
    std::string token;
    std::string web_api_url;
    std::string ban_list_file;
    std::string log_file = "lime-room.log";
    u64 preferred_game_id = 0;
    u16 port = Network::DefaultRoomPort;
    u32 max_members = 16;
    bool enable_lime3ds_mods = false;

    static struct option long_options[] = {
        {"room-name", required_argument, 0, 'n'},
        {"room-description", required_argument, 0, 'd'},
        {"port", required_argument, 0, 'p'},
        {"max_members", required_argument, 0, 'm'},
        {"password", required_argument, 0, 'w'},
        {"preferred-game", required_argument, 0, 'g'},
        {"preferred-game-id", required_argument, 0, 'i'},
        {"username", optional_argument, 0, 'u'},
        {"token", required_argument, 0, 't'},
        {"web-api-url", required_argument, 0, 'a'},
        {"ban-list-file", required_argument, 0, 'b'},
        {"log-file", required_argument, 0, 'l'},
        {"enable-lime3ds-mods", no_argument, 0, 'e'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0},
    };

    while (optind < argc) {
        int arg = getopt_long(argc, argv, "n:d:p:m:w:g:u:t:a:i:l:hv", long_options, &option_index);
        if (arg != -1) {
            switch (static_cast<char>(arg)) {
            case 'n':
                room_name.assign(optarg);
                break;
            case 'd':
                room_description.assign(optarg);
                break;
            case 'p':
                port = static_cast<u16>(strtoul(optarg, &endarg, 0));
                break;
            case 'm':
                max_members = strtoul(optarg, &endarg, 0);
                break;
            case 'w':
                password.assign(optarg);
                break;
            case 'g':
                preferred_game.assign(optarg);
                break;
            case 'i':
                preferred_game_id = strtoull(optarg, &endarg, 16);
                break;
            case 'u':
                username.assign(optarg);
                break;
            case 't':
                token.assign(optarg);
                break;
            case 'a':
                web_api_url.assign(optarg);
                break;
            case 'b':
                ban_list_file.assign(optarg);
                break;
            case 'l':
                log_file.assign(optarg);
                break;
            case 'e':
                enable_lime3ds_mods = true;
                break;
            case 'h':
                PrintHelp(argv[0]);
                return 0;
            case 'v':
                PrintVersion();
                return 0;
            }
        }
    }

    if (room_name.empty()) {
        std::cout << "room name is empty!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (preferred_game.empty()) {
        std::cout << "preferred game is empty!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (preferred_game_id == 0) {
        std::cout << "preferred-game-id not set!\nThis should get set to allow users to find your "
                     "room.\nSet with --preferred-game-id id\n\n";
    }
    if (max_members > Network::MaxConcurrentConnections || max_members < 2) {
        std::cout << "max_members needs to be in the range 2 - "
                  << Network::MaxConcurrentConnections << "!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (port > 65535) {
        std::cout << "port needs to be in the range 0 - 65535!\n\n";
        PrintHelp(argv[0]);
        return -1;
    }
    if (ban_list_file.empty()) {
        std::cout << "Ban list file not set!\nThis should get set to load and save room ban "
                     "list.\nSet with --ban-list-file <file>\n\n";
    }
    bool announce = true;
    if (token.empty() && announce) {
        announce = false;
        std::cout << "token is empty: Hosting a private room\n\n";
    }
    if (web_api_url.empty() && announce) {
        announce = false;
        std::cout << "endpoint url is empty: Hosting a private room\n\n";
    }
    if (announce) {
        if (username.empty()) {
            std::cout << "Hosting a public room\n\n";
            NetSettings::values.web_api_url = web_api_url;
            NetSettings::values.lime3ds_username = UsernameFromDisplayToken(token);
            username = NetSettings::values.lime3ds_username;
            NetSettings::values.lime3ds_token = TokenFromDisplayToken(token);
        } else {
            std::cout << "Hosting a public room\n\n";
            NetSettings::values.web_api_url = web_api_url;
            NetSettings::values.lime3ds_username = username;
            NetSettings::values.lime3ds_token = token;
        }
    }
    if (!announce && enable_lime3ds_mods) {
        enable_lime3ds_mods = false;
        std::cout << "Can not enable Lime3DS Moderators for private rooms\n\n";
    }

    InitializeLogging(log_file);

    // Load the ban list
    Network::Room::BanList ban_list;
    if (!ban_list_file.empty()) {
        ban_list = LoadBanList(ban_list_file);
    }

    std::unique_ptr<Network::VerifyUser::Backend> verify_backend;
    if (announce) {
#ifdef ENABLE_WEB_SERVICE
        verify_backend =
            std::make_unique<WebService::VerifyUserJWT>(NetSettings::values.web_api_url);
#else
        std::cout
            << "Lime3DS Web Services is not available with this build: validation is disabled.\n\n";
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
#endif
    } else {
        verify_backend = std::make_unique<Network::VerifyUser::NullBackend>();
    }

    Network::Init();
    if (std::shared_ptr<Network::Room> room = Network::GetRoom().lock()) {
        if (!room->Create(room_name, room_description, "", port, password, max_members, username,
                          preferred_game, preferred_game_id, std::move(verify_backend), ban_list,
                          enable_lime3ds_mods)) {
            std::cout << "Failed to create room: \n\n";
            return -1;
        }
        std::cout << "Room is open. Close with Q+Enter...\n\n";
        auto announce_session = std::make_unique<Network::AnnounceMultiplayerSession>();
        if (announce) {
            announce_session->Start();
        }
        while (room->GetState() == Network::Room::State::Open) {
            std::string in;
            std::cin >> in;
            if (in.size() > 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (announce) {
            announce_session->Stop();
        }
        announce_session.reset();
        // Save the ban list
        if (!ban_list_file.empty()) {
            SaveBanList(room->GetBanList(), ban_list_file);
        }
        room->Destroy();
    }
    Network::Shutdown();
    detached_tasks.WaitForAllTasks();
    return 0;
}
