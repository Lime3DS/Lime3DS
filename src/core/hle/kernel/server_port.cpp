// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#include "common/archives.h"
#include "common/assert.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/thread.h"

SERIALIZE_EXPORT_IMPL(Kernel::ServerPort)

namespace Kernel {

ServerPort::ServerPort(KernelSystem& kernel) : WaitObject(kernel) {}
ServerPort::~ServerPort() {}

Result ServerPort::Accept(std::shared_ptr<ServerSession>* out_server_session) {
    R_UNLESS(!pending_sessions.empty(), ResultNoPendingSessions);

    *out_server_session = std::move(pending_sessions.back());
    pending_sessions.pop_back();
    return ResultSuccess;
}

bool ServerPort::ShouldWait(const Thread* thread) const {
    // If there are no pending sessions, we wait until a new one is added.
    return pending_sessions.size() == 0;
}

void ServerPort::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");
}

KernelSystem::PortPair KernelSystem::CreatePortPair(u32 max_sessions, std::string name) {
    auto server_port{std::make_shared<ServerPort>(*this)};
    auto client_port{std::make_shared<ClientPort>(*this)};

    server_port->name = name + "_Server";
    client_port->name = name + "_Client";
    client_port->server_port = server_port;
    client_port->max_sessions = max_sessions;
    client_port->active_sessions = 0;

    return std::make_pair(std::move(server_port), std::move(client_port));
}

template <class Archive>
void ServerPort::serialize(Archive& ar, const unsigned int) {
    ar& boost::serialization::base_object<WaitObject>(*this);
    ar & name;
    ar & pending_sessions;
    ar & hle_handler;
}
SERIALIZE_IMPL(ServerPort)

} // namespace Kernel
