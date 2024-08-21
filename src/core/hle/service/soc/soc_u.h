// Copyright Citra Emulator Project / Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <utility>
#include <boost/serialization/set.hpp>
#include <boost/serialization/unordered_map.hpp>
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace IPC {
class RequestParser;
}

namespace Service::SOC {

/// Holds information about a particular socket
struct SocketHolder {
#ifdef _WIN32
    using SOCKET = unsigned long long;
#else
    using SOCKET = int;
#endif // _WIN32

    SOCKET socket_fd;     ///< The socket descriptor
    bool blocking = true; ///< Whether the socket is blocking or not.
    bool isGlobal = false;
    bool shutdown_rd = false;

    u32 ownerProcess = 0;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar & socket_fd;
        ar & blocking;
        ar & isGlobal;
        ar & ownerProcess;
    }
    friend class boost::serialization::access;
};

class SOC_U final : public ServiceFramework<SOC_U> {
public:
    SOC_U();
    ~SOC_U();

    struct InterfaceInfo {
        u32 address;
        u32 netmask;
        u32 broadcast;
    };

    // Gets the interface info that is able to reach the internet.
    std::optional<InterfaceInfo> GetDefaultInterfaceInfo();

private:
    static constexpr Result ResultWrongProcess =
        Result(4, ErrorModule::SOC, ErrorSummary::InvalidState, ErrorLevel::Status);
    static constexpr Result ResultNotInitialized =
        Result(6, ErrorModule::SOC, ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    static constexpr Result ResultInvalidSocketDescriptor =
        Result(7, ErrorModule::SOC, ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    static constexpr Result ResultAlreadyInitialized =
        Result(11, ErrorModule::SOC, ErrorSummary::InvalidState, ErrorLevel::Status);

    static constexpr u32 SOC_ERR_INAVLID_ENUM_VALUE = 0xFFFF8025;

    static constexpr u32 SOC_SOL_IP = 0x0000;
    static constexpr u32 SOC_SOL_TCP = 0x0006;
    static constexpr u32 SOC_SOL_CONFIG = 0xFFFE;
    static constexpr u32 SOC_SOL_SOCKET = 0xFFFF;

    static constexpr int SOC_TTL_DEFAULT = 64;

    static const std::unordered_map<u64, std::pair<int, int>> sockopt_map;
    static std::pair<int, int> TranslateSockOpt(int level, int opt);
    static void TranslateSockOptDataToPlatform(std::vector<u8>& out, const std::vector<u8>& in,
                                               int platform_level, int platform_opt);
    bool GetSocketBlocking(const SocketHolder& socket_holder);
    u32 SetSocketBlocking(SocketHolder& socket_holder, bool blocking);

    std::optional<std::reference_wrapper<SocketHolder>> GetSocketHolder(u32 ctr_socket_fd,
                                                                        u32 process_id,
                                                                        IPC::RequestParser& rp);

    // Close and delete all sockets, optinally only the ones owned by the
    // specified process ID.
    void CloseAndDeleteAllSockets(s32 process_id = -1);

    s32 SendToImpl(SocketHolder& holder, u32 len, u32 flags, u32 addr_len,
                   const std::vector<u8>& input_buff, const u8* dest_addr_buff);

    static void RecvBusyWaitForEvent(SocketHolder& holder);

    // From
    // https://github.com/devkitPro/libctru/blob/1de86ea38aec419744149daf692556e187d4678a/libctru/include/3ds/services/soc.h#L15
    enum class NetworkOpt {
        NETOPT_MAC_ADDRESS = 0x1004,     ///< The mac address of the interface
        NETOPT_ARP_TABLE = 0x3002,       ///< The ARP table
        NETOPT_IP_INFO = 0x4003,         ///< The current IP setup
        NETOPT_IP_MTU = 0x4004,          ///< The value of the IP MTU
        NETOPT_ROUTING_TABLE = 0x4006,   ///< The routing table
        NETOPT_UDP_NUMBER = 0x8002,      ///< The number of sockets in the UDP table
        NETOPT_UDP_TABLE = 0x8003,       ///< The table of opened UDP sockets
        NETOPT_TCP_NUMBER = 0x9002,      ///< The number of sockets in the TCP table
        NETOPT_TCP_TABLE = 0x9003,       ///< The table of opened TCP sockets
        NETOPT_DNS_TABLE = 0xB003,       ///< The table of the DNS servers
        NETOPT_DHCP_LEASE_TIME = 0xC001, ///< The DHCP lease time remaining, in seconds
    };

    void Socket(Kernel::HLERequestContext& ctx);
    void Bind(Kernel::HLERequestContext& ctx);
    void Fcntl(Kernel::HLERequestContext& ctx);
    void Listen(Kernel::HLERequestContext& ctx);
    void Accept(Kernel::HLERequestContext& ctx);
    void SockAtMark(Kernel::HLERequestContext& ctx);
    void GetHostId(Kernel::HLERequestContext& ctx);
    void Close(Kernel::HLERequestContext& ctx);
    void SendToOther(Kernel::HLERequestContext& ctx);
    void SendToSingle(Kernel::HLERequestContext& ctx);
    void RecvFromOther(Kernel::HLERequestContext& ctx);
    void RecvFrom(Kernel::HLERequestContext& ctx);
    void Poll(Kernel::HLERequestContext& ctx);
    void GetSockName(Kernel::HLERequestContext& ctx);
    void Shutdown(Kernel::HLERequestContext& ctx);
    void GetHostByAddr(Kernel::HLERequestContext& ctx);
    void GetHostByName(Kernel::HLERequestContext& ctx);
    void GetPeerName(Kernel::HLERequestContext& ctx);
    void Connect(Kernel::HLERequestContext& ctx);
    void InitializeSockets(Kernel::HLERequestContext& ctx);
    void ShutdownSockets(Kernel::HLERequestContext& ctx);
    void GetSockOpt(Kernel::HLERequestContext& ctx);
    void SetSockOpt(Kernel::HLERequestContext& ctx);
    void GetNetworkOpt(Kernel::HLERequestContext& ctx);
    void SendToMultiple(Kernel::HLERequestContext& ctx);
    void CloseSockets(Kernel::HLERequestContext& ctx);
    void AddGlobalSocket(Kernel::HLERequestContext& ctx);

    // Some platforms seem to have GetAddrInfo and GetNameInfo defined as macros,
    // so we have to use a different name here.
    void GetAddrInfoImpl(Kernel::HLERequestContext& ctx);
    void GetNameInfoImpl(Kernel::HLERequestContext& ctx);

    // Socked ids
    u32 next_socket_id = 3;
    u32 GetNextSocketID() {
        return next_socket_id++;
    }

    /// Holds info about the currently open sockets
    friend struct CTRPollFD;
    std::unordered_map<u32, SocketHolder> created_sockets;
    std::set<u32> initialized_processes;

    /// Cache interface info for the current session
    /// These two fields are not saved to savestates on purpose
    /// as network interfaces may change and it's better to.
    /// obtain them again between play sessions.
    bool interface_info_cached = false;
    InterfaceInfo interface_info;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar & created_sockets;
        ar & initialized_processes;
    }
    friend class boost::serialization::access;
};

std::shared_ptr<SOC_U> GetService(Core::System& system);

void InstallInterfaces(Core::System& system);

} // namespace Service::SOC

BOOST_CLASS_EXPORT_KEY(Service::SOC::SOC_U)
