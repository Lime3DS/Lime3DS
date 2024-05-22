// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <vector>
#include <boost/container/small_vector.hpp>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "common/serialization/boost_small_vector.hpp"
#include "common/swap.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_session.h"

namespace Service {
class ServiceFrameworkBase;
}

namespace Memory {
class MemorySystem;
}

namespace Kernel {

class HandleTable;
class Process;
class Thread;
class Event;
class HLERequestContext;
class KernelSystem;

/**
 * Interface implemented by HLE Session handlers.
 * This can be provided to a ServerSession in order to hook into several relevant events
 * (such as a new connection or a SyncRequest) so they can be implemented in the emulator.
 */
class SessionRequestHandler : public std::enable_shared_from_this<SessionRequestHandler> {
public:
    virtual ~SessionRequestHandler() = default;

    /**
     * Handles a sync request from the emulated application.
     * @param context holds all the information relevant to his request (ServerSession, Translated
     * command buffer, etc).
     */
    virtual void HandleSyncRequest(Kernel::HLERequestContext& context) = 0;

    /**
     * Signals that a client has just connected to this HLE handler and keeps the
     * associated ServerSession alive for the duration of the connection.
     * @param server_session Owning pointer to the ServerSession associated with the connection.
     */
    virtual void ClientConnected(std::shared_ptr<ServerSession> server_session);

    /**
     * Signals that a client has just disconnected from this HLE handler and releases the
     * associated ServerSession.
     * @param server_session ServerSession associated with the connection.
     */
    virtual void ClientDisconnected(std::shared_ptr<ServerSession> server_session);

    /// Empty placeholder structure for services with no per-session data. The session data classes
    /// in each service must inherit from this.
    struct SessionDataBase {
        virtual ~SessionDataBase() = default;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int);
        friend class boost::serialization::access;
    };

    struct SessionInfo {
        SessionInfo(std::shared_ptr<ServerSession> session, std::unique_ptr<SessionDataBase> data);

        std::shared_ptr<ServerSession> session;
        std::unique_ptr<SessionDataBase> data;

    private:
        SessionInfo() = default;
        template <class Archive>
        void serialize(Archive& ar, const unsigned int);
        friend class boost::serialization::access;
    };

protected:
    /// Creates the storage for the session data of the service.
    virtual std::unique_ptr<SessionDataBase> MakeSessionData() = 0;

    /// Returns the session data associated with the server session.
    template <typename T>
    T* GetSessionData(std::shared_ptr<ServerSession> session) {
        static_assert(std::is_base_of<SessionDataBase, T>(),
                      "T is not a subclass of SessionDataBase");
        auto itr = std::find_if(connected_sessions.begin(), connected_sessions.end(),
                                [&](const SessionInfo& info) { return info.session == session; });
        ASSERT(itr != connected_sessions.end());
        return static_cast<T*>(itr->data.get());
    }

    /// List of sessions that are connected to this handler. A ServerSession whose server endpoint
    /// is an HLE implementation is kept alive by this list for the duration of the connection.
    std::vector<SessionInfo> connected_sessions;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

// NOTE: The below classes are ephemeral and don't need serialization

class MappedBuffer {
public:
    MappedBuffer(Memory::MemorySystem& memory, std::shared_ptr<Process> process, u32 descriptor,
                 VAddr address, u32 id);

    // interface for service
    void Read(void* dest_buffer, std::size_t offset, std::size_t size);
    void Write(const void* src_buffer, std::size_t offset, std::size_t size);
    std::size_t GetSize() const {
        return size;
    }

    // interface for ipc helper
    u32 GenerateDescriptor() const {
        return IPC::MappedBufferDesc(size, perms);
    }

    u32 GetId() const {
        return id;
    }

private:
    friend class HLERequestContext;
    Memory::MemorySystem* memory;
    u32 id;
    VAddr address;
    std::shared_ptr<Process> process;
    u32 size;
    IPC::MappedBufferPermissions perms;

    MappedBuffer();

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & id;
        ar & address;
        ar & process;
        ar & size;
        ar & perms;
    }
    friend class boost::serialization::access;
};

