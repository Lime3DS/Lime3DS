// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "artic_base_client.h"
#include "common/assert.h"
#include "common/logging/log.h"

#include "chrono"
#include "limits.h"
#include "memory"
#include "sstream"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#define WSAEAGAIN WSAEWOULDBLOCK
#define WSAEMULTIHOP -1 // Invalid dummy value
#define ERRNO(x) WSA##x
#define GET_ERRNO WSAGetLastError()
#define poll(x, y, z) WSAPoll(x, y, z);
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#else
#define ERRNO(x) x
#define GET_ERRNO errno
#define closesocket(x) close(x)
#endif

// #define DISABLE_PING_TIMEOUT

namespace Network::ArticBase {

using namespace std::chrono_literals;

bool Client::Request::AddParameterS8(s8 parameter) {
    if (parameters.size() >= max_param_count) {
        LOG_ERROR(Network, "Too many parameters added to method: {}", method_name);
        return false;
    }
    auto& param = parameters.emplace_back();
    param.type = ArticBaseCommon::RequestParameterType::IN_INTEGER_8;
    std::memcpy(param.data, &parameter, sizeof(s8));
    return true;
}

bool Client::Request::AddParameterS16(s16 parameter) {
    if (parameters.size() >= max_param_count) {
        LOG_ERROR(Network, "Too many parameters added to method: {}", method_name);
        return false;
    }
    auto& param = parameters.emplace_back();
    param.type = ArticBaseCommon::RequestParameterType::IN_INTEGER_16;
    std::memcpy(param.data, &parameter, sizeof(s16));
    return true;
}

bool Client::Request::AddParameterS32(s32 parameter) {
    if (parameters.size() >= max_param_count) {
        LOG_ERROR(Network, "Too many parameters added to method: {}", method_name);
        return false;
    }
    auto& param = parameters.emplace_back();
    param.type = ArticBaseCommon::RequestParameterType::IN_INTEGER_32;
    std::memcpy(param.data, &parameter, sizeof(s32));
    return true;
}

bool Client::Request::AddParameterS64(s64 parameter) {
    if (parameters.size() >= max_param_count) {
        LOG_ERROR(Network, "Too many parameters added to method: {}", method_name);
        return false;
    }
    auto& param = parameters.emplace_back();
    param.type = ArticBaseCommon::RequestParameterType::IN_INTEGER_64;
    std::memcpy(param.data, &parameter, sizeof(s64));
    return true;
}

bool Client::Request::AddParameterBuffer(const void* buffer, size_t size) {
    if (parameters.size() >= max_param_count) {
        LOG_ERROR(Network, "Too many parameters added to method: {}", method_name);
        return false;
    }
    auto& param = parameters.emplace_back();
    if (size <= sizeof(param.data)) {
        param.type = ArticBaseCommon::RequestParameterType::IN_SMALL_BUFFER;
        std::memcpy(param.data, buffer, size);
        param.parameterSize = static_cast<u16>(size);
    } else {
        param.type = ArticBaseCommon::RequestParameterType::IN_BIG_BUFFER;
        param.bigBufferID = static_cast<u16>(pending_big_buffers.size());
        s32 size_32 = static_cast<s32>(size);
        std::memcpy(param.data, &size_32, sizeof(size_32));
        pending_big_buffers.push_back(std::make_pair(buffer, size));
    }
    return true;
}

Client::Request::Request(u32 request_id, const std::string& method, size_t max_params) {
    method_name = method;
    max_param_count = max_params;
    request_packet.requestID = request_id;
    std::memcpy(request_packet.method.data(), method.data(),
                std::min<size_t>(request_packet.method.size(), method.size()));
}

Client::~Client() {
    StopImpl(false);

    for (auto it = handlers.begin(); it != handlers.end(); it++) {
        Handler* h = *it;
        h->thread->join();
        delete h;
    }

    if (ping_thread.joinable()) {
        ping_thread.join();
    }

    SocketManager::DisableSockets();
}

bool Client::Connect() {
    if (connected)
        return true;

    auto str_to_int = [](const std::string& str) -> int {
        char* pEnd = NULL;
        unsigned long ul = ::strtoul(str.c_str(), &pEnd, 10);
        if (*pEnd)
            return -1;
        return static_cast<int>(ul);
    };

    struct addrinfo hints, *addrinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    LOG_INFO(Network, "Starting Artic Base Client");

    if (getaddrinfo(address.data(), NULL, &hints, &addrinfo) != 0) {
        LOG_ERROR(Network, "Failed to get server address");
        SignalCommunicationError();
        return false;
    }

    main_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket == -1) {
        LOG_ERROR(Network, "Failed to create socket");
        SignalCommunicationError();
        return false;
    }

