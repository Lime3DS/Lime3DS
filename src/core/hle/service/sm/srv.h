// Copyright 2014 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <boost/serialization/export.hpp>
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class HLERequestContext;
class Semaphore;
} // namespace Kernel

namespace Service::SM {

/// Interface to "srv:" service
class SRV final : public ServiceFramework<SRV> {
public:
    explicit SRV(Core::System& system);
    ~SRV();

    class ThreadCallback;

private:
    void RegisterClient(Kernel::HLERequestContext& ctx);
    void EnableNotification(Kernel::HLERequestContext& ctx);
    void GetServiceHandle(Kernel::HLERequestContext& ctx);
    void Subscribe(Kernel::HLERequestContext& ctx);
    void Unsubscribe(Kernel::HLERequestContext& ctx);
    void PublishToSubscriber(Kernel::HLERequestContext& ctx);
    void RegisterService(Kernel::HLERequestContext& ctx);

    Core::System& system;
    std::shared_ptr<Kernel::Semaphore> notification_semaphore;
    std::unordered_map<std::string, std::shared_ptr<Kernel::Event>> get_service_handle_delayed_map;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Service::SM

SERVICE_CONSTRUCT(Service::SM::SRV)
BOOST_CLASS_EXPORT_KEY(Service::SM::SRV)
BOOST_CLASS_EXPORT_KEY(Service::SM::SRV::ThreadCallback)
