// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <tuple>
#include <unordered_map>
#include <boost/algorithm/string/replace.hpp>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <fmt/format.h>
#include "common/archives.h"
#include "common/assert.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/romfs.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/http/http_c.h"
#include "core/hw/aes/key.h"

SERIALIZE_EXPORT_IMPL(Service::HTTP::HTTP_C)
SERIALIZE_EXPORT_IMPL(Service::HTTP::SessionData)

namespace Service::HTTP {

namespace ErrCodes {
enum {
    InvalidRequestState = 22,
    TooManyContexts = 26,
    InvalidRequestMethod = 32,
    HeaderNotFound = 40,
    BufferTooSmall = 43,

    /// This error is returned in multiple situations: when trying to add Post data that is
    /// incompatible with the one that is used in the session, or when trying to use chunked
    /// requests with Post data already set
    IncompatibleAddPostData = 50,

    InvalidPostDataEncoding = 53,
    IncompatibleSendPostData = 54,
    WrongCertID = 57,
    CertAlreadySet = 61,
    ContextNotFound = 100,
    Timeout = 105,

    /// This error is returned in multiple situations: when trying to initialize an
    /// already-initialized session, or when using the wrong context handle in a context-bound
    /// session
    SessionStateError = 102,

    WrongCertHandle = 201,
    TooManyClientCerts = 203,
    NotImplemented = 1012,
};
}

constexpr Result ErrorStateError = // 0xD8A0A066
    Result(ErrCodes::SessionStateError, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorNotImplemented = // 0xD960A3F4
    Result(ErrCodes::NotImplemented, ErrorModule::HTTP, ErrorSummary::Internal,
           ErrorLevel::Permanent);
constexpr Result ErrorTooManyClientCerts = // 0xD8A0A0CB
    Result(ErrCodes::TooManyClientCerts, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorHeaderNotFound = // 0xD8A0A028
    Result(ErrCodes::HeaderNotFound, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorBufferSmall = // 0xD840A02B
    Result(ErrCodes::BufferTooSmall, ErrorModule::HTTP, ErrorSummary::WouldBlock,
           ErrorLevel::Permanent);
constexpr Result ErrorWrongCertID = // 0xD8E0B839
    Result(ErrCodes::WrongCertID, ErrorModule::SSL, ErrorSummary::InvalidArgument,
           ErrorLevel::Permanent);
constexpr Result ErrorWrongCertHandle = // 0xD8A0A0C9
    Result(ErrCodes::WrongCertHandle, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorCertAlreadySet = // 0xD8A0A03D
    Result(ErrCodes::CertAlreadySet, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorIncompatibleAddPostData = // 0xD8A0A032
    Result(ErrCodes::IncompatibleAddPostData, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorContextNotFound = // 0xD8A0A064
    Result(ErrCodes::ContextNotFound, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorTimeout = // 0xD820A069
    Result(ErrCodes::Timeout, ErrorModule::HTTP, ErrorSummary::NothingHappened,
           ErrorLevel::Permanent);
constexpr Result ErrorTooManyContexts = // 0xD8A0A01A
    Result(ErrCodes::TooManyContexts, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorInvalidRequestMethod = // 0xD8A0A020
    Result(ErrCodes::InvalidRequestMethod, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorInvalidRequestState = // 0xD8A0A016
    Result(ErrCodes::InvalidRequestState, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorInvalidPostDataEncoding = // 0xD8A0A035
    Result(ErrCodes::InvalidPostDataEncoding, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);
constexpr Result ErrorIncompatibleSendPostData = // 0xD8A0A036
    Result(ErrCodes::IncompatibleSendPostData, ErrorModule::HTTP, ErrorSummary::InvalidState,
           ErrorLevel::Permanent);

// Splits URL into its components. Example: https://citra-emu.org:443/index.html
// is_https: true; host: citra-emu.org; port: 443; path: /index.html
static URLInfo SplitUrl(const std::string& url) {
    const std::string prefix = "://";
    constexpr int default_http_port = 80;
    constexpr int default_https_port = 443;

    std::string host;
    int port = -1;
    std::string path;

    const auto scheme_end = url.find(prefix);
    const auto prefix_end = scheme_end == std::string::npos ? 0 : scheme_end + prefix.length();
    bool is_https = scheme_end != std::string::npos && url.starts_with("https");
    const auto path_index = url.find("/", prefix_end);

    if (path_index == std::string::npos) {
        // If no path is specified after the host, set it to "/"
        host = url.substr(prefix_end);
        path = "/";
    } else {
        host = url.substr(prefix_end, path_index - prefix_end);
        path = url.substr(path_index);
    }

    const auto port_start = host.find(":");
    if (port_start != std::string::npos) {
        std::string port_str = host.substr(port_start + 1);
        host = host.substr(0, port_start);
        char* p_end = nullptr;
        port = std::strtol(port_str.c_str(), &p_end, 10);
        if (*p_end) {
            port = -1;
        }
    }

    if (port == -1) {
        port = is_https ? default_https_port : default_http_port;
    }
    return URLInfo{
        .is_https = is_https,
        .host = host,
        .port = port,
        .path = path,
    };
}

static std::size_t WriteHeaders(httplib::Stream& stream,
                                std::span<const Context::RequestHeader> headers) {
    std::size_t write_len = 0;
    for (const auto& header : headers) {
        auto len = stream.write_format("%s: %s\r\n", header.name.c_str(), header.value.c_str());
        if (len < 0) {
            return len;
        }
        write_len += len;
    }
    auto len = stream.write("\r\n");
    if (len < 0) {
        return len;
    }
    write_len += len;
    return write_len;
}

static void SerializeChunkedAsciiPostData(httplib::DataSink& sink, const Context::Params& params) {
    std::string query;

    for (auto it = params.begin(); it != params.end(); ++it) {
        if (it != params.begin()) {
            sink.os << "&";
        }

        query =
            fmt::format("{}={}", it->first, httplib::detail::encode_query_param(it->second.value));
        boost::replace_all(query, "*", "%2A");
        sink.os << query;
    }
}

static void SerializeChunkedMultipartPostData(httplib::DataSink& sink,
                                              const Context::Params& params,
                                              const std::string& boundary) {
    for (const auto& param : params) {
        const auto item = param.second.ToMultipartForm();
        std::string body = httplib::detail::serialize_multipart_formdata_item_begin(item, boundary);
        body += item.content + httplib::detail::serialize_multipart_formdata_item_end();
        sink.os << body;
    }

    sink.os << httplib::detail::serialize_multipart_formdata_finish(boundary);
}

std::size_t Context::HandleHeaderWrite(std::vector<Context::RequestHeader>& pending_headers,
                                       httplib::Stream& strm, httplib::Headers& httplib_headers) {
    std::vector<Context::RequestHeader> final_headers;
    std::vector<Context::RequestHeader>::iterator it_pending_headers;
    httplib::Headers::iterator it_httplib_headers;

    auto find_pending_header = [&pending_headers](const std::string& str) {
        return std::find_if(pending_headers.begin(), pending_headers.end(),
                            [&str](Context::RequestHeader& rh) { return rh.name == str; });
    };

    // Watch out for header ordering!!
    // First: Host
    it_pending_headers = find_pending_header("Host");
    if (it_pending_headers != pending_headers.end()) {
        final_headers.push_back(
            Context::RequestHeader(it_pending_headers->name, it_pending_headers->value));
        pending_headers.erase(it_pending_headers);
    } else {
        it_httplib_headers = httplib_headers.find("Host");
        if (it_httplib_headers != httplib_headers.end()) {
            final_headers.push_back(
                Context::RequestHeader(it_httplib_headers->first, it_httplib_headers->second));
        }
    }

    // Second, user defined headers
    // Third, Content-Type (optional, appended by MakeRequest)
    for (const auto& header : pending_headers) {
        final_headers.push_back(header);
    }

    // Fourth: Content-Length
    it_pending_headers = find_pending_header("Content-Length");
    if (it_pending_headers == pending_headers.end()) {
        if ((method == RequestMethod::Post || method == RequestMethod::Put) && !chunked_request) {
            it_httplib_headers = httplib_headers.find("Content-Length");
            if (it_httplib_headers != httplib_headers.end()) {
                final_headers.push_back(
                    Context::RequestHeader(it_httplib_headers->first, it_httplib_headers->second));
            }
        }
    }

    // Fifth: Transfer-Encoding
    if (chunked_request) {
        final_headers.push_back(Context::RequestHeader("Transfer-Encoding", "chunked"));
    }

    return WriteHeaders(strm, final_headers);
};

void Context::ParseAsciiPostData() {
    httplib::Params ascii_form;
    for (auto param : post_data) {
        ascii_form.emplace(param.first, param.second.value);
    }

    post_data_raw = httplib::detail::params_to_query_str(ascii_form);
    boost::replace_all(post_data_raw, "*", "%2A");
}

std::string Context::ParseMultipartFormData() {
    httplib::MultipartFormDataItems multipart_form;
    for (auto param : post_data) {
        multipart_form.push_back(param.second.ToMultipartForm());
    }

    multipart_boundary = httplib::detail::make_multipart_data_boundary();
    post_data_raw =
        httplib::detail::serialize_multipart_formdata(multipart_form, multipart_boundary);
    return httplib::detail::serialize_multipart_formdata_get_content_type(multipart_boundary);
}

void Context::MakeRequest() {
    ASSERT(state == RequestState::NotStarted);

    state = RequestState::ConnectingToServer;

    static const std::unordered_map<RequestMethod, std::string> request_method_strings{
        {RequestMethod::Get, "GET"},       {RequestMethod::Post, "POST"},
        {RequestMethod::Head, "HEAD"},     {RequestMethod::Put, "PUT"},
        {RequestMethod::Delete, "DELETE"}, {RequestMethod::PostEmpty, "POST"},
        {RequestMethod::PutEmpty, "PUT"},
    };

    URLInfo url_info = SplitUrl(url);

    httplib::Request request;
    std::vector<Context::RequestHeader> pending_headers;
    request.method = request_method_strings.at(method);
    request.path = url_info.path;

    request.progress = [this](u64 current, u64 total) -> bool {
        // TODO(B3N30): Is there a state that shows response header are available
        current_download_size_bytes = current;
        total_download_size_bytes = total;
        return true;
    };

    for (const auto& header : headers) {
        pending_headers.push_back(header);
    }

    httplib::Params ascii_form;
    httplib::MultipartFormDataItems multipart_form;
    if ((method == RequestMethod::Post || method == RequestMethod::Put) && !chunked_request) {
        switch (post_data_encoding) {
        case PostDataEncoding::AsciiForm:
            ParseAsciiPostData();
            pending_headers.push_back(
                Context::RequestHeader("Content-Type", "application/x-www-form-urlencoded"));
            break;
        case PostDataEncoding::MultipartForm:
            pending_headers.push_back(
                Context::RequestHeader("Content-Type", ParseMultipartFormData()));
            break;
        case PostDataEncoding::Auto:
            if (!post_data.empty()) {
                if (force_multipart) {
                    pending_headers.push_back(
                        Context::RequestHeader("Content-Type", ParseMultipartFormData()));
                } else {
                    pending_headers.push_back(Context::RequestHeader(
                        "Content-Type", "application/x-www-form-urlencoded"));
                    ParseAsciiPostData();
                }
            }
            break;
        }
    }

    // httplib doesn't expose setting the content provider for the request when not using the usual
    // send methods like Client::Post or Client::Put, so we have to set the internal fields manually
    if (!chunked_request) {
        request.content_length_ = post_data_raw.size();
        request.content_provider_ = [this](size_t offset, size_t length, httplib::DataSink& sink) {
            return ContentProvider(offset, length, sink);
        };
    } else {
        if (post_data_type == PostDataType::MultipartForm) {
            multipart_boundary = httplib::detail::make_multipart_data_boundary();
            pending_headers.push_back(Context::RequestHeader(
                "Content-Type", httplib::detail::serialize_multipart_formdata_get_content_type(
                                    multipart_boundary)));
        }

        if (post_data_type == PostDataType::Raw && chunked_content_length > 0) {
            pending_headers.push_back(Context::RequestHeader(
                "Content-Length", fmt::format("{}", chunked_content_length)));
        }

        request.content_length_ = 0;
        request.content_provider_ =
            httplib::detail::ContentProviderAdapter([this](size_t offset, httplib::DataSink& sink) {
                return ChunkedContentProvider(offset, sink);
            });
        request.is_chunked_content_provider_ = true;
    }

    if (url_info.is_https) {
        MakeRequestSSL(request, url_info, pending_headers);
    } else {
        MakeRequestNonSSL(request, url_info, pending_headers);
    }
}

void Context::MakeRequestNonSSL(httplib::Request& request, const URLInfo& url_info,
                                std::vector<Context::RequestHeader>& pending_headers) {
    httplib::Error error{-1};
    std::unique_ptr<httplib::Client> client =
        std::make_unique<httplib::Client>(url_info.host, url_info.port);

    client->set_header_writer(
        [this, &pending_headers](httplib::Stream& strm, httplib::Headers& httplib_headers) {
            return HandleHeaderWrite(pending_headers, strm, httplib_headers);
        });

    if (!client->send(request, response, error)) {
        LOG_ERROR(Service_HTTP, "Request failed: {}: {}", error, httplib::to_string(error));
        state = RequestState::TimedOut;
    } else {
        LOG_DEBUG(Service_HTTP, "Request successful");
        state = RequestState::ReadyToDownloadContent;
    }
}

void Context::MakeRequestSSL(httplib::Request& request, const URLInfo& url_info,
                             std::vector<Context::RequestHeader>& pending_headers) {
    httplib::Error error{-1};
    X509* cert = nullptr;
    EVP_PKEY* key = nullptr;
    const unsigned char* cert_data = nullptr;
    const unsigned char* key_data = nullptr;
    long cert_size = 0;
    long key_size = 0;
    SCOPE_EXIT({
        if (cert) {
            X509_free(cert);
        }
        if (key) {
            EVP_PKEY_free(key);
        }
    });

    if (uses_default_client_cert) {
        cert_data = clcert_data->certificate.data();
        key_data = clcert_data->private_key.data();
        cert_size = static_cast<long>(clcert_data->certificate.size());
        key_size = static_cast<long>(clcert_data->private_key.size());
    } else if (auto client_cert = ssl_config.client_cert_ctx.lock()) {
        cert_data = client_cert->certificate.data();
        key_data = client_cert->private_key.data();
        cert_size = static_cast<long>(client_cert->certificate.size());
        key_size = static_cast<long>(client_cert->private_key.size());
    }

    std::unique_ptr<httplib::SSLClient> client;
    if (cert_data && key_data) {
        cert = d2i_X509(nullptr, &cert_data, cert_size);
        key = d2i_PrivateKey(EVP_PKEY_RSA, nullptr, &key_data, key_size);
        client = std::make_unique<httplib::SSLClient>(url_info.host, url_info.port, cert, key);
    } else {
        client = std::make_unique<httplib::SSLClient>(url_info.host, url_info.port);
    }

    // TODO(B3N30): Check for SSLOptions-Bits and set the verify method accordingly
    // https://www.3dbrew.org/wiki/SSL_Services#SSLOpt
    // Hack: Since for now RootCerts are not implemented we set the VerifyMode to None.
    client->enable_server_certificate_verification(false);

    client->set_header_writer(
        [this, &pending_headers](httplib::Stream& strm, httplib::Headers& httplib_headers) {
            return HandleHeaderWrite(pending_headers, strm, httplib_headers);
        });

    if (!client->send(request, response, error)) {
        LOG_ERROR(Service_HTTP, "Request failed: {}: {}", error, httplib::to_string(error));
        state = RequestState::TimedOut;
    } else {
        LOG_DEBUG(Service_HTTP, "Request successful");
        state = RequestState::ReadyToDownloadContent;
    }
}

bool Context::ContentProvider(size_t offset, size_t length, httplib::DataSink& sink) {
    state = RequestState::SendingRequest;

    if (!post_data_raw.empty()) {
        sink.write(post_data_raw.data() + offset, length);
    }

    // This state is set after sending the request, even if it hasn't received a response yet
    state = RequestState::ReceivingResponse;
    return true;
}

bool Context::ChunkedContentProvider(size_t offset, httplib::DataSink& sink) {
    state = RequestState::SendingRequest;

    finish_post_data.Wait();

    switch (post_data_type) {
    case PostDataType::AsciiForm:
        SerializeChunkedAsciiPostData(sink, post_data);
        break;
    case PostDataType::MultipartForm:
        SerializeChunkedMultipartPostData(sink, post_data, multipart_boundary);
        break;
    // Write the data values
    case PostDataType::Raw:
        for (const auto& data : post_data) {
            sink.os << data.second.value;
        }
        break;
    }

    sink.done();
    // This state is set after sending the request, even if it hasn't received a response yet
    state = RequestState::ReceivingResponse;
    return true;
}

void HTTP_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 shmem_size = rp.Pop<u32>();
    u32 pid = rp.PopPID();
    shared_memory = rp.PopObject<Kernel::SharedMemory>();
    if (shared_memory) {
        shared_memory->SetName("HTTP_C:shared_memory");
    }

    LOG_DEBUG(Service_HTTP, "called, shared memory size: {} pid: {}", shmem_size, pid);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    session_data->initialized = true;
    session_data->session_id = ++session_counter;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    // This returns 0xd8a0a046 if no network connection is available.
    // Just assume we are always connected.
    rb.Push(ResultSuccess);
}

void HTTP_C::InitializeConnectionSession(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    u32 pid = rp.PopPID();

    LOG_DEBUG(Service_HTTP, "called, context_id={} pid={}", context_handle, pid);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    // TODO(Subv): Check that the input PID matches the PID that created the context.
    auto itr = contexts.find(context_handle);
    if (itr == contexts.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorContextNotFound);
        return;
    }

    session_data->initialized = true;
    session_data->session_id = ++session_counter;
    // Bind the context to the current session.
    session_data->current_http_context = context_handle;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::BeginRequest(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_id={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    // This should never happen in real hardware, but can happen on citra.
    if (http_context.uses_default_client_cert && !http_context.clcert_data->init) {
        LOG_ERROR(Service_HTTP, "Failed to begin HTTP request: client cert not found.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    // On a 3DS BeginRequest and BeginRequestAsync will push the Request to a worker queue.
    // You can only enqueue 8 requests at the same time.
    // trying to enqueue any more will either fail (BeginRequestAsync), or block (BeginRequest)
    // Note that you only can have 8 Contexts at a time. So this difference shouldn't matter
    // Then there are 3? worker threads that pop the requests from the queue and send them
    // For now make every request async in it's own thread.

    // This always returns success, but the request is only performed when it hasn't started
    if (http_context.state == RequestState::NotStarted) {
        http_context.request_future =
            std::async(std::launch::async, &Context::MakeRequest, std::ref(http_context));
        http_context.current_copied_data = 0;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::BeginRequestAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_id={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    // This should never happen in real hardware, but can happen on citra.
    if (http_context.uses_default_client_cert && !http_context.clcert_data->init) {
        LOG_ERROR(Service_HTTP, "Failed to begin HTTP request: client cert not found.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return;
    }

    // On a 3DS BeginRequest and BeginRequestAsync will push the Request to a worker queue.
    // You can only enqueue 8 requests at the same time.
    // trying to enqueue any more will either fail (BeginRequestAsync), or block (BeginRequest)
    // Note that you only can have 8 Contexts at a time. So this difference shouldn't matter
    // Then there are 3? worker threads that pop the requests from the queue and send them
    // For now make every request async in it's own thread.

    // This always returns success, but the request is only performed when it hasn't started
    if (http_context.state == RequestState::NotStarted) {
        http_context.request_future =
            std::async(std::launch::async, &Context::MakeRequest, std::ref(http_context));
        http_context.current_copied_data = 0;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::ReceiveData(Kernel::HLERequestContext& ctx) {
    ReceiveDataImpl(ctx, false);
}

void HTTP_C::ReceiveDataTimeout(Kernel::HLERequestContext& ctx) {
    ReceiveDataImpl(ctx, true);
}

void HTTP_C::ReceiveDataImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);

    struct AsyncData {
        // Input
        u64 timeout_nanos = 0;
        bool timeout;
        Context::Handle context_handle;
        u32 buffer_size;
        Kernel::MappedBuffer* buffer;
        bool is_complete;
        // Output
        Result async_res = ResultSuccess;
    };
    std::shared_ptr<AsyncData> async_data = std::make_shared<AsyncData>();
    async_data->timeout = timeout;
    async_data->context_handle = rp.Pop<u32>();
    async_data->buffer_size = rp.Pop<u32>();

    if (timeout) {
        async_data->timeout_nanos = rp.Pop<u64>();
        LOG_DEBUG(Service_HTTP, "called, timeout={}", async_data->timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called");
    }
    async_data->buffer = &rp.PopMappedBuffer();

    if (!PerformStateChecks(ctx, rp, async_data->context_handle)) {
        return;
    }

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            Context& http_context = GetContext(async_data->context_handle);

            if (async_data->timeout) {
                const auto wait_res = http_context.request_future.wait_for(
                    std::chrono::nanoseconds(async_data->timeout_nanos));
                if (wait_res == std::future_status::timeout) {
                    async_data->async_res = ErrorTimeout;
                }
            } else {
                http_context.request_future.wait();
            }
            // Simulate small delay from HTTP receive.
            return 1'000'000;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 1,
                                   0);
            if (async_data->async_res != ResultSuccess) {
                rb.Push(async_data->async_res);
                return;
            }
            Context& http_context = GetContext(async_data->context_handle);

            const std::size_t remaining_data =
                http_context.response.body.size() - http_context.current_copied_data;

            if (async_data->buffer_size >= remaining_data) {
                async_data->buffer->Write(http_context.response.body.data() +
                                              http_context.current_copied_data,
                                          0, remaining_data);
                http_context.current_copied_data += remaining_data;
                rb.Push(ResultSuccess);
            } else {
                async_data->buffer->Write(http_context.response.body.data() +
                                              http_context.current_copied_data,
                                          0, async_data->buffer_size);
                http_context.current_copied_data += async_data->buffer_size;
                rb.Push(ErrorBufferSmall);
            }
            LOG_DEBUG(Service_HTTP, "Receive: buffer_size= {}, total_copied={}, total_body={}",
                      async_data->buffer_size, http_context.current_copied_data,
                      http_context.response.body.size());
        });
}

void HTTP_C::SetProxyDefault(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::CreateContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 url_size = rp.Pop<u32>();
    RequestMethod method = rp.PopEnum<RequestMethod>();
    Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();

    // Copy the buffer into a string without the \0 at the end of the buffer
    std::string url(url_size, '\0');
    buffer.Read(&url[0], 0, url_size - 1);

    LOG_DEBUG(Service_HTTP, "called, url_size={}, url={}, method={}", url_size, url, method);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    // This command can only be called without a bound session.
    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorNotImplemented);
        rb.PushMappedBuffer(buffer);
        return;
    }

    static constexpr std::size_t MaxConcurrentHTTPContexts = 8;
    if (session_data->num_http_contexts >= MaxConcurrentHTTPContexts) {
        // There can only be 8 HTTP contexts open at the same time for any particular session.
        LOG_ERROR(Service_HTTP, "Tried to open too many HTTP contexts");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorTooManyContexts);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (method == RequestMethod::None || static_cast<u32>(method) >= TotalRequestMethods) {
        LOG_ERROR(Service_HTTP, "invalid request method={}", method);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestMethod);
        rb.PushMappedBuffer(buffer);
        return;
    }

    contexts.try_emplace(++context_counter);
    contexts[context_counter].url = std::move(url);
    contexts[context_counter].method = method;
    contexts[context_counter].state = RequestState::NotStarted;
    // TODO(Subv): Find a correct default value for this field.
    contexts[context_counter].socket_buffer_size = 0;
    contexts[context_counter].handle = context_counter;
    contexts[context_counter].session_id = session_data->session_id;

    session_data->num_http_contexts++;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(ResultSuccess);
    rb.Push<u32>(context_counter);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::CloseContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, handle={}", context_handle);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    ASSERT_MSG(!session_data->current_http_context,
               "Unimplemented CloseContext on context-bound session");

    auto itr = contexts.find(context_handle);
    if (itr == contexts.end()) {
        // The real HTTP module just silently fails in this case.
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultSuccess);
        LOG_ERROR(Service_HTTP, "called, context {} not found", context_handle);
        return;
    }

    // TODO(Subv): What happens if you try to close a context that's currently being used?
    // TODO(Subv): Make sure that only the session that created the context can close it.

    // Note that this will block if a request is still in progress
    contexts.erase(itr);
    session_data->num_http_contexts--;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::CancelConnection(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    [[maybe_unused]] Context& http_context = GetContext(context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::GetRequestState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();

    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    LOG_DEBUG(Service_HTTP, "called, context_handle={}", context_handle);

    Context& http_context = GetContext(context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.PushEnum<RequestState>(http_context.state);
}

void HTTP_C::AddRequestHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(&value[0], 0, value_size - 1);

    LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add a request header on a context that has already been started.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    http_context.headers.emplace_back(name, value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::AddPostDataAscii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(value.data(), 0, value_size - 1);

    LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Cannot add ASCII Post data to context with raw Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot add ASCII Post data to context in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::AddPostDataBinary(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a vector
    std::vector<u8> value(value_size);
    value_buffer.Read(value.data(), 0, value_size);

    LOG_DEBUG(Service_HTTP, "called, name={}, value_size={}, context_handle={}", name, value_size,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Cannot add Binary Post data to context with raw Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot add Binary Post data to context in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);
    http_context.force_multipart = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::AddPostDataRaw(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 post_data_len = rp.Pop<u32>();
    auto buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, post_data_len={}", context_handle,
              post_data_len);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (!http_context.post_data.empty()) {
        LOG_ERROR(Service_HTTP,
                  "Cannot add raw Post data to context with ASCII or Binary Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot add raw Post data to context in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleAddPostData);
        rb.PushMappedBuffer(buffer);
        return;
    }

    http_context.post_data_raw.resize(buffer.GetSize());
    buffer.Read(http_context.post_data_raw.data(), 0, buffer.GetSize());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::SetPostDataType(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const PostDataType type = rp.PopEnum<PostDataType>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, type={}", context_handle, type);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set chunked mode on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    if (!http_context.post_data.empty() || !http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Tried to set chunked mode on a context that has Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorIncompatibleSendPostData);
        return;
    }

    switch (type) {
    case PostDataType::AsciiForm:
    case PostDataType::MultipartForm:
    case PostDataType::Raw:
        http_context.post_data_type = type;
        break;
    // Use ASCII form by default
    default:
        http_context.post_data_type = PostDataType::AsciiForm;
        break;
    }

    http_context.chunked_request = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SendPostDataAscii(Kernel::HLERequestContext& ctx) {
    SendPostDataAsciiImpl(ctx, false);
}

void HTTP_C::SendPostDataAsciiTimeout(Kernel::HLERequestContext& ctx) {
    SendPostDataAsciiImpl(ctx, true);
}

void HTTP_C::SendPostDataAsciiImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    // TODO(DaniElectra): The original module waits until a connection with the server is made
    const u64 timeout_nanos = timeout ? rp.Pop<u64>() : 0;
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(value.data(), 0, value_size - 1);

    if (timeout) {
        LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}, timeout={}", name,
                  value, context_handle, timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
                  context_handle);
    }

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state == RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP, "Tried to send Post data on a context that has not been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.chunked_request) {
        LOG_ERROR(Service_HTTP,
                  "Cannot send ASCII Post data to context not in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::SendPostDataBinary(Kernel::HLERequestContext& ctx) {
    SendPostDataBinaryImpl(ctx, false);
}

void HTTP_C::SendPostDataBinaryTimeout(Kernel::HLERequestContext& ctx) {
    SendPostDataBinaryImpl(ctx, true);
}

void HTTP_C::SendPostDataBinaryImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    // TODO(DaniElectra): The original module waits until a connection with the server is made
    const u64 timeout_nanos = timeout ? rp.Pop<u64>() : 0;
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a vector
    std::vector<u8> value(value_size);
    value_buffer.Read(value.data(), 0, value_size);

    if (timeout) {
        LOG_DEBUG(Service_HTTP, "called, name={}, value_size={}, context_handle={}, timeout={}",
                  name, value_size, context_handle, timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called, name={}, value_size={}, context_handle={}", name,
                  value_size, context_handle);
    }

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state == RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP, "Tried to add Post data on a context that has not been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (!http_context.chunked_request) {
        LOG_ERROR(Service_HTTP,
                  "Cannot send Binary Post data to context not in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    Context::Param param_value(name, value);
    http_context.post_data.emplace(name, param_value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::SendPostDataRaw(Kernel::HLERequestContext& ctx) {
    SendPostDataRawImpl(ctx, false);
}

void HTTP_C::SendPostDataRawTimeout(Kernel::HLERequestContext& ctx) {
    SendPostDataRawImpl(ctx, true);
}

void HTTP_C::SendPostDataRawImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 post_data_len = rp.Pop<u32>();
    // TODO(DaniElectra): The original module waits until a connection with the server is made
    const u64 timeout_nanos = timeout ? rp.Pop<u64>() : 0;
    auto buffer = rp.PopMappedBuffer();

    if (timeout) {
        LOG_DEBUG(Service_HTTP, "called, context_handle={}, post_data_len={}, timeout={}",
                  context_handle, post_data_len, timeout_nanos);
    } else {
        LOG_DEBUG(Service_HTTP, "called, context_handle={}, post_data_len={}", context_handle,
                  post_data_len);
    }

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add Post data on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (!http_context.chunked_request) {
        LOG_ERROR(Service_HTTP, "Cannot send raw Post data to context not in chunked request mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorInvalidRequestState);
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (http_context.post_data_type != PostDataType::Raw) {
        LOG_ERROR(Service_HTTP, "Cannot send raw Post data to context not in raw mode");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ErrorIncompatibleSendPostData);
        rb.PushMappedBuffer(buffer);
        return;
    }

    std::vector<u8> value(buffer.GetSize());
    buffer.Read(value.data(), 0, value.size());

    // Workaround for sending the raw data in combination of other data in chunked requests
    Context::Param raw_param(value);
    std::string value_string(value.begin(), value.end());
    http_context.post_data.emplace(value_string, raw_param);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::SetPostDataEncoding(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const PostDataEncoding encoding = rp.PopEnum<PostDataEncoding>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, encoding={}", context_handle, encoding);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set Post encoding on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    switch (encoding) {
    case PostDataEncoding::Auto:
        http_context.post_data_encoding = encoding;
        break;
    case PostDataEncoding::AsciiForm:
    case PostDataEncoding::MultipartForm:
        if (!http_context.post_data_raw.empty()) {
            LOG_ERROR(Service_HTTP, "Cannot set Post data encoding to context with raw Post data");
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(ErrorIncompatibleAddPostData);
            return;
        }

        http_context.post_data_encoding = encoding;
        break;
    default:
        LOG_ERROR(Service_HTTP, "Invalid Post data encoding: {}", encoding);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidPostDataEncoding);
        return;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::NotifyFinishSendPostData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state == RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to notfy finish Post on a context that has not been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    http_context.finish_post_data.Set();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::GetResponseHeader(Kernel::HLERequestContext& ctx) {
    GetResponseHeaderImpl(ctx, false);
}

void HTTP_C::GetResponseHeaderTimeout(Kernel::HLERequestContext& ctx) {
    GetResponseHeaderImpl(ctx, true);
}

void HTTP_C::GetResponseHeaderImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);

    struct AsyncData {
        // Input
        u32 context_handle;
        u32 name_len;
        u32 value_max_len;
        bool timeout;
        u64 timeout_nanos;
        std::span<const u8> header_name;
        Kernel::MappedBuffer* value_buffer;
        // Output
        Result async_res = ResultSuccess;
    };
    std::shared_ptr<AsyncData> async_data = std::make_shared<AsyncData>();

    async_data->timeout = timeout;
    async_data->context_handle = rp.Pop<u32>();
    async_data->name_len = rp.Pop<u32>();
    async_data->value_max_len = rp.Pop<u32>();
    if (timeout) {
        async_data->timeout_nanos = rp.Pop<u64>();
    }
    async_data->header_name = rp.PopStaticBuffer();
    async_data->value_buffer = &rp.PopMappedBuffer();

    if (!PerformStateChecks(ctx, rp, async_data->context_handle)) {
        return;
    }

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            Context& http_context = GetContext(async_data->context_handle);

            if (async_data->timeout) {
                const auto wait_res = http_context.request_future.wait_for(
                    std::chrono::nanoseconds(async_data->timeout_nanos));
                if (wait_res == std::future_status::timeout) {
                    async_data->async_res = ErrorTimeout;
                }
            } else {
                http_context.request_future.wait();
            }

            return 0;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            IPC::RequestBuilder rb(ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 2,
                                   2);
            if (async_data->async_res != ResultSuccess) {
                rb.Push(async_data->async_res);
                return;
            }

            std::string header_name_str(
                reinterpret_cast<const char*>(async_data->header_name.data()),
                async_data->name_len);
            Common::TruncateString(header_name_str);

            Context& http_context = GetContext(async_data->context_handle);

            auto& headers = http_context.response.headers;
            u32 copied_size = 0;

            if (async_data->timeout) {
                LOG_DEBUG(Service_HTTP, "header={}, max_len={}, timeout={}", header_name_str,
                          async_data->value_buffer->GetSize(), async_data->timeout_nanos);
            } else {
                LOG_DEBUG(Service_HTTP, "header={}, max_len={}", header_name_str,
                          async_data->value_buffer->GetSize());
            }

            auto header = headers.find(header_name_str);
            if (header != headers.end()) {
                std::string header_value = header->second;
                copied_size = static_cast<u32>(header_value.size());
                if (header_value.size() >= async_data->value_buffer->GetSize()) {
                    header_value.resize(async_data->value_buffer->GetSize() - 1);
                }
                header_value.push_back('\0');
                async_data->value_buffer->Write(header_value.data(), 0, header_value.size());
            } else {
                LOG_DEBUG(Service_HTTP, "header={} not found", header_name_str);
                rb.Push(ErrorHeaderNotFound);
                rb.Push(0);
                rb.PushMappedBuffer(*async_data->value_buffer);
                return;
            }

            rb.Push(ResultSuccess);
            rb.Push(copied_size);
            rb.PushMappedBuffer(*async_data->value_buffer);
        });
}

void HTTP_C::GetResponseStatusCode(Kernel::HLERequestContext& ctx) {
    GetResponseStatusCodeImpl(ctx, false);
}

void HTTP_C::GetResponseStatusCodeTimeout(Kernel::HLERequestContext& ctx) {
    GetResponseStatusCodeImpl(ctx, true);
}

void HTTP_C::GetResponseStatusCodeImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);

    struct AsyncData {
        // Input
        Context::Handle context_handle;
        bool timeout;
        u64 timeout_nanos = 0;
        // Output
        Result async_res = ResultSuccess;
    };
    std::shared_ptr<AsyncData> async_data = std::make_shared<AsyncData>();

    async_data->context_handle = rp.Pop<u32>();
    async_data->timeout = timeout;

    if (timeout) {
        async_data->timeout_nanos = rp.Pop<u64>();
        LOG_INFO(Service_HTTP, "called, timeout={}", async_data->timeout_nanos);
    } else {
        LOG_INFO(Service_HTTP, "called");
    }

    if (!PerformStateChecks(ctx, rp, async_data->context_handle)) {
        return;
    }

    ctx.RunAsync(
        [this, async_data](Kernel::HLERequestContext& ctx) {
            Context& http_context = GetContext(async_data->context_handle);

            if (async_data->timeout) {
                const auto wait_res = http_context.request_future.wait_for(
                    std::chrono::nanoseconds(async_data->timeout_nanos));
                if (wait_res == std::future_status::timeout) {
                    LOG_DEBUG(Service_HTTP, "Status code: {}", "timeout");
                    async_data->async_res = ErrorTimeout;
                }
            } else {
                http_context.request_future.wait();
            }
            return 0;
        },
        [this, async_data](Kernel::HLERequestContext& ctx) {
            if (async_data->async_res != ResultSuccess) {
                IPC::RequestBuilder rb(
                    ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 1, 0);
                rb.Push(async_data->async_res);
                return;
            }

            Context& http_context = GetContext(async_data->context_handle);

            const u32 response_code = http_context.response.status;
            LOG_DEBUG(Service_HTTP, "Status code: {}, response_code={}", "good", response_code);

            IPC::RequestBuilder rb(ctx, static_cast<u16>(ctx.CommandHeader().command_id.Value()), 2,
                                   0);
            rb.Push(ResultSuccess);
            rb.Push(response_code);
        });
}

void HTTP_C::AddTrustedRootCA(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 root_ca_len = rp.Pop<u32>();
    auto root_ca_data = rp.PopMappedBuffer();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(ResultSuccess);
    rb.PushMappedBuffer(root_ca_data);
}

void HTTP_C::AddDefaultCert(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    const u32 certificate_id = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}, certificate_id={}", context_handle,
                certificate_id);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetDefaultClientCert(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    const ClientCertID client_cert_id = static_cast<ClientCertID>(rp.Pop<u32>());

    LOG_DEBUG(Service_HTTP, "client_cert_id={}", client_cert_id);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (client_cert_id != ClientCertID::Default) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorWrongCertID);
        return;
    }

    http_context.uses_default_client_cert = true;
    http_context.clcert_data = &GetClCertA();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 client_cert_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called with context_handle={} client_cert_handle={}", context_handle,
              client_cert_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    auto cert_context_itr = client_certs.find(client_cert_handle);
    if (cert_context_itr == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "called with wrong client_cert_handle {}", client_cert_handle);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorWrongCertHandle);
        return;
    }

    if (http_context.ssl_config.client_cert_ctx.lock()) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set a client cert to a context that already has a client cert");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorCertAlreadySet);
        return;
    }

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set a client cert on a context that has already been started.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    http_context.ssl_config.client_cert_ctx = cert_context_itr->second;
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::GetSSLError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 unk = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, context_handle={}, unk={}", context_handle, unk);

    [[maybe_unused]] Context& http_context = GetContext(context_handle);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    // Since we create the actual http/ssl context only when the request is submitted we can't check
    // for SSL Errors here. Just submit no error.
    rb.Push<u32>(0);
}

void HTTP_C::SetSSLOpt(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 opts = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, context_handle={}, opts={}", context_handle, opts);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::OpenClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 cert_size = rp.Pop<u32>();
    u32 key_size = rp.Pop<u32>();
    Kernel::MappedBuffer& cert_buffer = rp.PopMappedBuffer();
    Kernel::MappedBuffer& key_buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_HTTP, "called, cert_size {}, key_size {}", cert_size, key_size);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    Result result(ResultSuccess);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Command called without Initialize");
        result = ErrorStateError;
    } else if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        result = ErrorNotImplemented;
    } else if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        result = ErrorTooManyClientCerts;
    } else {
        client_certs[++client_certs_counter] = std::make_shared<ClientCertContext>();
        client_certs[client_certs_counter]->handle = client_certs_counter;
        client_certs[client_certs_counter]->certificate.resize(cert_size);
        cert_buffer.Read(&client_certs[client_certs_counter]->certificate[0], 0, cert_size);
        client_certs[client_certs_counter]->private_key.resize(key_size);
        cert_buffer.Read(&client_certs[client_certs_counter]->private_key[0], 0, key_size);
        client_certs[client_certs_counter]->session_id = session_data->session_id;

        ++session_data->num_client_certs;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(cert_buffer);
    rb.PushMappedBuffer(key_buffer);
}

void HTTP_C::OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 cert_id = rp.Pop<u8>();

