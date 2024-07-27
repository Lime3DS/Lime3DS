// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class ClientSession;
class SessionRequestHandler;
} // namespace Kernel

namespace Service::SM {

class SRV;

constexpr Result ResultServiceNotRegistered(1, ErrorModule::SRV, ErrorSummary::WouldBlock,
                                            ErrorLevel::Temporary); // 0xD0406401
constexpr Result ResultMaxConnectionsReached(2, ErrorModule::SRV, ErrorSummary::WouldBlock,
                                             ErrorLevel::Temporary); // 0xD0406402
constexpr Result ResultInvalidNameSize(5, ErrorModule::SRV, ErrorSummary::WrongArgument,
                                       ErrorLevel::Permanent); // 0xD9006405
constexpr Result ResultAccessDenied(6, ErrorModule::SRV, ErrorSummary::InvalidArgument,
                                    ErrorLevel::Permanent); // 0xD8E06406
constexpr Result ResultNameContainsNul(7, ErrorModule::SRV, ErrorSummary::WrongArgument,
                                       ErrorLevel::Permanent); // 0xD9006407
constexpr Result ResultAlreadyRegistered(ErrorDescription::AlreadyExists, ErrorModule::OS,
                                         ErrorSummary::WrongArgument,
                                         ErrorLevel::Permanent); // 0xD9001BFC

class ServiceManager {
public:
    static void InstallInterfaces(Core::System& system);

    explicit ServiceManager(Core::System& system);

    Result RegisterService(std::shared_ptr<Kernel::ServerPort>* out_server_port, std::string name,
                           u32 max_sessions);
    Result GetServicePort(std::shared_ptr<Kernel::ClientPort>* out_client_port,
                          const std::string& name);
    Result ConnectToService(std::shared_ptr<Kernel::ClientSession>* out_client_session,
                            const std::string& name);
    // For IPC Recorder
    std::string GetServiceNameByPortId(u32 port) const;

    template <typename T>
    std::shared_ptr<T> GetService(const std::string& service_name) const {
        static_assert(std::is_base_of_v<Kernel::SessionRequestHandler, T>,
                      "Not a base of ServiceFrameworkBase");
        auto service = registered_services.find(service_name);
        if (service == registered_services.end()) {
            LOG_DEBUG(Service, "Can't find service: {}", service_name);
            return nullptr;
        }
        auto port = service->second->GetServerPort();
        if (port == nullptr) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(port->hle_handler);
    }

private:
    Core::System& system;
    std::weak_ptr<SRV> srv_interface;

    /// Map of registered services, retrieved using GetServicePort or ConnectToService.
    std::unordered_map<std::string, std::shared_ptr<Kernel::ClientPort>> registered_services;

    // For IPC Recorder
    /// client port Object id -> service name
    std::unordered_map<u32, std::string> registered_services_inverse;

    template <class Archive>
    void save(Archive& ar, const unsigned int file_version) const {
        ar << registered_services;
    }

    template <class Archive>
    void load(Archive& ar, const unsigned int file_version) {
        ar >> registered_services;
        registered_services_inverse.clear();
        for (const auto& pair : registered_services) {
            registered_services_inverse.emplace(pair.second->GetObjectId(), pair.first);
        }
    }

    BOOST_SERIALIZATION_SPLIT_MEMBER()

    friend class boost::serialization::access;
};

} // namespace Service::SM
