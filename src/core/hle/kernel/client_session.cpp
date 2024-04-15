// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/thread.h"

SERIALIZE_EXPORT_IMPL(Kernel::ClientSession)

namespace Kernel {

ClientSession::ClientSession(KernelSystem& kernel) : Object(kernel) {}
ClientSession::~ClientSession() {
    // This destructor will be called automatically when the last ClientSession handle is closed by
    // the emulated application.

    // Local references to ServerSession and SessionRequestHandler are necessary to guarantee they
    // will be kept alive until after ClientDisconnected() returns.
    std::shared_ptr<ServerSession> server = SharedFrom(parent->server);
    if (server) {
        std::shared_ptr<SessionRequestHandler> hle_handler = server->hle_handler;
        if (hle_handler)
            hle_handler->ClientDisconnected(server);

        // Clean up the list of client threads with pending requests, they are unneeded now that the
        // client endpoint is closed.
        server->pending_requesting_threads.clear();
        server->currently_handling = nullptr;
    }

    parent->client = nullptr;

    if (server) {
        // Notify any threads waiting on the ServerSession that the endpoint has been closed. Note
        // that this call has to happen after `Session::client` has been set to nullptr to let the
        // ServerSession know that the client endpoint has been closed.
        server->WakeupAllWaitingThreads();
    }
}

Result ClientSession::SendSyncRequest(std::shared_ptr<Thread> thread) {
    // Keep ServerSession alive until we're done working with it.
    std::shared_ptr<ServerSession> server = SharedFrom(parent->server);
    R_UNLESS(server, ResultSessionClosed);

    // Signal the server session that new data is available
    return server->HandleSyncRequest(std::move(thread));
}

template <class Archive>
void ClientSession::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<Object>(*this);
    ar & name;
    ar & parent;
}
SERIALIZE_IMPL(ClientSession)

} // namespace Kernel