    LOG_DEBUG(Service_HTTP, "called, cert_id={} cert_handle={}", cert_id, client_certs_counter);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorNotImplemented);
        return;
    }

    if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorTooManyClientCerts);
        return;
    }

    constexpr u8 default_cert_id = static_cast<u8>(ClientCertID::Default);
    if (cert_id != default_cert_id) {
        LOG_ERROR(Service_HTTP, "called with invalid cert_id {}", cert_id);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorWrongCertID);
        return;
    }

    if (!ClCertA.init) {
        LOG_ERROR(Service_HTTP, "called but ClCertA is missing");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(static_cast<Result>(-1));
        return;
    }

    const auto& it = std::find_if(client_certs.begin(), client_certs.end(),
                                  [default_cert_id, &session_data](const auto& i) {
                                      return default_cert_id == i.second->cert_id &&
                                             session_data->session_id == i.second->session_id;
                                  });

    if (it != client_certs.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(ResultSuccess);
        rb.Push<u32>(it->first);

        LOG_DEBUG(Service_HTTP, "called, with an already loaded cert_id={}", cert_id);
        return;
    }

    client_certs[++client_certs_counter] = std::make_shared<ClientCertContext>();
    client_certs[client_certs_counter]->handle = client_certs_counter;
    client_certs[client_certs_counter]->certificate = ClCertA.certificate;
    client_certs[client_certs_counter]->private_key = ClCertA.private_key;
    client_certs[client_certs_counter]->session_id = session_data->session_id;
    ++session_data->num_client_certs;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(ResultSuccess);
    rb.Push<u32>(client_certs_counter);
}

