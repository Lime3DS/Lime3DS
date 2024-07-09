// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include "condition_variable"
#include "cstring"
#include "functional"
#include "map"
#include "memory"
#include "mutex"
#include "optional"
#include "string"
#include "thread"
#include "utility"

#include "artic_base_common.h"
#include "network/socket_manager.h"

#ifdef _WIN32
using SocketHolder = unsigned long long;
#else
using SocketHolder = int;
#endif // _WIN32

namespace Network::ArticBase {

class Client {
public:
    class Request {
    public:
        bool AddParameterS8(s8 parameter);
        bool AddParameterU8(u8 parameter) {
            return AddParameterS8(static_cast<s8>(parameter));
        }
        bool AddParameterS16(s16 parameter);
        bool AddParameterU16(u16 parameter) {
            return AddParameterS16(static_cast<s16>(parameter));
        }
        bool AddParameterS32(s32 parameter);
        bool AddParameterU32(u32 parameter) {
            return AddParameterS32(static_cast<s32>(parameter));
        }
        bool AddParameterS64(s64 parameter);
        bool AddParameterU64(u64 parameter) {
            return AddParameterS64(static_cast<s64>(parameter));
        }

        // NOTE: Buffer pointer must remain alive until the response is received
        bool AddParameterBuffer(const void* buffer, size_t bufferSize);

    private:
        friend class Client;
        Request(u32 request_id, const std::string& method, size_t max_params);

        ArticBaseCommon::RequestPacket request_packet{};
        std::vector<ArticBaseCommon::RequestParameter> parameters;
        std::string method_name;
        size_t max_param_count;
        std::vector<std::pair<const void*, size_t>> pending_big_buffers;
    };

    Client(const std::string& _address, u16 _port) : address(_address), port(_port) {
        SocketManager::EnableSockets();
    }
    ~Client();

    bool Connect();
    bool connected = false;

    size_t GetServerRequestMaxSize() {
        return max_server_work_ram;
    }

    Request NewRequest(const std::string& method) {
        return Request(GetNextRequestID(), method, max_parameter_count);
    }

    void Stop() {
        StopImpl(false);
    }

    void SetCommunicationErrorCallback(const std::function<void(const std::string&)>& callback) {
        communication_error_callback = callback;
    }

    void SetArticReportTrafficCallback(const std::function<void(s32)>& callback) {
        report_traffic_callback = callback;
    }

    void ReportArticEvent(u64 event) {
        if (report_artic_event_callback) {
            report_artic_event_callback(event);
        }
    }
    void SetReportArticEventCallback(const std::function<void(u64)>& callback) {
        report_artic_event_callback = callback;
    }

private:
    static constexpr const int SERVER_VERSION = 1;

    std::string address;
    u16 port;

    SocketHolder main_socket = -1;
    std::atomic<u32> currRequestID;
    u32 GetNextRequestID() {
        return currRequestID++;
    }

    void SignalCommunicationError(const std::string& msg = "");
    std::function<void(const std::string&)> communication_error_callback;

    std::function<void(u64)> report_artic_event_callback;

    size_t max_server_work_ram = 0;
    size_t max_parameter_count = 0;
    std::mutex send_mutex;

    std::atomic<bool> stopped = false;

    std::atomic<std::chrono::time_point<std::chrono::steady_clock>> last_sent_request;
    std::thread ping_thread;
    std::condition_variable ping_cv;
    std::mutex ping_cv_mutex;
    bool ping_run = true;

    void StopImpl(bool from_error);

    void PingFunction();

    static bool ConnectWithTimeout(SocketHolder sockFD, void* server_addr, size_t server_addr_len,
                                   int timeout_seconds);
    static bool SetNonBlock(SocketHolder sockFD, bool blocking);
    bool Read(SocketHolder sockFD, void* buffer, size_t size,
              const std::chrono::nanoseconds& timeout = std::chrono::nanoseconds(0));
    bool Write(SocketHolder sockFD, const void* buffer, size_t size,
               const std::chrono::nanoseconds& timeout = std::chrono::nanoseconds(0));
    std::function<void(u32)> report_traffic_callback;

    std::optional<ArticBaseCommon::DataPacket> SendRequestPacket(
        const ArticBaseCommon::RequestPacket& req, bool expect_response,
        const std::vector<ArticBaseCommon::RequestParameter>& params,
        const std::chrono::nanoseconds& read_timeout = std::chrono::nanoseconds(0));
    std::optional<std::string> SendSimpleRequest(const std::string& method);

