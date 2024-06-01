// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/optional.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/weak_ptr.hpp>
#include <httplib.h>
#include "common/thread.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace IPC {
class RequestParser;
}

namespace Service::HTTP {

enum class RequestMethod : u8 {
    None = 0x0,
    Get = 0x1,
    Post = 0x2,
    Head = 0x3,
    Put = 0x4,
    Delete = 0x5,
    PostEmpty = 0x6,
    PutEmpty = 0x7,
};

/// The number of request methods, any valid method must be less than this.
constexpr u32 TotalRequestMethods = 8;

enum class RequestState : u8 {
    /// Request has not started yet.
    NotStarted = 0x1,

    /// Request in progress, connecting to server.
    ConnectingToServer = 0x5,

    /// Request in progress, sending HTTP request.
    SendingRequest = 0x6,

    // Request in progress, receiving HTTP response and headers.
    ReceivingResponse = 0x7,

    /// Request in progress, receiving HTTP body. The HTTP module may
    /// get stuck in this state if the internal receive buffer gets full.
    /// Once the user calls ReceiveData it will get unstuck.
    ReceivingBody = 0x8,

    /// Request is finished and all data has been received. HTTP transitions
    /// to the Completed state shortly afterwards after some cleanup.
    Received = 0x9,

    /// Request is completed.
    Completed = 0xA,
};

enum class PostDataEncoding : u8 {
    Auto = 0x0,
    AsciiForm = 0x1,
    MultipartForm = 0x2,
};

enum class PostDataType : u8 {
    AsciiForm = 0x0,
    MultipartForm = 0x1,
    Raw = 0x2,
};

enum class ClientCertID : u32 {
    Default = 0x40, // Default client cert
};

struct URLInfo {
    bool is_https;
    std::string host;
    int port;
    std::string path;
};

/// Represents a client certificate along with its private key, stored as a byte array of DER data.
/// There can only be at most one client certificate context attached to an HTTP context at any
/// given time.
struct ClientCertContext {
    using Handle = u32;
    Handle handle;
    u32 session_id;
    u8 cert_id;
    std::vector<u8> certificate;
    std::vector<u8> private_key;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & handle;
        ar & session_id;
        ar & cert_id;
        ar & certificate;
        ar & private_key;
    }
    friend class boost::serialization::access;
};

/// Represents a root certificate chain, it contains a list of DER-encoded certificates for
/// verifying HTTP requests. An HTTP context can have at most one root certificate chain attached to
/// it, but the chain may contain an arbitrary number of certificates in it.
struct RootCertChain {
    struct RootCACert {
        using Handle = u32;
        Handle handle;
        u32 session_id;
        std::vector<u8> certificate;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & handle;
            ar & session_id;
            ar & certificate;
        }
        friend class boost::serialization::access;
    };

    using Handle = u32;
    Handle handle;
    u32 session_id;
    std::vector<RootCACert> certificates;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & handle;
        ar & session_id;
        ar & certificates;
    }
    friend class boost::serialization::access;
};

struct ClCertAData {
    std::vector<u8> certificate;
    std::vector<u8> private_key;
    bool init = false;
};