void HTTP_C::CloseClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    ClientCertContext::Handle cert_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, cert_handle={}", cert_handle);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (client_certs.find(cert_handle) == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "Command called with a unkown client cert handle {}", cert_handle);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        // This just return success without doing anything
        rb.Push(ResultSuccess);
        return;
    }

    if (client_certs[cert_handle]->session_id != session_data->session_id) {
        LOG_ERROR(Service_HTTP, "called from another main session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        // This just return success without doing anything
        rb.Push(ResultSuccess);
        return;
    }

    client_certs.erase(cert_handle);
    session_data->num_client_certs--;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetKeepAlive(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 option = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}, option={}", context_handle, option);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::SetPostDataTypeSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const PostDataType type = rp.PopEnum<PostDataType>();
    const u32 size = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, context_handle={}, type={}, size={}", context_handle, type,
              size);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    if (http_context.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set chunked mode on a context that has already been started");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorInvalidRequestState);
        return;
    }

    if (!http_context.post_data.empty() || !http_context.post_data_raw.empty()) {
        LOG_ERROR(Service_HTTP, "Tried to set chunked mode on a context that has Post data");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorIncompatibleSendPostData);
        return;
    }

    switch (type) {
    case PostDataType::AsciiForm:
    case PostDataType::MultipartForm:
    case PostDataType::Raw:
        http_context.post_data_type = type;
        break;
    default:
        http_context.post_data_type = PostDataType::AsciiForm;
        break;
    }

    http_context.chunked_request = true;
    http_context.chunked_content_length = size;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);
}

