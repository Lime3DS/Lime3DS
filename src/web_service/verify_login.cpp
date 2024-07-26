// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <json.hpp>
#include "common/web_result.h"
#include "web_service/verify_login.h"
#include "web_service/web_backend.h"

namespace WebService {

bool VerifyLogin(const std::string& host, const std::string& username, const std::string& token) {
    Client client(host, username, token);
    auto reply = client.GetJson("/profile", false).returned_data;
    if (reply.empty()) {
        return false;
    }
    nlohmann::json json = nlohmann::json::parse(reply);
    const auto iter = json.find("username");

    if (iter == json.end()) {
        return username.empty();
    }

    return *iter == username;
}

} // namespace WebService