/// Represents an HTTP context.
class Context final {
public:
    using Handle = u32;

    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    struct Proxy {
        std::string url;
        std::string username;
        std::string password;
        u16 port;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & url;
            ar & username;
            ar & password;
            ar & port;
        }
        friend class boost::serialization::access;
    };

    struct BasicAuth {
        std::string username;
        std::string password;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & username;
            ar & password;
        }
        friend class boost::serialization::access;
    };

    struct RequestHeader {
        RequestHeader(std::string name, std::string value) : name(name), value(value) {};
        std::string name;
        std::string value;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & name;
            ar & value;
        }
        friend class boost::serialization::access;
    };

    struct SSLConfig {
        u32 options;
        std::weak_ptr<ClientCertContext> client_cert_ctx;
        std::weak_ptr<RootCertChain> root_ca_chain;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & options;
            ar & client_cert_ctx;
            ar & root_ca_chain;
        }
        friend class boost::serialization::access;
    };

    struct Param {
        Param(const std::vector<u8>& value)
            : name(value.begin(), value.end()), value(value.begin(), value.end()) {};
        Param(const std::string& name, const std::string& value) : name(name), value(value) {};
        Param(const std::string& name, const std::vector<u8>& value)
            : name(name), value(value.begin(), value.end()), is_binary(true) {};
        std::string name;
        std::string value;
        bool is_binary = false;

        httplib::MultipartFormData ToMultipartForm() const {
            httplib::MultipartFormData form;
            form.name = name;
            form.content = value;
            if (is_binary) {
                form.content_type = "application/octet-stream";
                // TODO(DaniElectra): httplib doesn't support setting Content-Transfer-Encoding,
                // while the 3DS sets Content-Transfer-Encoding: binary if a binary value is set
            }

            return form;
        }

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & name;
            ar & value;
            ar & is_binary;
        }
        friend class boost::serialization::access;
    };

    using Params = std::multimap<std::string, Param>;

    Handle handle;
    u32 session_id;
    std::string url;
    RequestMethod method;
    std::atomic<RequestState> state = RequestState::NotStarted;
    std::optional<Proxy> proxy;
    std::optional<BasicAuth> basic_auth;
    SSLConfig ssl_config{};
    u32 socket_buffer_size;
    std::vector<RequestHeader> headers;
    const ClCertAData* clcert_data;
    Params post_data;
    std::string post_data_raw;
    PostDataEncoding post_data_encoding = PostDataEncoding::Auto;
    PostDataType post_data_type;
    std::string multipart_boundary;
    bool force_multipart = false;
    bool chunked_request = false;
    u32 chunked_content_length;

    std::future<void> request_future;
    std::atomic<u64> current_download_size_bytes;
    std::atomic<u64> total_download_size_bytes;
    std::size_t current_copied_data;
    bool uses_default_client_cert{};
    httplib::Response response;
    Common::Event finish_post_data;

    void ParseAsciiPostData();
    std::string ParseMultipartFormData();
    void MakeRequest();
    void MakeRequestNonSSL(httplib::Request& request, const URLInfo& url_info,
                           std::vector<Context::RequestHeader>& pending_headers);
    void MakeRequestSSL(httplib::Request& request, const URLInfo& url_info,
                        std::vector<Context::RequestHeader>& pending_headers);
    bool ContentProvider(size_t offset, size_t length, httplib::DataSink& sink);
    bool ChunkedContentProvider(size_t offset, httplib::DataSink& sink);
    std::size_t HandleHeaderWrite(std::vector<Context::RequestHeader>& pending_headers,
                                  httplib::Stream& strm, httplib::Headers& httplib_headers);
};

struct SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
    /// The HTTP context that is currently bound to this session, this can be empty if no context
    /// has been bound. Certain commands can only be called on a session with a bound context.
    boost::optional<Context::Handle> current_http_context;

    u32 session_id;

    /// Number of HTTP contexts that are currently opened in this session.
    u32 num_http_contexts = 0;
    /// Number of ClientCert contexts that are currently opened in this session.
    u32 num_client_certs = 0;

    /// Whether this session has been initialized in some way, be it via Initialize or
    /// InitializeConnectionSession.
    bool initialized = false;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(
            *this);
        ar & current_http_context;
        ar & session_id;
        ar & num_http_contexts;
        ar & num_client_certs;
        ar & initialized;
    }
    friend class boost::serialization::access;
};

class HTTP_C final : public ServiceFramework<HTTP_C, SessionData> {
public:
    HTTP_C();

