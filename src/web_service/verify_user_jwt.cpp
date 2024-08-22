// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <system_error>
#include <jwt/jwt.hpp>
#include "common/logging/log.h"
#include "common/web_result.h"
#include "web_service/verify_user_jwt.h"
#include "web_service/web_backend.h"

namespace WebService {

static std::string public_key;
std::string GetPublicKey(const std::string& host) {
    if (public_key.empty()) {
        Client client(host, "", ""); // no need for credentials here
        public_key = client.GetPlain("/jwt/external/key.pem", true).returned_data;
        if (public_key.empty()) {
            LOG_ERROR(WebService, "Could not fetch external JWT public key, verification may fail");
        } else {
            LOG_INFO(WebService, "Fetched external JWT public key (size={})", public_key.size());
        }
    }
    return public_key;
}

VerifyUserJWT::VerifyUserJWT(const std::string& host) : pub_key(GetPublicKey(host)) {}

Network::VerifyUser::UserData VerifyUserJWT::LoadUserData(const std::string& verify_UID,
                                                          const std::string& token) {
    const std::string audience = fmt::format("external-{}", verify_UID);
    using namespace jwt::params;
    std::error_code error;
    auto decoded =
        jwt::decode(token, algorithms({"rs256"}), error, secret(pub_key), issuer("lime3ds-core"),
                    aud(audience), validate_iat(true), validate_jti(true));
    if (error) {
        LOG_INFO(WebService, "Verification failed: category={}, code={}, message={}",
                 error.category().name(), error.value(), error.message());
        return {};
    }
    Network::VerifyUser::UserData user_data{};
    if (decoded.payload().has_claim("username")) {
        user_data.username = decoded.payload().get_claim_value<std::string>("username");
    }
    if (decoded.payload().has_claim("displayName")) {
        user_data.display_name = decoded.payload().get_claim_value<std::string>("displayName");
    }
    if (decoded.payload().has_claim("avatarUrl")) {
        user_data.avatar_url = decoded.payload().get_claim_value<std::string>("avatarUrl");
    }
    if (decoded.payload().has_claim("roles")) {
        auto roles = decoded.payload().get_claim_value<std::vector<std::string>>("roles");
        user_data.moderator = std::find(roles.begin(), roles.end(), "moderator") != roles.end();
    }
    return user_data;
}

} // namespace WebService
