// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "client.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <httplib.h>
#include <rc_client.h>
#include <rc_consoles.h>

#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "core/core.h"
#include "core/memory.h"

#define USE_RETRO_ACHIEVEMENTS_DEV_SERVER

// TODO: Make this use a numeric version as per
// https://github.com/RetroAchievements/rcheevos/wiki/rc_client-integration#user-agent-header
static const std::string user_agent = std::string("Azahar/") + Common::g_build_fullname;
static const httplib::Headers headers = httplib::Headers({{"User-Agent", user_agent}});

static std::pair<std::string_view, std::string_view> parse_url(std::string_view full_url) {
    constexpr std::string_view protocol_separator = "://";
    const size_t protocol_end = full_url.find(protocol_separator);
    const size_t host_start =
        protocol_end == std::string_view::npos ? 0 : protocol_end + protocol_separator.size();
    const size_t path_start = full_url.find('/', host_start);

    if (path_start == std::string_view::npos) {
        return {full_url, "/"};
    }

    return {full_url.substr(0, path_start), full_url.substr(path_start)};
}

static uint32_t read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes,
                            rc_client_t* rc_client) {
    LOG_DEBUG(RetroAchievements, "Reading {} bytes from 0x{:x}", num_bytes, address);

    Core::System& system = Core::System::GetInstance();
    return system.Memory().ReadBlock(static_cast<VAddr>(address), buffer, num_bytes) ? num_bytes
                                                                                     : 0;
}

static void log_message(const char* message, const rc_client_t* rc_client) {
    LOG_INFO(RetroAchievements, "rcheevos message: \"{}\"", message);
}

static void login_callback(int result, const char* error_message, rc_client_t* rc_client,
                           void* userdata) {
    RetroAchievements::Client* client = static_cast<RetroAchievements::Client*>(userdata);
    client->OnLoginCallback(result,
                            error_message ? std::string_view{error_message} : std::string_view{});
}

static void load_game_callback(int result, const char* error_message, rc_client_t* rc_client,
                               void* userdata) {
    RetroAchievements::Client* client = static_cast<RetroAchievements::Client*>(userdata);
    client->OnLoadGameCallback(result, error_message ? std::string_view{error_message}
                                                     : std::string_view{});
}

static void event_handler(const rc_client_event_t* event, rc_client_t* client) {
    auto* ra_client = static_cast<RetroAchievements::Client*>(rc_client_get_userdata(client));
    if (ra_client) {
        ra_client->OnEvent(event);
    }
}

static void call_server(const rc_api_request_t* request, rc_client_server_callback_t callback,
                        void* callback_data, rc_client_t* rc_client) {
    auto* client = static_cast<RetroAchievements::Client*>(rc_client_get_userdata(rc_client));
    if (!client) {
        return;
    }

    RetroAchievements::Client::HttpRequest http_request = {
        .url = request->url != nullptr ? request->url : "",
        .post_data = request->post_data != nullptr ? std::optional<std::string>{request->post_data}
                                                   : std::nullopt,
        .content_type = request->content_type != nullptr ? request->content_type : "",
    };

    LOG_DEBUG(RetroAchievements, "Server request: {} {}",
              http_request.post_data.has_value() ? "POST" : "GET", http_request.url);

    client->QueueHttpRequest(
        std::move(http_request),
        [callback, callback_data](RetroAchievements::Client::HttpResponse&& response) {
            if (response.success) {
                LOG_DEBUG(RetroAchievements, "Server response status: {}", response.status);
                LOG_DEBUG(RetroAchievements, "Server response body: {}", response.body);
            } else {
                LOG_ERROR(RetroAchievements, "httplib error: {}", response.body);
            }

            rc_api_server_response_t server_response = {
                .body = response.body.c_str(),
                .body_length = response.body.length(),
                .http_status_code =
                    response.success ? response.status : RC_API_SERVER_RESPONSE_CLIENT_ERROR,
            };
            callback(&server_response, callback_data);
        });
}

