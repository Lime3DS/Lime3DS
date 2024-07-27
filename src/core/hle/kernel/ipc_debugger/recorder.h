// Copyright 2019 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"

namespace Kernel {
class ClientSession;
class Thread;
} // namespace Kernel

namespace IPCDebugger {

/**
 * Record of a kernel object, for debugging purposes.
 */
struct ObjectInfo {
    std::string type;
    std::string name;
    int id = -1;
};

/**
 * Status of a request.
 */
enum class RequestStatus {
    Invalid,          ///< Invalid status
    Sent,             ///< The request is sent to the kernel and is waiting to be handled
    Handling,         ///< The request is being handled
    Handled,          ///< The request is handled with reply sent
    HLEUnimplemented, ///< The request is unimplemented by HLE, and unhandled
};

/**
 * Record of an IPC request.
 */
struct RequestRecord {
    int id;
    RequestStatus status = RequestStatus::Invalid;
    ObjectInfo client_process;
    ObjectInfo client_thread;
    ObjectInfo client_session;
    ObjectInfo client_port;    // Not available for portless
    ObjectInfo server_process; // Only available for LLE requests
    ObjectInfo server_thread;  // Only available for LLE requests
    ObjectInfo server_session;
    std::string function_name; // Not available for LLE or portless
    bool is_hle = false;
    // Request info is only available when status is not `Invalid` or `Sent`
    std::vector<u32> untranslated_request_cmdbuf;
    std::vector<u32> translated_request_cmdbuf;
    // Reply info is only available when status is `Handled`
    std::vector<u32> untranslated_reply_cmdbuf;
    std::vector<u32> translated_reply_cmdbuf;
};

using CallbackType = std::function<void(const RequestRecord&)>;
using CallbackHandle = std::shared_ptr<CallbackType>;

class Recorder {
public:
    explicit Recorder();
    ~Recorder();

    /**
     * Returns whether the recorder is enabled.
     */
    bool IsEnabled() const;

    /**
     * Registers a request into the recorder. The request is then assoicated with the client thread.
     */
    void RegisterRequest(const std::shared_ptr<Kernel::ClientSession>& client_session,
                         const std::shared_ptr<Kernel::Thread>& client_thread);

    /**
     * Sets the request information of the request record associated with the client thread.
     * When the server thread is empty, the request will be considered HLE.
     */
    void SetRequestInfo(const std::shared_ptr<Kernel::Thread>& client_thread,
                        std::vector<u32> untranslated_cmdbuf, std::vector<u32> translated_cmdbuf,
                        const std::shared_ptr<Kernel::Thread>& server_thread = {});

    /**
     * Sets the reply information of the request record assoicated with the client thread.
     * The request is then unlinked from the client thread.
     */
    void SetReplyInfo(const std::shared_ptr<Kernel::Thread>& client_thread,
                      std::vector<u32> untranslated_cmdbuf, std::vector<u32> translated_cmdbuf);

    /**
     * Set the status of a record to HLEUnimplemented.
     */
    void SetHLEUnimplemented(const std::shared_ptr<Kernel::Thread>& client_thread);

    /**
     * Set the status of the debugger (enabled/disabled).
     */
    void SetEnabled(bool enabled);

    CallbackHandle BindCallback(CallbackType callback);
    void UnbindCallback(const CallbackHandle& handle);

private:
    void InvokeCallbacks(const RequestRecord& request);

    std::unordered_map<u32, std::unique_ptr<RequestRecord>> record_map;
    int record_count{};

    // Temporary client session map for function name handling
    std::unordered_map<u32, std::shared_ptr<Kernel::ClientSession>> client_session_map;

    std::atomic_bool enabled{false};

    std::set<CallbackHandle> callbacks;
    mutable std::shared_mutex callback_mutex;
};

} // namespace IPCDebugger