void HTTP_C::Finalize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    shared_memory = nullptr;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(ResultSuccess);

    LOG_WARNING(Service_HTTP, "(STUBBED) called");
}

void HTTP_C::GetDownloadSizeState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_INFO(Service_HTTP, "called");

    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    Context& http_context = GetContext(context_handle);

    // On the real console, the current downloaded progress and the total size of the content gets
    // returned. Since we do not support chunked downloads on the host, always return the content
    // length if the download is complete and 0 otherwise.
    u32 content_length = 0;
    const bool is_complete = http_context.request_future.wait_for(std::chrono::milliseconds(0)) ==
                             std::future_status::ready;
    if (is_complete) {
        const auto& headers = http_context.response.headers;
        const auto& it = headers.find("Content-Length");
        if (it != headers.end()) {
            content_length = std::stoi(it->second);
        }
    }

    LOG_DEBUG(Service_HTTP, "current={}, total={}", http_context.current_copied_data,
              content_length);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(http_context.current_copied_data));
    rb.Push(content_length);
}

SessionData* HTTP_C::EnsureSessionInitialized(Kernel::HLERequestContext& ctx,
                                              IPC::RequestParser rp) {
    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to make a request on an uninitialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return nullptr;
    }

    return session_data;
}

bool HTTP_C::PerformStateChecks(Kernel::HLERequestContext& ctx, IPC::RequestParser rp,
                                Context::Handle context_handle) {
    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return false;
    }

    // This command can only be called with a bound context
    if (!session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Tried to make a request without a bound context");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorNotImplemented);
        return false;
    }

    if (session_data->current_http_context != context_handle) {
        LOG_ERROR(
            Service_HTTP,
            "Tried to make a request on a mismatched session input context={} session context={}",
            context_handle, *session_data->current_http_context);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ErrorStateError);
        return false;
    }

    return true;
}