namespace RetroAchievements {

Client::Client() {
    m_rc_client = rc_client_create(read_memory, call_server);

    rc_client_enable_logging(m_rc_client, RC_CLIENT_LOG_LEVEL_VERBOSE, log_message);
    rc_client_set_event_handler(m_rc_client, event_handler);
    rc_client_set_userdata(m_rc_client, this);
    rc_client_set_allow_background_memory_reads(m_rc_client, 0);
    rc_client_set_hardcore_enabled(m_rc_client, 0);

#ifdef USE_RETRO_ACHIEVEMENTS_DEV_SERVER
    rc_client_set_host(m_rc_client, "http://localhost:64000");
#endif
}

Client::~Client() {
    m_enabled = false;
    m_http_worker.WaitForRequests();

    if (m_rc_client) {
        rc_client_destroy(m_rc_client);
        m_rc_client = nullptr;
    }
}

void Client::QueueHttpRequest(HttpRequest&& request, HttpCallback callback) {
    m_http_worker.QueueWork([request, callback = std::move(callback)]() {
        const auto [base_url, path] = parse_url(request.url);

        httplib::Client http_client{std::string{base_url}};
        const std::string request_path{path};

        httplib::Result result =
            request.post_data.has_value()
                ? http_client.Post(request_path, headers, request.post_data->data(),
                                   request.post_data->size(), request.content_type.c_str())
                : http_client.Get(request_path, headers);

        if (result) {
            callback({
                .body = std::move(result->body),
                .status = result->status,
                .success = true,
            });
        } else {
            callback({
                .body = httplib::to_string(result.error()),
                .status = 0,
                .success = false,
            });
        }
    });
}

void Client::RegisterObserver(ClientObserver& observer) {
    m_observers.push_back(&observer);
}

void Client::SetEnabled(bool enabled) {
    m_enabled = enabled;
}

void Client::AttemptLogin(const char* username, const char* password) {
    if (!m_enabled)
        return;

    rc_client_begin_login_with_password(m_rc_client, username, password, login_callback, this);
}

void Client::AttemptLoginWithToken(const char* username, const char* token) {
    if (!m_enabled)
        return;

    rc_client_begin_login_with_token(m_rc_client, username, token, login_callback, this);
}

void Client::LogOut() {
    rc_client_logout(m_rc_client);
    m_user.reset();
    m_game.reset();
}

void Client::LoadGame(const char* file_path) {
    if (!m_enabled)
        return;

    m_game.reset();
    rc_client_begin_identify_and_load_game(m_rc_client, RC_CONSOLE_NINTENDO_3DS, file_path, NULL, 0,
                                           load_game_callback, this);
}

void Client::UnloadGame() {
    rc_client_unload_game(m_rc_client);
    m_game.reset();
}

void Client::Reset() {
    rc_client_reset(m_rc_client);
}

void Client::DoFrame() {
    if (!m_enabled) {
        UnloadGame();
        return;
    }

    rc_client_do_frame(m_rc_client);
}

void Client::FetchImage(std::string url, ImageCallback callback) {
    if (!m_enabled)
        return;

    HttpRequest request = {
        .url = url,
        .post_data = std::nullopt,
        .content_type = "",
    };
    QueueHttpRequest(std::move(request), [callback = std::move(callback)](HttpResponse&& response) {
        std::vector<uint8_t> image_data;
        if (response.success) {
            image_data.assign(response.body.begin(), response.body.end());
        } else {
            LOG_ERROR(RetroAchievements, "Image fetch failed: {}", response.body);
        }

        callback(std::move(image_data));
    });
}

const std::optional<User>& Client::GetUser() const {
    return m_user;
}

const std::optional<Game>& Client::GetGame() const {
    return m_game;
}

std::vector<Achievement> Client::GetAchievementList() {
    rc_client_achievement_list_t* rc_list = rc_client_create_achievement_list(
        m_rc_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
        RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);

    std::vector<Achievement> list;
    if (!rc_list) {
        return list;
    }

    for (uint32_t bucket_index = 0; bucket_index < rc_list->num_buckets; ++bucket_index) {
        const rc_client_achievement_bucket_t& rc_bucket = rc_list->buckets[bucket_index];

        for (uint32_t achievement_index = 0; achievement_index < rc_bucket.num_achievements;
             ++achievement_index) {
            const rc_client_achievement_t* rc_achievement =
                rc_bucket.achievements[achievement_index];
            if (!rc_achievement) {
                continue;
            }

            list.emplace_back(Achievement{
                .id = rc_achievement->id,
                .title = rc_achievement->title,
                .description = rc_achievement->description,
                .points = rc_achievement->points,
                .state = rc_achievement->state,
                .category = rc_achievement->category,
                .type = rc_achievement->type,
                .bucket = rc_achievement->bucket,
                .unlocked = rc_achievement->unlocked,
                .measured_progress = rc_achievement->measured_progress,
                .measured_percent = rc_achievement->measured_percent,
                .rarity = rc_achievement->rarity,
                .rarity_hardcore = rc_achievement->rarity_hardcore,
                .badge_url = rc_achievement->badge_url,
                .badge_locked_url = rc_achievement->badge_locked_url,
            });
        }
    }

    rc_client_destroy_achievement_list(rc_list);

    return list;
}

void Client::OnLoginCallback(int result, std::string_view error_message) {
    if (!m_enabled)
        return;

    if (result == 0) {
        const rc_client_user_t* rc_user = rc_client_get_user_info(m_rc_client);
        User user = {
            .display_name = rc_user->display_name,
            .username = rc_user->username,
            .token = rc_user->token,
            .score = rc_user->score,
            .avatar_url = rc_user->avatar_url,
        };

        m_user.emplace(user);

        for (ClientObserver* observer : m_observers) {
            observer->OnLoginSucceeded(user);
        }
    } else {
        for (ClientObserver* observer : m_observers) {
            observer->OnLoginFailed(result, error_message);
        }
    }
}

void Client::OnLoadGameCallback(int result, std::string_view error_message) {
    if (!m_enabled)
        return;

    if (result == 0) {
        const rc_client_game_t* rc_game = rc_client_get_game_info(m_rc_client);
        Game game = {
            .title = rc_game->title,
            .badge_url = rc_game->badge_url,
        };

        m_game.emplace(game);

        for (ClientObserver* observer : m_observers) {
            observer->OnLoadGameSucceeded(game);
        }
    } else {
        m_game.reset();
        for (ClientObserver* observer : m_observers) {
            observer->OnLoadGameFailed(result, error_message);
        }
    }
}

void Client::OnEvent(const rc_client_event_t* event) {
    if (!m_enabled)
        return;

    LOG_DEBUG(RetroAchievements, "Event! ({})", event->type);
    for (ClientObserver* observer : m_observers) {
        observer->OnEvent(event);
    }
}

} // namespace RetroAchievements