    class Handler {
    public:
        Handler(Client& _client, u32 _addr, u16 _port, int _id);
        ~Handler() {
            delete thread;
        }
        void RunLoop();

        int id = 0;
        bool should_run = true;
        SocketHolder handler_socket = -1;
        std::thread* thread = nullptr;

    private:
        Client& client;
        u32 addr;
        u16 port;
    };

    class PendingResponse;

public:
    class Response {
    public:
        Response() {}
        Response(Response& other)
            : articResult(other.articResult), methodResult(other.methodResult),
              resp_data_size(other.resp_data_size) {
            if (resp_data_size) {
                resp_data_buffer = reinterpret_cast<char*>(operator new(resp_data_size));
                std::memcpy(resp_data_buffer, other.resp_data_buffer, resp_data_size);
            }
        }
        Response(Response&& other) noexcept
            : articResult(other.articResult), methodResult(other.methodResult),
              resp_data_buffer(std::exchange(other.resp_data_buffer, nullptr)),
              resp_data_size(other.resp_data_size) {}

        Response& operator=(Response& other) {
            articResult = other.articResult;
            methodResult = other.methodResult;
            resp_data_size = other.resp_data_size;
            if (resp_data_size) {
                resp_data_buffer = reinterpret_cast<char*>(operator new(resp_data_size));
                std::memcpy(resp_data_buffer, other.resp_data_buffer, resp_data_size);
            }
            return *this;
        }

        Response& operator=(Response&& other) noexcept {
            articResult = other.articResult;
            methodResult = other.methodResult;
            resp_data_size = other.resp_data_size;
            resp_data_buffer = std::exchange(other.resp_data_buffer, nullptr);
            return *this;
        }

        ~Response() {
            if (resp_data_buffer) {
                operator delete(resp_data_buffer);
            }
        }

        bool Succeeded() const {
            return articResult == ArticBaseCommon::ResponseMethod::ArticResult::SUCCESS;
        }

        int GetMethodResult() const {
            return methodResult;
        }

        std::optional<std::pair<void*, size_t>> GetResponseBuffer(u32 buffer_id) const;

        std::optional<s32> GetResponseS32(u32 buffer_id) const {
            auto buf = GetResponseBuffer(buffer_id);
            if (!buf.has_value() || buf->second != sizeof(s32)) {
                return std::nullopt;
            }
            return *reinterpret_cast<s32*>(buf->first);
        }

        std::optional<s64> GetResponseS64(u32 buffer_id) const {
            auto buf = GetResponseBuffer(buffer_id);
            if (!buf.has_value() || buf->second != sizeof(s64)) {
                return std::nullopt;
            }
            return *reinterpret_cast<s64*>(buf->first);
        }

        std::optional<u64> GetResponseU64(u32 buffer_id) const {
            auto buf = GetResponseBuffer(buffer_id);
            if (!buf.has_value() || buf->second != sizeof(u64)) {
                return std::nullopt;
            }
            return *reinterpret_cast<u64*>(buf->first);
        }

    private:
        friend class Client;
        friend class Client::Handler;
        friend class PendingResponse;

        // Start in error state in case the request is not fullfilled properly.
        ArticBaseCommon::ResponseMethod::ArticResult articResult =
            ArticBaseCommon::ResponseMethod::ArticResult::METHOD_ERROR;
        union {
            ArticBaseCommon::MethodState methodState =
                ArticBaseCommon::MethodState::INTERNAL_METHOD_ERROR;
            int methodResult;
        };
        char* resp_data_buffer{};
        size_t resp_data_size = 0;
    };

    std::optional<Response> Send(Request& request);

private:
    class PendingResponse {
    public:
        bool is_done = false;

    private:
        friend class Client;
        friend class Client::Handler;
        PendingResponse(const Request& req) : request(req) {}
        std::condition_variable cv;
        std::mutex cv_mutex;

        const Request& request;

        Response response{};
    };

    std::mutex recv_map_mutex;
    std::map<u32, PendingResponse*> pending_responses;

    std::vector<Handler*> handlers;
    std::atomic<size_t> running_handlers;
    void OnAllHandlersFinished();
};
} // namespace Network::ArticBase