    if (!SetNonBlock(main_socket, true)) {
        shutdown(main_socket, SHUT_RDWR);
        closesocket(main_socket);
        LOG_ERROR(Network, "Cannot set non-blocking socket mode");
        SignalCommunicationError();
        return false;
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ((struct sockaddr_in*)(addrinfo->ai_addr))->sin_addr.s_addr;
    servaddr.sin_port = htons(port);
    freeaddrinfo(addrinfo);

    if (!ConnectWithTimeout(main_socket, &servaddr, sizeof(servaddr), 10)) {
        closesocket(main_socket);
        LOG_ERROR(Network, "Failed to connect");
        SignalCommunicationError();
        return false;
    }

    auto version = SendSimpleRequest("VERSION");
    if (version.has_value()) {
        int version_value = str_to_int(*version);
        if (version_value != SERVER_VERSION) {
            shutdown(main_socket, SHUT_RDWR);
            closesocket(main_socket);
            LOG_ERROR(Network, "Incompatible server version: {}", version_value);
            SignalCommunicationError();
            return false;
        }
    } else {
        shutdown(main_socket, SHUT_RDWR);
        closesocket(main_socket);
        LOG_ERROR(Network, "Couldn't fetch server version.");
        SignalCommunicationError();
        return false;
    }

    auto max_work_size = SendSimpleRequest("MAXSIZE");
    int max_work_size_value = -1;
    if (max_work_size.has_value()) {
        max_work_size_value = str_to_int(*max_work_size);
    }
    if (max_work_size_value < 0) {
        shutdown(main_socket, SHUT_RDWR);
        closesocket(main_socket);
        LOG_ERROR(Network, "Couldn't fetch server work ram size");
        SignalCommunicationError();
        return false;
    }
    max_server_work_ram = max_work_size_value;

    auto max_params = SendSimpleRequest("MAXPARAM");
    int max_param_value = -1;
    if (max_params.has_value()) {
        max_param_value = str_to_int(*max_params);
    }
    if (max_param_value < 0) {
        shutdown(main_socket, SHUT_RDWR);
        closesocket(main_socket);
        LOG_ERROR(Network, "Couldn't fetch server max params");
        SignalCommunicationError();
        return false;
    }
    max_parameter_count = max_param_value;

    auto worker_ports = SendSimpleRequest("PORTS");
    if (!worker_ports.has_value()) {
        shutdown(main_socket, SHUT_RDWR);
        closesocket(main_socket);
        LOG_ERROR(Network, "Couldn't fetch server worker ports");
        SignalCommunicationError();
        return false;
    }
    std::vector<u16> ports;
    std::string str_port;
    std::stringstream ss_port(worker_ports.value());
    while (std::getline(ss_port, str_port, ',')) {
        int port = str_to_int(str_port);
        if (port < 0 || port > USHRT_MAX) {
            shutdown(main_socket, SHUT_RDWR);
            closesocket(main_socket);
            LOG_ERROR(Network, "Couldn't parse server worker ports");
            SignalCommunicationError();
            return false;
        }
        ports.push_back(static_cast<u16>(port));
    }
    if (ports.empty()) {
        shutdown(main_socket, SHUT_RDWR);
        closesocket(main_socket);
        LOG_ERROR(Network, "Couldn't parse server worker ports");
        SignalCommunicationError();
        return false;
    }

    for (int i = 0; i < 101; i++) {
        auto ready_server = SendSimpleRequest("READY");
        if (!ready_server.has_value() || i == 100) {
            shutdown(main_socket, SHUT_RDWR);
            closesocket(main_socket);
            LOG_ERROR(Network, "Couldn't fetch server readiness");
            SignalCommunicationError();
            return false;
        }
        if (*ready_server == "1")
            break;
        std::this_thread::sleep_for(100ms);
    }

    ping_thread = std::thread(&Client::PingFunction, this);

    int i = 0;
    running_handlers = ports.size();
    for (auto it = ports.begin(); it != ports.end(); it++) {
        handlers.push_back(new Handler(*this, static_cast<u32>(servaddr.sin_addr.s_addr), *it, i));
        i++;
    }

    connected = true;
    return true;
}

void Client::StopImpl(bool from_error) {
    bool expected = false;
    if (!stopped.compare_exchange_strong(expected, true))
        return;

    if (!from_error) {
        SendSimpleRequest("STOP");
    }

    if (ping_thread.joinable()) {
        std::scoped_lock l2(ping_cv_mutex);
        ping_run = false;
        ping_cv.notify_one();
    }

    // Stop handlers
    for (auto it = handlers.begin(); it != handlers.end(); it++) {
        Handler* handler = *it;
        handler->should_run = false;
        // Shouldn't matter if the socket is shut down twice
        shutdown(handler->handler_socket, SHUT_RDWR);
        closesocket(handler->handler_socket);
    }

    // Close main socket
    shutdown(main_socket, SHUT_RDWR);
    closesocket(main_socket);
}

std::optional<std::pair<void*, size_t>> Client::Response::GetResponseBuffer(u32 buffer_id) const {
    if (!resp_data_buffer)
        return std::nullopt;

    char* resp_data_buffer_end = resp_data_buffer + resp_data_size;
    char* resp_data_buffer_start = resp_data_buffer;
    while (resp_data_buffer_start + sizeof(ArticBaseCommon::Buffer) < resp_data_buffer_end) {
        ArticBaseCommon::Buffer* curr_buffer =
            reinterpret_cast<ArticBaseCommon::Buffer*>(resp_data_buffer_start);
        resp_data_buffer_start += sizeof(ArticBaseCommon::Buffer);
        if (curr_buffer->bufferID == buffer_id) {
            if (curr_buffer->data + curr_buffer->bufferSize <= resp_data_buffer_end) {
                return std::make_pair(curr_buffer->data, curr_buffer->bufferSize);
            } else {
                return std::nullopt;
            }
        }
        resp_data_buffer_start += curr_buffer->bufferSize;
    }
    return std::nullopt;
}

std::optional<Client::Response> Client::Send(Request& request) {
    if (stopped)
        return std::nullopt;

    request.request_packet.parameterCount = static_cast<u32>(request.parameters.size());
    PendingResponse resp(request);

    {
        std::scoped_lock l(recv_map_mutex);
        pending_responses[request.request_packet.requestID] = &resp;
    }

    auto respPacket = SendRequestPacket(request.request_packet, false, request.parameters);
    if (stopped || !respPacket.has_value()) {
        std::scoped_lock l(recv_map_mutex);
        pending_responses.erase(request.request_packet.requestID);
        return std::nullopt;
    }

    std::unique_lock cv_lk(resp.cv_mutex);
    resp.cv.wait(cv_lk, [&resp]() { return resp.is_done; });

    return std::optional<Client::Response>(std::move(resp.response));
}

void Client::SignalCommunicationError() {
    StopImpl(true);
    LOG_CRITICAL(Network, "Communication error");
    if (communication_error_callback)
        communication_error_callback();
}

void Client::PingFunction() {
    // Max silence time => 7 secs interval + 3 secs wait + 10 seconds timeout = 25 seconds
    while (ping_run) {
        std::chrono::time_point<std::chrono::steady_clock> last = last_sent_request;
        if (std::chrono::steady_clock::now() - last > std::chrono::seconds(7)) {
#ifdef DISABLE_PING_TIMEOUT
            client->last_sent_request = std::chrono::steady_clock::now();
#else
            auto ping_reply = SendSimpleRequest("PING");
            if (!ping_reply.has_value()) {
                SignalCommunicationError();
                break;
            }
#endif // DISABLE_PING_TIMEOUT
        }

        std::unique_lock lk(ping_cv_mutex);
        ping_cv.wait_for(lk, std::chrono::seconds(3));
    }
}

bool Client::ConnectWithTimeout(SocketHolder sockFD, void* server_addr, size_t server_addr_len,
                                int timeout_seconds) {

    int res = ::connect(sockFD, (struct sockaddr*)server_addr, static_cast<int>(server_addr_len));
    if (res == -1 && ((GET_ERRNO == ERRNO(EINPROGRESS) || GET_ERRNO == ERRNO(EWOULDBLOCK)))) {
        struct timeval tv;
        fd_set fdset;
        FD_ZERO(&fdset);
        FD_SET(sockFD, &fdset);

        tv.tv_sec = timeout_seconds;
        tv.tv_usec = 0;
        int select_res = ::select(static_cast<int>(sockFD + 1), NULL, &fdset, NULL, &tv);
#ifdef _WIN32
        if (select_res == 0) {
            return false;
        }
#else
        bool select_good = false;
        if (select_res == 1) {
            int so_error;
            socklen_t len = sizeof so_error;

            getsockopt(sockFD, SOL_SOCKET, SO_ERROR, &so_error, &len);

            if (so_error == 0) {
                select_good = true;
            }
        }
        if (!select_good) {
            return false;
        }
#endif // _WIN32

    } else if (res == -1) {
        return false;
    }
    return true;
}

bool Client::SetNonBlock(SocketHolder sockFD, bool nonBlocking) {
    bool blocking = !nonBlocking;
#ifdef _WIN32
    unsigned long nonblocking = (blocking) ? 0 : 1;
    int ret = ioctlsocket(sockFD, FIONBIO, &nonblocking);
    if (ret == -1) {
        return false;
    }
#else
    int flags = ::fcntl(sockFD, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }

    flags &= ~O_NONBLOCK;
    if (!blocking) { // O_NONBLOCK
        flags |= O_NONBLOCK;
    }

    const int ret = ::fcntl(sockFD, F_SETFL, flags);
    if (ret == -1) {
        return false;
    }
#endif
    return true;
}

bool Client::Read(SocketHolder sockFD, void* buffer, size_t size,
                  const std::chrono::nanoseconds& timeout) {
    size_t read_bytes = 0;
    auto before = std::chrono::steady_clock::now();
    while (read_bytes != size) {
        int new_read =
            ::recv(sockFD, (char*)((uintptr_t)buffer + read_bytes), (int)(size - read_bytes), 0);
        if (new_read < 0) {
            if (GET_ERRNO == ERRNO(EWOULDBLOCK) &&
                (timeout == std::chrono::nanoseconds(0) ||
                 std::chrono::steady_clock::now() - before < timeout)) {
                continue;
            }
            read_bytes = 0;
            break;
        }
        if (report_traffic_callback && new_read) {
            report_traffic_callback(new_read);
        }
        read_bytes += new_read;
    }
    return read_bytes == size;
}

bool Client::Write(SocketHolder sockFD, const void* buffer, size_t size,
                   const std::chrono::nanoseconds& timeout) {
    size_t write_bytes = 0;
    auto before = std::chrono::steady_clock::now();
    while (write_bytes != size) {
        int new_written = ::send(sockFD, (const char*)((uintptr_t)buffer + write_bytes),
                                 (int)(size - write_bytes), 0);
        if (new_written < 0) {
            if (GET_ERRNO == ERRNO(EWOULDBLOCK) &&
                (timeout == std::chrono::nanoseconds(0) ||
                 std::chrono::steady_clock::now() - before < timeout)) {
                continue;
            }
            write_bytes = 0;
            break;
        }
        if (report_traffic_callback && new_written) {
            report_traffic_callback(new_written);
        }
        write_bytes += new_written;
    }
    return write_bytes == size;
}

std::optional<ArticBaseCommon::DataPacket> Client::SendRequestPacket(
    const ArticBaseCommon::RequestPacket& req, bool expect_response,
    const std::vector<ArticBaseCommon::RequestParameter>& params,
    const std::chrono::nanoseconds& read_timeout) {
    std::scoped_lock<std::mutex> l(send_mutex);

    if (main_socket == -1) {
        return std::nullopt;
    }

    if (!Write(main_socket, &req, sizeof(req))) {
        LOG_WARNING(Network, "Failed to write to socket");
        SignalCommunicationError();
        return std::nullopt;
    }

    if (!params.empty()) {
        if (!Write(main_socket, params.data(),
                   params.size() * sizeof(ArticBaseCommon::RequestParameter))) {
            LOG_WARNING(Network, "Failed to write to socket");
            SignalCommunicationError();
            return std::nullopt;
        }
    }

    ArticBaseCommon::DataPacket resp;
    if (expect_response) {
        if (!Read(main_socket, &resp, sizeof(resp), read_timeout)) {
            LOG_WARNING(Network, "Failed to read from socket");
            SignalCommunicationError();
            return std::nullopt;
        }

        if (resp.requestID != req.requestID) {
            return std::nullopt;
        }
    }

    last_sent_request = std::chrono::steady_clock::now();
    return resp;
}

std::optional<std::string> Client::SendSimpleRequest(const std::string& method) {
    ArticBaseCommon::RequestPacket req{};
    req.requestID = GetNextRequestID();
    const std::string final_method = "$" + method;
    if (final_method.size() > sizeof(req.method)) {
        return std::nullopt;
    }
    std::memcpy(req.method.data(), final_method.data(), final_method.size());
    auto resp = SendRequestPacket(req, true, {}, std::chrono::seconds(10));
    if (!resp.has_value() || resp->requestID != req.requestID) {
        return std::nullopt;
    }
    char respBody[sizeof(ArticBaseCommon::DataPacket::dataRaw) + 1] = {0};
    std::memcpy(respBody, resp->dataRaw, sizeof(ArticBaseCommon::DataPacket::dataRaw));
    return respBody;
}

Client::Handler::Handler(Client& _client, u32 _addr, u16 _port, int _id)
    : id(_id), client(_client), addr(_addr), port(_port) {
    thread = new std::thread(
        [](Handler* handler) {
            handler->RunLoop();
            handler->should_run = false;
            if (--handler->client.running_handlers == 0) {
                handler->client.OnAllHandlersFinished();
            }
        },
        this);
}

void Client::Handler::RunLoop() {
    handler_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (handler_socket == -1) {
        LOG_ERROR(Network, "Failed to create socket");
        return;
    }

    if (!SetNonBlock(handler_socket, true)) {
        closesocket(handler_socket);
        client.SignalCommunicationError();
        LOG_ERROR(Network, "Cannot set non-blocking socket mode");
        return;
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = static_cast<decltype(servaddr.sin_addr.s_addr)>(addr);
    servaddr.sin_port = htons(port);

    if (!ConnectWithTimeout(handler_socket, &servaddr, sizeof(servaddr), 10)) {
        closesocket(handler_socket);
        LOG_ERROR(Network, "Failed to connect");
        client.SignalCommunicationError();
        return;
    }

    const auto signal_error = [&] {
        if (should_run) {
            client.SignalCommunicationError();
        }
    };

    ArticBaseCommon::DataPacket dataPacket;
    u32 retry_count = 0;
    while (should_run) {
        if (!client.Read(handler_socket, &dataPacket, sizeof(dataPacket))) {
            if (should_run) {
                LOG_WARNING(Network, "Failed to read from socket");
                std::this_thread::sleep_for(100ms);
                if (++retry_count == 300) {
                    signal_error();
                    break;
                }
                continue;
            } else {
                break;
            }
        }
        retry_count = 0;

        PendingResponse* pending_response;
        {
            std::scoped_lock l(client.recv_map_mutex);
            auto it = client.pending_responses.find(dataPacket.requestID);
            if (it == client.pending_responses.end()) {
                continue;
            }
            pending_response = it->second;
        }

        switch (dataPacket.resp.articResult) {
        case ArticBaseCommon::ResponseMethod::ArticResult::SUCCESS: {
            pending_response->response.articResult = dataPacket.resp.articResult;
            pending_response->response.methodResult = dataPacket.resp.methodResult;
            if (dataPacket.resp.bufferSize) {
                pending_response->response.resp_data_buffer =
                    reinterpret_cast<char*>(operator new(dataPacket.resp.bufferSize));
                ASSERT_MSG(pending_response->response.resp_data_buffer != nullptr,
                           "ArticBase Handler: Cannot allocate buffer");
                pending_response->response.resp_data_size =
                    static_cast<size_t>(dataPacket.resp.bufferSize);
                if (!client.Read(handler_socket, pending_response->response.resp_data_buffer,
                                 dataPacket.resp.bufferSize)) {
                    signal_error();
                }
            }
        } break;
        case ArticBaseCommon::ResponseMethod::ArticResult::METHOD_NOT_FOUND: {
            LOG_ERROR(Network, "Method {} not found by server",
                      pending_response->request.method_name);
            pending_response->response.articResult = dataPacket.resp.articResult;
        } break;

        case ArticBaseCommon::ResponseMethod::ArticResult::PROVIDE_INPUT: {
            size_t bufferID = static_cast<size_t>(dataPacket.resp.provideInputBufferID);
            if (bufferID >= pending_response->request.pending_big_buffers.size() ||
                pending_response->request.pending_big_buffers[bufferID].second !=
                    static_cast<size_t>(dataPacket.resp.bufferSize)) {
                LOG_ERROR(Network, "Method {} incorrect big buffer state {}",
                          pending_response->request.method_name, bufferID);
                dataPacket.resp.articResult =
                    ArticBaseCommon::ResponseMethod::ArticResult::METHOD_ERROR;
                if (client.Write(handler_socket, &dataPacket, sizeof(dataPacket))) {
                    continue;
                } else {
                    signal_error();
                }
            } else {
                auto& buffer = pending_response->request.pending_big_buffers[bufferID];
                if (client.Write(handler_socket, &dataPacket, sizeof(dataPacket))) {
                    if (client.Write(handler_socket, buffer.first, buffer.second)) {
                        continue;
                    } else {
                        signal_error();
                    }
                } else {
                    signal_error();
                }
            }
        } break;
        case ArticBaseCommon::ResponseMethod::ArticResult::METHOD_ERROR:
        default: {
            LOG_ERROR(Network, "Method {} error {}", pending_response->request.method_name,
                      dataPacket.resp.methodResult);
            pending_response->response.articResult = dataPacket.resp.articResult;
            pending_response->response.methodState =
                static_cast<ArticBaseCommon::MethodState>(dataPacket.resp.methodResult);
        } break;
        }

        {
            std::scoped_lock l(client.recv_map_mutex);
            client.pending_responses.erase(dataPacket.requestID);
        }

        {
            std::scoped_lock<std::mutex> lk(pending_response->cv_mutex);
            pending_response->is_done = true;
            pending_response->cv.notify_one();
        }
    }
    should_run = false;
    shutdown(handler_socket, SHUT_RDWR);
    closesocket(handler_socket);
}

void Client::OnAllHandlersFinished() {
    // If no handlers are running, signal all pending requests so that
    // they don't become stuck.
    std::scoped_lock l(recv_map_mutex);
    for (auto& [id, response] : pending_responses) {
        std::scoped_lock l2(response->cv_mutex);
        response->is_done = true;
        response->cv.notify_one();
    }
    pending_responses.clear();
}

} // namespace Network::ArticBase