    const ClCertAData& GetClCertA() const {
        return ClCertA;
    }

private:
    /**
     * HTTP_C::Initialize service function
     *  Inputs:
     *      1 : POST buffer size
     *      2 : 0x20
     *      3 : 0x0 (Filled with process ID by ARM11 Kernel)
     *      4 : 0x0
     *      5 : POST buffer memory block handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Initialize(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::CreateContext service function
     *  Inputs:
     *      1 : URL buffer size, including null-terminator
     *      2 : RequestMethod
     *      3 : (URLSize << 4) | 10
     *      4 : URL data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : HTTP context handle
     */
    void CreateContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::CreateContext service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CloseContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::CancelConnection service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CancelConnection(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::GetRequestState service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Request state
     */
    void GetRequestState(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::GetDownloadSizeState service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Total content data downloaded so far
     *      3 : Total content size from the "Content-Length" response header
     */
    void GetDownloadSizeState(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::InitializeConnectionSession service function
     *  Inputs:
     *      1 : HTTP context handle
     *      2 : 0x20, processID translate-header for the ARM11-kernel
     *      3 : processID set by the ARM11-kernel
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void InitializeConnectionSession(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::BeginRequest service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void BeginRequest(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::BeginRequestAsync service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void BeginRequestAsync(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetProxyDefault service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetProxyDefault(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::ReceiveData service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Buffer size
     *      3 : (OutSize<<4) | 12
     *      4 : Output data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ReceiveData(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::ReceiveDataTimeout service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Buffer size
     *    3-4 : u64 nanoseconds delay
     *      5 : (OutSize<<4) | 12
     *      6 : Output data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ReceiveDataTimeout(Kernel::HLERequestContext& ctx);

    /**
     * ReceiveDataImpl:
     *  Implements ReceiveData and ReceiveDataTimeout service functions
     */
    void ReceiveDataImpl(Kernel::HLERequestContext& ctx, bool timeout);

    /**
     * HTTP_C::AddRequestHeader service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Header name buffer size, including null-terminator.
     * 3 : Header value buffer size, including null-terminator.
     * 4 : (HeaderNameSize<<14) | 0xC02
     * 5 : Header name data pointer
     * 6 : (HeaderValueSize<<4) | 10
     * 7 : Header value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AddRequestHeader(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::AddPostDataAscii service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Form name buffer size, including null-terminator.
     * 3 : Form value buffer size, including null-terminator.
     * 4 : (FormNameSize<<14) | 0xC02
     * 5 : Form name data pointer
     * 6 : (FormValueSize<<4) | 10
     * 7 : Form value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AddPostDataAscii(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::AddPostDataBinary service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Form name buffer size, including null-terminator.
     * 3 : Form value buffer size
     * 4 : (FormNameSize<<14) | 0xC02
     * 5 : Form name data pointer
     * 6 : (FormValueSize<<4) | 10
     * 7 : Form value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AddPostDataBinary(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::AddPostDataRaw service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Post data length
     *      3-4: (Mapped buffer) Post data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-3: (Mapped buffer) Post data
     */
    void AddPostDataRaw(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetPostDataType service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Post data type
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetPostDataType(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SendPostDataAscii service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Form name buffer size, including null-terminator.
     * 3 : Form value buffer size, including null-terminator.
     * 4 : (FormNameSize<<14) | 0xC02
     * 5 : Form name data pointer
     * 6 : (FormValueSize<<4) | 10
     * 7 : Form value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SendPostDataAscii(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SendPostDataAsciiTimeout service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Form name buffer size, including null-terminator.
     * 3 : Form value buffer size, including null-terminator.
     * 4-5 : u64 nanoseconds delay
     * 6 : (FormNameSize<<14) | 0xC02
     * 7 : Form name data pointer
     * 8 : (FormValueSize<<4) | 10
     * 9 : Form value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SendPostDataAsciiTimeout(Kernel::HLERequestContext& ctx);

    /**
     * SendPostDataAsciiImpl:
     *  Implements SendPostDataAscii and SendPostDataAsciiTimeout service functions
     */
    void SendPostDataAsciiImpl(Kernel::HLERequestContext& ctx, bool timeout);

    /**
     * HTTP_C::SendPostDataBinary service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Form name buffer size, including null-terminator.
     * 3 : Form value buffer size
     * 4 : (FormNameSize<<14) | 0xC02
     * 5 : Form name data pointer
     * 6 : (FormValueSize<<4) | 10
     * 7 : Form value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SendPostDataBinary(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SendPostDataBinaryTimeout service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Form name buffer size, including null-terminator.
     * 3 : Form value buffer size
     * 4-5 : u64 nanoseconds delay
     * 6 : (FormNameSize<<14) | 0xC02
     * 7 : Form name data pointer
     * 8 : (FormValueSize<<4) | 10
     * 9 : Form value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SendPostDataBinaryTimeout(Kernel::HLERequestContext& ctx);

    /**
     * SendPostDataBinaryImpl:
     *  Implements SendPostDataBinary and SendPostDataBinaryTimeout service functions
     */
    void SendPostDataBinaryImpl(Kernel::HLERequestContext& ctx, bool timeout);

    /**
     * HTTP_C::SendPostDataRaw service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Post data length
     *      3-4: (Mapped buffer) Post data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-3: (Mapped buffer) Post data
     */
    void SendPostDataRaw(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SendPostDataRawTimeout service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Post data length
     *      3-4: u64 nanoseconds delay
     *      5-6: (Mapped buffer) Post data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-3: (Mapped buffer) Post data
     */
    void SendPostDataRawTimeout(Kernel::HLERequestContext& ctx);

    /**
     * SendPostDataRawImpl:
     *  Implements SendPostDataRaw and SendPostDataRawTimeout service functions
     */
    void SendPostDataRawImpl(Kernel::HLERequestContext& ctx, bool timeout);

    /**
     * HTTP_C::NotifyFinishSendPostData service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void NotifyFinishSendPostData(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetPostDataEncoding service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Post data encoding
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetPostDataEncoding(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::GetResponseHeader service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Header name length
     *      3 : Return value length
     *      4-5 : (Static buffer) Header name
     *      6-7 : (Mapped buffer) Header value
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Header value copied size
     *      3-4: (Mapped buffer) Header value
     */
    void GetResponseHeader(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::GetResponseHeaderTimeout service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Header name length
     *      3 : Return value length
     *      4-5 : u64 nanoseconds delay
     *      6-7 : (Static buffer) Header name
     *      8-9 : (Mapped buffer) Header value
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Header value copied size
     *      3-4: (Mapped buffer) Header value
     */
    void GetResponseHeaderTimeout(Kernel::HLERequestContext& ctx);

    /**
     * GetResponseHeaderImpl:
     *  Implements GetResponseHeader and GetResponseHeaderTimeout service functions
     */
    void GetResponseHeaderImpl(Kernel::HLERequestContext& ctx, bool timeout);

    /**
     * HTTP_C::GetResponseStatusCode service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : HTTP response status code
     */
    void GetResponseStatusCode(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::GetResponseStatusCode service function
     *  Inputs:
     *      1 : Context handle
     *    2-3 : u64 nanoseconds timeout
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : HTTP response status code
     */
    void GetResponseStatusCodeTimeout(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::AddTrustedRootCA service function
     *  Inputs:
     *      1 : Context handle
     *      2 : CA data length
     *      3-4: (Mapped buffer) CA data
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-3: (Mapped buffer) CA data
     */
    void AddTrustedRootCA(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::AddDefaultCert service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Cert ID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AddDefaultCert(Kernel::HLERequestContext& ctx);

    /**
     * GetResponseStatusCodeImpl:
     *  Implements GetResponseStatusCode and GetResponseStatusCodeTimeout service functions
     */
    void GetResponseStatusCodeImpl(Kernel::HLERequestContext& ctx, bool timeout);

    /**
     * HTTP_C::SetDefaultClientCert service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Client cert ID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetDefaultClientCert(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetClientCertContext service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Cert context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::GetSSLError service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Unknown
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : SSL Error code
     */
    void GetSSLError(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetSSLOpt service function
     *  Inputs:
     *      1 : Context handle
     *      2 : SSL Option
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetSSLOpt(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::OpenClientCertContext service function
     *  Inputs:
     *      1 :  Cert size
     *      2 :  Key size
     *      3 :  (CertSize<<4) | 10
     *      4 :  Pointer to input cert
     *      5 :  (KeySize<<4) | 10
     *      6 :  Pointer to input key
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void OpenClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::OpenDefaultClientCertContext service function
     *  Inputs:
     * 1 : CertID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Client Cert context handle
     */
    void OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::CloseClientCertContext service function
     *  Inputs:
     * 1 : ClientCert Handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CloseClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetKeepAlive service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Keep Alive Option
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetKeepAlive(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetPostDataTypeSize service function
     *  Inputs:
     *      1 : Context handle
     *      2 : Post data type
     *      3 : Content length size
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetPostDataTypeSize(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::Finalize service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Finalize(Kernel::HLERequestContext& ctx);

    [[nodiscard]] SessionData* EnsureSessionInitialized(Kernel::HLERequestContext& ctx,
                                                        IPC::RequestParser rp);

    [[nodiscard]] bool PerformStateChecks(Kernel::HLERequestContext& ctx, IPC::RequestParser rp,
                                          Context::Handle context_handle);

    void DecryptClCertA();

    std::shared_ptr<Kernel::SharedMemory> shared_memory = nullptr;

    /// The next number to use when a new HTTP session is initalized.
    u32 session_counter = 0;

    /// The next handle number to use when a new HTTP context is created.
    Context::Handle context_counter = 0;

    /// The next handle number to use when a new ClientCert context is created.
    ClientCertContext::Handle client_certs_counter = 0;

    /// Global list of HTTP contexts currently opened.
    std::unordered_map<Context::Handle, Context> contexts;

    // Get context from its handle
    inline Context& GetContext(const Context::Handle& handle) {
        auto it = contexts.find(handle);
        ASSERT(it != contexts.end());
        return it->second;
    }

    /// Global list of  ClientCert contexts currently opened.
    std::unordered_map<ClientCertContext::Handle, std::shared_ptr<ClientCertContext>> client_certs;

    ClCertAData ClCertA;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        // NOTE: Serialization of the HTTP service is on a 'best effort' basis.
        // There is a very good chance that saving/loading during a network connection will break,
        // regardless!
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar & ClCertA.certificate;
        ar & ClCertA.private_key;
        ar & ClCertA.init;
        ar & context_counter;
        ar & client_certs_counter;
        ar & client_certs;
        // NOTE: `contexts` is not serialized because it contains non-serializable data. (i.e.
        // handles to ongoing HTTP requests.) Serializing across HTTP contexts will break.
    }
    friend class boost::serialization::access;
};

std::shared_ptr<HTTP_C> GetService(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::HTTP

BOOST_CLASS_EXPORT_KEY(Service::HTTP::HTTP_C)
BOOST_CLASS_EXPORT_KEY(Service::HTTP::SessionData)