/**
 * Class containing information about an in-flight IPC request being handled by an HLE service
 * implementation.
 *
 * HLE handle protocol
 * ===================
 *
 * To avoid needing HLE services to keep a separate handle table, or having to directly modify the
 * requester's table, a tweaked protocol is used to receive and send handles in requests. The kernel
 * will decode the incoming handles into object pointers and insert a id in the buffer where the
 * handle would normally be. The service then calls GetIncomingHandle() with that id to get the
 * pointer to the object. Similarly, instead of inserting a handle into the command buffer, the
 * service calls AddOutgoingHandle() and stores the returned id where the handle would normally go.
 *
 * The end result is similar to just giving services their own real handle tables, but since these
 * ids are local to a specific context, it avoids requiring services to manage handles for objects
 * across multiple calls and ensuring that unneeded handles are cleaned up.
 *
 * HLE mapped buffer protocol
 * ==========================
 *
 * HLE services don't have their own virtual memory space, a tweaked protocol is used to simulate
 * memory mapping. The kernel will wrap the incoming buffers into a memory interface on which HLE
 * services can operate, and insert a id in the buffer where the vaddr would normally be. The
 * service then calls GetMappedBuffer with that id to get the memory interface. On response, like
 * real services pushing back the mapped buffer address to unmap it, HLE services push back the
 * id of the memory interface and let kernel convert it back to client vaddr. No real unmapping is
 * needed in this case, though.
 */
class HLERequestContext : public std::enable_shared_from_this<HLERequestContext> {
public:
    HLERequestContext(KernelSystem& kernel, std::shared_ptr<ServerSession> session,
                      std::shared_ptr<Thread> thread);
    ~HLERequestContext();

    /// Returns a pointer to the IPC command buffer for this request.
    u32* CommandBuffer() {
        return cmd_buf.data();
    }

    /// Returns the command header from the IPC command buffer.
    IPC::Header CommandHeader() const {
        return {cmd_buf[0]};
    }

    /// Returns the Command ID from the IPC command buffer.
    u16 CommandID() const {
        return static_cast<u16>(CommandHeader().command_id.Value());
    }

    /**
     * Returns the session through which this request was made. This can be used as a map key to
     * access per-client data on services.
     */
    std::shared_ptr<ServerSession> Session() const {
        return session;
    }

    /**
     * Returns the client thread that made the service request.
     */
    std::shared_ptr<Thread> ClientThread() const {
        return thread;
    }

    class WakeupCallback {
    public:
        virtual ~WakeupCallback() = default;
        virtual void WakeUp(std::shared_ptr<Thread> thread, HLERequestContext& context,
                            ThreadWakeupReason reason) = 0;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {}
        friend class boost::serialization::access;
    };

    /**
     * Puts the specified guest thread to sleep until the returned event is signaled or until the
     * specified timeout expires.
     * @param reason Reason for pausing the thread, to be used for debugging purposes.
     * @param timeout Timeout in nanoseconds after which the thread will be awoken and the callback
     * invoked with a Timeout reason.
     * @param callback Callback to be invoked when the thread is resumed. This callback must write
     * the entire command response once again, regardless of the state of it before this function
     * was called.
     * @returns Event that when signaled will resume the thread and call the callback function.
     */
    std::shared_ptr<Event> SleepClientThread(const std::string& reason,
                                             std::chrono::nanoseconds timeout,
                                             std::shared_ptr<WakeupCallback> callback);

private:
    template <typename ResultFunctor>
    class AsyncWakeUpCallback : public WakeupCallback {
    public:
        explicit AsyncWakeUpCallback(ResultFunctor res_functor, std::future<void> fut)
            : functor(res_functor) {
            future = std::move(fut);
        }

        void WakeUp(std::shared_ptr<Kernel::Thread> thread, Kernel::HLERequestContext& ctx,
                    Kernel::ThreadWakeupReason reason) {
            functor(ctx);
        }

    private:
        ResultFunctor functor;
        std::future<void> future;

        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            if (!Archive::is_loading::value && future.valid()) {
                future.wait();
            }
            ar & functor;
        }
        friend class boost::serialization::access;
    };

