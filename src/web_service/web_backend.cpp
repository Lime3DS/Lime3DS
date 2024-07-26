// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstdlib>
#include <mutex>
#include <string>
#include <fmt/format.h>
#include <httplib.h>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/web_result.h"
#include "web_service/web_backend.h"

namespace WebService {

constexpr std::array<const char, 1> API_VERSION{'1'};

constexpr std::size_t TIMEOUT_SECONDS = 30;

struct Client::Impl {
    Impl(std::string host, std::string username, std::string token)
        : host{std::move(host)}, username{std::move(username)}, token{std::move(token)} {
        std::lock_guard lock{jwt_cache.mutex};
        if (this->username == jwt_cache.username && this->token == jwt_cache.token) {
            jwt = jwt_cache.jwt;
        }
        // normalize host expression
        if (!this->host.empty() && this->host.back() == '/') {
            static_cast<void>(this->host.pop_back());
        }
    }

    /// A generic function handles POST, GET and DELETE request together
    Common::WebResult GenericRequest(const std::string& method, const std::string& path,
                                     const std::string& data, bool allow_anonymous,
                                     const std::string& accept) {
        if (jwt.empty()) {
            UpdateJWT();
        }

        if (jwt.empty() && !allow_anonymous) {
            LOG_ERROR(WebService, "Credentials must be provided for authenticated requests");
            return Common::WebResult{Common::WebResult::Code::CredentialsMissing,
                                     "Credentials needed"};
        }

        auto result = GenericRequest(method, path, data, accept, jwt);
        if (result.result_string == "401") {
            // Try again with new JWT
            UpdateJWT();
            result = GenericRequest(method, path, data, accept, jwt);
        }

        return result;
    }

    /**
     * A generic function with explicit authentication method specified
     * JWT is used if the jwt parameter is not empty
     * username + token is used if jwt is empty but username and token are
     * not empty anonymous if all of jwt, username and token are empty
     */
    Common::WebResult GenericRequest(const std::string& method, const std::string& path,
                                     const std::string& data, const std::string& accept,
                                     const std::string& jwt = "", const std::string& username = "",
                                     const std::string& token = "") {
        if (cli == nullptr) {
            cli = std::make_unique<httplib::Client>(host.c_str());
            cli->set_connection_timeout(TIMEOUT_SECONDS);
            cli->set_read_timeout(TIMEOUT_SECONDS);
            cli->set_write_timeout(TIMEOUT_SECONDS);
        }
        if (!cli->is_valid()) {
            LOG_ERROR(WebService, "Invalid URL {}", host + path);
            return Common::WebResult{Common::WebResult::Code::InvalidURL, "Invalid URL"};
        }

        httplib::Headers params;
        if (!jwt.empty()) {
            params = {
                {std::string("Authorization"), fmt::format("Bearer {}", jwt)},
            };
        } else if (!username.empty()) {
            params = {
                {std::string("x-username"), username},
                {std::string("x-token"), token},
            };
        }

        params.emplace(std::string("api-version"),
                       std::string(API_VERSION.begin(), API_VERSION.end()));
        if (method != "GET") {
            params.emplace(std::string("Content-Type"), std::string("application/json"));
        };

        httplib::Request request;
        request.method = method;
        request.path = path;
        request.headers = params;
        request.body = data;

        httplib::Result result = cli->send(request);

        if (!result) {
            LOG_ERROR(WebService, "{} to {} returned null", method, host + path);
            return Common::WebResult{Common::WebResult::Code::LibError, "Null response"};
        }

        httplib::Response response = result.value();

        if (response.status >= 400) {
            LOG_ERROR(WebService, "{} to {} returned error status code: {}", method, host + path,
                      response.status);
            return Common::WebResult{Common::WebResult::Code::HttpError,
                                     std::to_string(response.status)};
        }

        auto content_type = response.headers.find("content-type");

        if (content_type == response.headers.end()) {
            LOG_ERROR(WebService, "{} to {} returned no content", method, host + path);
            return Common::WebResult{Common::WebResult::Code::WrongContent, ""};
        }

        if (content_type->second.find(accept) == std::string::npos) {
            LOG_ERROR(WebService, "{} to {} returned wrong content: {}", method, host + path,
                      content_type->second);
            return Common::WebResult{Common::WebResult::Code::WrongContent, "Wrong content"};
        }
        return Common::WebResult{Common::WebResult::Code::Success, "", response.body};
    }

    // Retrieve a new JWT from given username and token
    void UpdateJWT() {
        if (username.empty() || token.empty()) {
            return;
        }

        auto result = GenericRequest("POST", "/jwt/internal", "", "text/html", "", username, token);
        if (result.result_code != Common::WebResult::Code::Success) {
            LOG_ERROR(WebService, "UpdateJWT failed");
        } else {
            std::lock_guard lock{jwt_cache.mutex};
            jwt_cache.username = username;
            jwt_cache.token = token;
            jwt_cache.jwt = jwt = result.returned_data;
        }
    }

    std::string host;
    std::string username;
    std::string token;
    std::string jwt;
    std::unique_ptr<httplib::Client> cli;

    struct JWTCache {
        std::mutex mutex;
        std::string username;
        std::string token;
        std::string jwt;
    };
    static inline JWTCache jwt_cache;
};

Client::Client(std::string host, std::string username, std::string token)
    : impl{std::make_unique<Impl>(std::move(host), std::move(username), std::move(token))} {}

Client::~Client() = default;

Common::WebResult Client::PostJson(const std::string& path, const std::string& data,
                                   bool allow_anonymous) {
    return impl->GenericRequest("POST", path, data, allow_anonymous, "application/json");
}

Common::WebResult Client::GetJson(const std::string& path, bool allow_anonymous) {
    return impl->GenericRequest("GET", path, "", allow_anonymous, "application/json");
}

Common::WebResult Client::DeleteJson(const std::string& path, const std::string& data,
                                     bool allow_anonymous) {
    return impl->GenericRequest("DELETE", path, data, allow_anonymous, "application/json");
}

Common::WebResult Client::GetPlain(const std::string& path, bool allow_anonymous) {
    return impl->GenericRequest("GET", path, "", allow_anonymous, "text/plain");
}

Common::WebResult Client::GetImage(const std::string& path, bool allow_anonymous) {
    return impl->GenericRequest("GET", path, "", allow_anonymous, "image/png");
}

Common::WebResult Client::GetExternalJWT(const std::string& audience) {
    return impl->GenericRequest("POST", fmt::format("/jwt/external/{}", audience), "", false,
                                "text/html");
}

} // namespace WebService
