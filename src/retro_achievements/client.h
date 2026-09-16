// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <rc_client.h>

#include "common/thread_worker.h"

#include "client_observer.h"
#include "models.h"

namespace RetroAchievements {

class Client {
public:
    explicit Client();
    ~Client();

    void RegisterObserver(ClientObserver& observer);

    void SetEnabled(bool enabled);

    void AttemptLogin(const char* username, const char* password);
    void AttemptLoginWithToken(const char* username, const char* token);
    void LogOut();

    void LoadGame(const char* file_path);
    void UnloadGame();
    void Reset();
    void DoFrame();

    using ImageCallback = std::function<void(std::vector<uint8_t>&& image_data)>;
    void FetchImage(std::string url, ImageCallback callback);

    const std::optional<User>& GetUser() const;
    const std::optional<Game>& GetGame() const;
    std::vector<Achievement> GetAchievementList();

    void OnLoginCallback(int result, std::string_view error_message);
    void OnLoadGameCallback(int result, std::string_view error_message);
    void OnEvent(const rc_client_event_t* event);

    struct HttpRequest {
        std::string url;
        std::optional<std::string> post_data;
        std::string content_type;
    };

    struct HttpResponse {
        std::string body;
        int status;
        bool success;
    };

    using HttpCallback = std::function<void(HttpResponse&& response)>;
    void QueueHttpRequest(HttpRequest&& request, HttpCallback callback);

private:
    rc_client_t* m_rc_client;
    std::atomic_bool m_enabled = false;

    std::optional<User> m_user = std::nullopt;
    std::optional<Game> m_game = std::nullopt;

    std::vector<ClientObserver*> m_observers;

    Common::ThreadWorker m_http_worker{1, "RetroAchievements_HTTP"};
};

} // namespace RetroAchievements