void HTTP_C::DecryptClCertA() {
    static constexpr u32 iv_length = 16;

    FileSys::NCCHArchive archive(0x0004001b00010002, Service::FS::MediaType::NAND);

    std::array<char, 8> exefs_filepath;
    FileSys::Path file_path = FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::RomFS, exefs_filepath);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = archive.OpenFile(file_path, open_mode, 0);
    if (file_result.Failed()) {
        LOG_ERROR(Service_HTTP, "ClCertA file missing");
        return;
    }

    auto romfs = std::move(file_result).Unwrap();
    std::vector<u8> romfs_buffer(romfs->GetSize());
    romfs->Read(0, romfs_buffer.size(), romfs_buffer.data());
    romfs->Close();

    if (!HW::AES::IsNormalKeyAvailable(HW::AES::KeySlotID::SSLKey)) {
        LOG_ERROR(Service_HTTP, "NormalKey in KeySlot 0x0D missing");
        return;
    }
    HW::AES::AESKey key = HW::AES::GetNormalKey(HW::AES::KeySlotID::SSLKey);

    const RomFS::RomFSFile cert_file =
        RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-cert.bin"});
    if (cert_file.Length() == 0) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin missing");
        return;
    }
    if (cert_file.Length() <= iv_length) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin size is too small. Size: {}",
                  cert_file.Length());
        return;
    }

    std::vector<u8> cert_data(cert_file.Length() - iv_length);

    using CryptoPP::AES;
    CryptoPP::CBC_Mode<AES>::Decryption aes_cert;
    std::array<u8, iv_length> cert_iv;
    std::memcpy(cert_iv.data(), cert_file.Data(), iv_length);
    aes_cert.SetKeyWithIV(key.data(), AES::BLOCKSIZE, cert_iv.data());
    aes_cert.ProcessData(cert_data.data(), cert_file.Data() + iv_length,
                         cert_file.Length() - iv_length);

    const RomFS::RomFSFile key_file =
        RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-key.bin"});
    if (key_file.Length() == 0) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin missing");
        return;
    }
    if (key_file.Length() <= iv_length) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin size is too small. Size: {}",
                  key_file.Length());
        return;
    }

    std::vector<u8> key_data(key_file.Length() - iv_length);

    CryptoPP::CBC_Mode<AES>::Decryption aes_key;
    std::array<u8, iv_length> key_iv;
    std::memcpy(key_iv.data(), key_file.Data(), iv_length);
    aes_key.SetKeyWithIV(key.data(), AES::BLOCKSIZE, key_iv.data());
    aes_key.ProcessData(key_data.data(), key_file.Data() + iv_length,
                        key_file.Length() - iv_length);

    ClCertA.certificate = std::move(cert_data);
    ClCertA.private_key = std::move(key_data);
    ClCertA.init = true;
}

