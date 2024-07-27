// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

namespace Common {
struct WebResult;
}

namespace WebService {

class Client {
public:
    Client(std::string host, std::string username, std::string token);
    ~Client();

    /**
     * Posts JSON to the specified path.
     * @param path the URL segment after the host address.
     * @param data String of JSON data to use for the body of the POST request.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult PostJson(const std::string& path, const std::string& data,
                               bool allow_anonymous);

    /**
     * Gets JSON from the specified path.
     * @param path the URL segment after the host address.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult GetJson(const std::string& path, bool allow_anonymous);

    /**
     * Deletes JSON to the specified path.
     * @param path the URL segment after the host address.
     * @param data String of JSON data to use for the body of the DELETE request.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult DeleteJson(const std::string& path, const std::string& data,
                                 bool allow_anonymous);

    /**
     * Gets a plain string from the specified path.
     * @param path the URL segment after the host address.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult GetPlain(const std::string& path, bool allow_anonymous);

    /**
     * Gets an PNG image from the specified path.
     * @param path the URL segment after the host address.
     * @param allow_anonymous If true, allow anonymous unauthenticated requests.
     * @return the result of the request.
     */
    Common::WebResult GetImage(const std::string& path, bool allow_anonymous);

    /**
     * Requests an external JWT for the specific audience provided.
     * @param audience the audience of the JWT requested.
     * @return the result of the request.
     */
    Common::WebResult GetExternalJWT(const std::string& audience);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace WebService
