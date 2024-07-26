// Copyright 2015 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include "common/assert.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/applets/applet.h"
#include "core/hle/result.h"

namespace HLE::Applets {

bool Applet::IsRunning() const {
    return is_running;
}

bool Applet::IsActive() const {
    return is_active;
}

Result Applet::ReceiveParameter(const Service::APT::MessageParameter& parameter) {
    switch (parameter.signal) {
    case Service::APT::SignalType::Wakeup: {
        Result result = Start(parameter);
        if (!result.IsError()) {
            is_active = true;
        }
        return result;
    }
    case Service::APT::SignalType::WakeupByCancel:
        return Finalize();
    default:
        return ReceiveParameterImpl(parameter);
    }
}

void Applet::SendParameter(const Service::APT::MessageParameter& parameter) {
    if (auto locked = manager.lock()) {
        locked->CancelAndSendParameter(parameter);
    } else {
        LOG_ERROR(Service_APT, "called after destructing applet manager");
    }
}

void Applet::CloseApplet(std::shared_ptr<Kernel::Object> object, const std::vector<u8>& buffer) {
    if (auto locked = manager.lock()) {
        locked->PrepareToCloseLibraryApplet(true, false, false);
        locked->CloseLibraryApplet(std::move(object), buffer);
    } else {
        LOG_ERROR(Service_APT, "called after destructing applet manager");
    }

    is_active = false;
    is_running = false;
}

} // namespace HLE::Applets