HTTP_C::HTTP_C() : ServiceFramework("http:C", 32) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &HTTP_C::Initialize, "Initialize"},
        {0x0002, &HTTP_C::CreateContext, "CreateContext"},
        {0x0003, &HTTP_C::CloseContext, "CloseContext"},
        {0x0004, &HTTP_C::CancelConnection, "CancelConnection"},
        {0x0005, &HTTP_C::GetRequestState, "GetRequestState"},
        {0x0006, &HTTP_C::GetDownloadSizeState, "GetDownloadSizeState"},
        {0x0007, nullptr, "GetRequestError"},
        {0x0008, &HTTP_C::InitializeConnectionSession, "InitializeConnectionSession"},
        {0x0009, &HTTP_C::BeginRequest, "BeginRequest"},
        {0x000A, &HTTP_C::BeginRequestAsync, "BeginRequestAsync"},
        {0x000B, &HTTP_C::ReceiveData, "ReceiveData"},
        {0x000C, &HTTP_C::ReceiveDataTimeout, "ReceiveDataTimeout"},
        {0x000D, nullptr, "SetProxy"},
        {0x000E, &HTTP_C::SetProxyDefault, "SetProxyDefault"},
        {0x000F, nullptr, "SetBasicAuthorization"},
        {0x0010, nullptr, "SetSocketBufferSize"},
        {0x0011, &HTTP_C::AddRequestHeader, "AddRequestHeader"},
        {0x0012, &HTTP_C::AddPostDataAscii, "AddPostDataAscii"},
        {0x0013, &HTTP_C::AddPostDataBinary, "AddPostDataBinary"},
        {0x0014, &HTTP_C::AddPostDataRaw, "AddPostDataRaw"},
        {0x0015, &HTTP_C::SetPostDataType, "SetPostDataType"},
        {0x0016, &HTTP_C::SendPostDataAscii, "SendPostDataAscii"},
        {0x0017, &HTTP_C::SendPostDataAsciiTimeout, "SendPostDataAsciiTimeout"},
        {0x0018, &HTTP_C::SendPostDataBinary, "SendPostDataBinary"},
        {0x0019, &HTTP_C::SendPostDataBinaryTimeout, "SendPostDataBinaryTimeout"},
        {0x001A, &HTTP_C::SendPostDataRaw, "SendPostDataRaw"},
        {0x001B, &HTTP_C::SendPostDataRawTimeout, "SendPostDataRawTimeout"},
        {0x001C, &HTTP_C::SetPostDataEncoding, "SetPostDataEncoding"},
        {0x001D, &HTTP_C::NotifyFinishSendPostData, "NotifyFinishSendPostData"},
        {0x001E, &HTTP_C::GetResponseHeader, "GetResponseHeader"},
        {0x001F, &HTTP_C::GetResponseHeaderTimeout, "GetResponseHeaderTimeout"},
        {0x0020, nullptr, "GetResponseData"},
        {0x0021, nullptr, "GetResponseDataTimeout"},
        {0x0022, &HTTP_C::GetResponseStatusCode, "GetResponseStatusCode"},
        {0x0023, &HTTP_C::GetResponseStatusCodeTimeout, "GetResponseStatusCodeTimeout"},
        {0x0024, &HTTP_C::AddTrustedRootCA, "AddTrustedRootCA"},
        {0x0025, &HTTP_C::AddDefaultCert, "AddDefaultCert"},
        {0x0026, nullptr, "SelectRootCertChain"},
        {0x0027, nullptr, "SetClientCert"},
        {0x0028, &HTTP_C::SetDefaultClientCert, "SetDefaultClientCert"},
        {0x0029, &HTTP_C::SetClientCertContext, "SetClientCertContext"},
        {0x002A, &HTTP_C::GetSSLError, "GetSSLError"},
        {0x002B, &HTTP_C::SetSSLOpt, "SetSSLOpt"},
        {0x002C, nullptr, "SetSSLClearOpt"},
        {0x002D, nullptr, "CreateRootCertChain"},
        {0x002E, nullptr, "DestroyRootCertChain"},
        {0x002F, nullptr, "RootCertChainAddCert"},
        {0x0030, nullptr, "RootCertChainAddDefaultCert"},
        {0x0031, nullptr, "RootCertChainRemoveCert"},
        {0x0032, &HTTP_C::OpenClientCertContext, "OpenClientCertContext"},
        {0x0033, &HTTP_C::OpenDefaultClientCertContext, "OpenDefaultClientCertContext"},
        {0x0034, &HTTP_C::CloseClientCertContext, "CloseClientCertContext"},
        {0x0035, nullptr, "SetDefaultProxy"},
        {0x0036, nullptr, "ClearDNSCache"},
        {0x0037, &HTTP_C::SetKeepAlive, "SetKeepAlive"},
        {0x0038, &HTTP_C::SetPostDataTypeSize, "SetPostDataTypeSize"},
        {0x0039, &HTTP_C::Finalize, "Finalize"},
        // clang-format on
    };
    RegisterHandlers(functions);

    DecryptClCertA();
}

std::shared_ptr<HTTP_C> GetService(Core::System& system) {
    return system.ServiceManager().GetService<HTTP_C>("http:C");
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<HTTP_C>()->InstallAsService(service_manager);
}
} // namespace Service::HTTP