public:
    /**
     * Puts the game thread to sleep and calls the specified async_section asynchronously.
     * Once the execution of the async section finishes, result_function is called. Use this
     * mechanism to run blocking IO operations, so that other game threads are allowed to run
     * while the one performing the blocking operation waits.
     * @param async_section Callable that takes Kernel::HLERequestContext& as argument
     * and returns the amount of nanoseconds to wait before calling result_function.
     * This callable is ran asynchronously.
     * @param result_function Callable that takes Kernel::HLERequestContext& as argument
     * and doesn't return anything. This callable is ran from the emulator thread
     * and can be used to set the IPC result.
     * @param really_async If set to false, it will call both async_section and result_function
     * from the emulator thread.
     */
    template <typename AsyncFunctor, typename ResultFunctor>
    void RunAsync(AsyncFunctor async_section, ResultFunctor result_function,
                  bool really_async = true) {

        if (really_async) {
            this->SleepClientThread(
                "RunAsync", std::chrono::nanoseconds(-1),
                std::make_shared<AsyncWakeUpCallback<ResultFunctor>>(
                    result_function,
                    std::move(std::async(std::launch::async, [this, async_section] {
                        s64 sleep_for = async_section(*this);
                        this->thread->WakeAfterDelay(sleep_for, true);
                    }))));

        } else {
            s64 sleep_for = async_section(*this);
            if (sleep_for > 0) {
                auto parallel_wakeup = std::make_shared<AsyncWakeUpCallback<ResultFunctor>>(
                    result_function, std::move(std::future<void>()));
                this->SleepClientThread("RunAsync", std::chrono::nanoseconds(sleep_for),
                                        parallel_wakeup);
            } else {
                result_function(*this);
            }
        }
    }

    /**
     * Resolves a object id from the request command buffer into a pointer to an object. See the
     * "HLE handle protocol" section in the class documentation for more details.
     */
    std::shared_ptr<Object> GetIncomingHandle(u32 id_from_cmdbuf) const;

    /**
     * Adds an outgoing object to the response, returning the id which should be used to reference
     * it. See the "HLE handle protocol" section in the class documentation for more details.
     */
    u32 AddOutgoingHandle(std::shared_ptr<Object> object);

    /**
     * Discards all Objects from the context, invalidating all ids. This may be called after reading
     * out all incoming objects, so that the buffer memory can be re-used for outgoing handles, but
     * this is not required.
     */
    void ClearIncomingObjects();

    /**
     * Retrieves the static buffer identified by the input buffer_id. The static buffer *must* have
     * been created in PopulateFromIncomingCommandBuffer by way of an input StaticBuffer descriptor.
     */
    const std::vector<u8>& GetStaticBuffer(u8 buffer_id) const;

    /**
     * Sets up a static buffer that will be copied to the target process when the request is
     * translated.
     */
    void AddStaticBuffer(u8 buffer_id, std::vector<u8> data);

    /**
     * Gets a memory interface by the id from the request command buffer. See the "HLE mapped buffer
     * protocol" section in the class documentation for more details.
     */
    MappedBuffer& GetMappedBuffer(u32 id_from_cmdbuf);

    /// Populates this context with data from the requesting process/thread.
    Result PopulateFromIncomingCommandBuffer(const u32_le* src_cmdbuf,
                                             std::shared_ptr<Process> src_process);
    /// Writes data from this context back to the requesting process/thread.
    Result WriteToOutgoingCommandBuffer(u32_le* dst_cmdbuf, Process& dst_process) const;

    /// Reports an unimplemented function.
    void ReportUnimplemented() const;

    class ThreadCallback;
    friend class ThreadCallback;

private:
    KernelSystem& kernel;
    std::array<u32, IPC::COMMAND_BUFFER_LENGTH> cmd_buf;
    std::shared_ptr<ServerSession> session;
    std::shared_ptr<Thread> thread;
    // TODO(yuriks): Check common usage of this and optimize size accordingly
    boost::container::small_vector<std::shared_ptr<Object>, 8> request_handles;
    // The static buffers will be created when the IPC request is translated.
    std::array<std::vector<u8>, IPC::MAX_STATIC_BUFFERS> static_buffers;
    // The mapped buffers will be created when the IPC request is translated
    boost::container::small_vector<MappedBuffer, 8> request_mapped_buffers;

    HLERequestContext();
    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Kernel

BOOST_CLASS_EXPORT_KEY(Kernel::SessionRequestHandler)
BOOST_CLASS_EXPORT_KEY(Kernel::SessionRequestHandler::SessionDataBase)
BOOST_CLASS_EXPORT_KEY(Kernel::SessionRequestHandler::SessionInfo)
BOOST_CLASS_EXPORT_KEY(Kernel::HLERequestContext)
BOOST_CLASS_EXPORT_KEY(Kernel::HLERequestContext::ThreadCallback)
