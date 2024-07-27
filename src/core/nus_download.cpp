// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <httplib.h>
#include "common/logging/log.h"
#include "core/nus_download.h"

namespace Core::NUS {

std::optional<std::vector<u8>> Download(const std::string& path) {
    constexpr auto HOST = "http://nus.cdn.c.shop.nintendowifi.net";

    std::unique_ptr<httplib::Client> client = std::make_unique<httplib::Client>(HOST);
    if (client == nullptr) {
        LOG_ERROR(WebService, "Invalid URL {}{}", HOST, path);
        return {};
    }

    httplib::Request request{
        .method = "GET",
        .path = path,
        // Needed when httplib is included on android
        .matches = httplib::Match(),
    };

    client->set_follow_location(true);
    const auto result = client->send(request);
    if (!result) {
        LOG_ERROR(WebService, "GET to {}{} returned null", HOST, path);
        return {};
    }

    const auto& response = result.value();
    if (response.status >= 400) {
        LOG_ERROR(WebService, "GET to {}{} returned error status code: {}", HOST, path,
                  response.status);
        return {};
    }
    if (!response.headers.contains("content-type")) {
        LOG_ERROR(WebService, "GET to {}{} returned no content", HOST, path);
        return {};
    }

    return std::vector<u8>(response.body.begin(), response.body.end());
}

} // namespace Core::NUS
