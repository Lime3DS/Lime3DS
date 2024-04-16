// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <boost/optional.hpp>
#include <boost/serialization/export.hpp>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"
#include "network/network.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
class SharedMemory;
} // namespace Kernel

// Local-WLAN service

namespace Service::NWM {

using MacAddress = std::array<u8, 6>;

const std::size_t ApplicationDataSize = 0xC8;
const u8 DefaultNetworkChannel = 11;

// Number of milliseconds in a TU.
const double MillisecondsPerTU = 1.024;
// Interval measured in TU, the default value is 100TU = 102.4ms
const u16 DefaultBeaconInterval = 100;

/// The maximum number of nodes that can exist in an UDS session.
constexpr u32 UDSMaxNodes = 16;

struct NodeInfo {
    u64_le friend_code_seed;
    std::array<u16_le, 10> username;
    INSERT_PADDING_BYTES(4);
    u16_le network_node_id;
    INSERT_PADDING_BYTES(6);

    void Reset() {
        friend_code_seed = 0;
        username.fill(0);
        network_node_id = 0;
    }
};

static_assert(sizeof(NodeInfo) == 40, "NodeInfo has incorrect size.");

using NodeList = std::vector<NodeInfo>;

enum class NetworkStatus : u32 {
    NotConnected = 3,
    ConnectedAsHost = 6,
    Connecting = 7,
    ConnectedAsClient = 9,
    ConnectedAsSpectator = 10,
};

enum class NetworkStatusChangeReason : u32 {
    None = 0,
    ConnectionEstablished = 1,
    ConnectionLost = 4,
};

struct ConnectionStatus {
    enum_le<NetworkStatus> status;
    enum_le<NetworkStatusChangeReason> status_change_reason;
    u16_le network_node_id;
    u16_le changed_nodes;
    u16_le nodes[UDSMaxNodes];
    u8 total_nodes;
    u8 max_nodes;
    u16_le node_bitmask;
};

static_assert(sizeof(ConnectionStatus) == 0x30, "ConnectionStatus has incorrect size.");

struct NetworkInfo {
    std::array<u8, 6> host_mac_address;
    u8 channel;
    INSERT_PADDING_BYTES(1);
    u8 initialized;
    INSERT_PADDING_BYTES(3);
    std::array<u8, 3> oui_value;
    u8 oui_type;
    // This field is received as BigEndian from the game.
    u32_be wlan_comm_id;
    u8 id;
    INSERT_PADDING_BYTES(1);
    u16_be attributes;
    u32_be network_id;
    u8 total_nodes;
    u8 max_nodes;
    INSERT_PADDING_BYTES(2);
    INSERT_PADDING_BYTES(0x1F);
    u8 application_data_size;
    std::array<u8, ApplicationDataSize> application_data;
};

static_assert(offsetof(NetworkInfo, oui_value) == 0xC, "oui_value is at the wrong offset.");
static_assert(offsetof(NetworkInfo, wlan_comm_id) == 0x10, "wlancommid is at the wrong offset.");
static_assert(sizeof(NetworkInfo) == 0x108, "NetworkInfo has incorrect size.");

/// Additional block tag ids in the Beacon and Association Response frames
enum class TagId : u8 {
    SSID = 0,
    SupportedRates = 1,
    DSParameterSet = 2,
    TrafficIndicationMap = 5,
    CountryInformation = 7,
    ERPInformation = 42,
    VendorSpecific = 221
};

class NWM_UDS final : public ServiceFramework<NWM_UDS> {
public:
    explicit NWM_UDS(Core::System& system);
    ~NWM_UDS();

    class ThreadCallback;

private:
    Core::System& system;

    void UpdateNetworkAttribute(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::Shutdown service function
     *  Inputs:
     *      1 : None
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Shutdown(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::DestroyNetwork service function.
     * Closes the network that we're currently hosting.
     *  Inputs:
     *      0 : Command header.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DestroyNetwork(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::DisconnectNetwork service function.
     * This disconnects this device from the network.
     *  Inputs:
     *      0 : Command header.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void DisconnectNetwork(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::GetConnectionStatus service function.
     * Returns the connection status structure for the currently open network connection.
     * This structure contains information about the connection,
     * like the number of connected nodes, etc.
     *  Inputs:
     *      0 : Command header.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-13 : Channel of the current WiFi network connection.
     */
    void GetConnectionStatus(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::GetNodeInformation service function.
     * Returns the node inforamtion structure for the currently connected node.
     *  Inputs:
     *      0 : Command header.
     *      1 : Node ID.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2-11 : NodeInfo structure.
     */
    void GetNodeInformation(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::RecvBeaconBroadcastData service function
     * Returns the raw beacon data for nearby networks that match the supplied WlanCommId.
     *  Inputs:
     *      1 : Output buffer max size
     *    2-3 : Unknown
     *    4-5 : Host MAC address.
     *   6-14 : Unused
     *     15 : WLan Comm Id
     *     16 : Id
     *     17 : Value 0
     *     18 : Input handle
     *     19 : (Size<<4) | 12
     *     20 : Output buffer ptr
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2, 3: output buffer return descriptor & ptr
     */
    void RecvBeaconBroadcastData(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::SetApplicationData service function.
     * Updates the application data that is being broadcast in the beacon frames
     * for the network that we're hosting.
     *  Inputs:
     *      1 : Data size.
     *      3 : VAddr of the data.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Channel of the current WiFi network connection.
     */
    void SetApplicationData(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::GetApplicationData service function.
     * Loads the application data from the current beacon.
     *  Inputs:
     *      1 : Data size.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, always 0
     *      2 : Actual data size
     */
    void GetApplicationData(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::Bind service function.
     * Binds a BindNodeId to a data channel and retrieves a data event.
     *  Inputs:
     *      1 : BindNodeId
     *      2 : Receive buffer size.
     *      3 : u8 Data channel to bind to.
     *      4 : Network node id.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Copy handle descriptor.
     *      3 : Data available event handle.
     */
    void Bind(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::Unbind service function.
     * Unbinds a BindNodeId from a data channel.
     *  Inputs:
     *      1 : BindNodeId
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Unbind(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::PullPacket service function.
     * Receives a data frame from the specified bind node id
     *  Inputs:
     *      0 : Command header.
     *      1 : Bind node id.
     *      2 : Max out buff size >> 2.
     *      3 : Max out buff size.
     *     64 : Output buffer descriptor
     *     65 : Output buffer address
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Received data size
     *      3 : u16 Source network node id
     *      4 : Buffer descriptor
     *      5 : Buffer address
     */
    void PullPacket(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::SendTo service function.
     * Sends a data frame to the UDS network we're connected to.
     *  Inputs:
     *      0 : Command header.
     *      1 : Unknown.
     *      2 : u16 Destination network node id.
     *      3 : u8 Data channel.
     *      4 : Buffer size >> 2
     *      5 : Data size
     *      6 : Flags
     *      7 : Input buffer descriptor
     *      8 : Input buffer address
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SendTo(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::GetChannel service function.
     * Returns the WiFi channel in which the network we're connected to is transmitting.
     *  Inputs:
     *      0 : Command header.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Channel of the current WiFi network connection.
     */
    void GetChannel(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::Initialize service function
     *  Inputs:
     *      1 : Shared memory size
     *   2-11 : Input NodeInfo Structure
     *     12 : 2-byte Version
     *     13 : Value 0
     *     14 : Shared memory handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Value 0
     *      3 : Output event handle
     */
    void InitializeWithVersion(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::InitializeDeprecated service function
     *  Inputs:
     *      1 : Shared memory size
     *   2-11 : Input NodeInfo Structure
     *     13 : Value 0
     *     14 : Shared memory handle
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Value 0
     *      3 : Output event handle
     */
    void InitializeDeprecated(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::BeginHostingNetwork service function.
     * Creates a network and starts broadcasting its presence.
     *  Inputs:
     *      1 : Passphrase buffer size.
     *      3 : VAddr of the NetworkInfo structure.
     *      5 : VAddr of the passphrase.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void BeginHostingNetwork(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::BeginHostingNetworkDeprecated service function.
     * Creates a network and starts broadcasting its presence.
     *  Inputs:
     *      1 - 15 : the NetworkInfo structure, excluding application data
     *      16 : passphrase size
     *      18 : VAddr of the passphrase.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void BeginHostingNetworkDeprecated(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::ConnectToNetwork service function.
     * This connects to the specified network
     *  Inputs:
     *      0 : Command header
     *      1 : Connection type: 0x1 = Client, 0x2 = Spectator.
     *      2 : Passphrase buffer size
     *      3 : (NetworkStructSize<<12) | 0x402
     *      4 : Network struct buffer ptr
     *      5 : (PassphraseSize<<12) | 2
     *      6 : Input passphrase buffer ptr
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ConnectToNetwork(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::ConnectToNetwork Deprecatedservice function.
     * This connects to the specified network
     *  Inputs:
     *      0 : Command header
     *      1 - 15 : the NetworkInfo structure, excluding application data
     *      16 : Connection type: 0x1 = Client, 0x2 = Spectator.
     *      17 : Passphrase buffer size
     *      18 : (PassphraseSize<<12) | 2
     *      19 : Input passphrase buffer ptr
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ConnectToNetworkDeprecated(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::EjectClient Disconnects clients.
     *  Inputs:
     *      0 : Command header
     *      1 : Network node id
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void EjectClient(Kernel::HLERequestContext& ctx);

    /**
     * NWM_UDS::DecryptBeaconData service function.
     * Decrypts the encrypted data tags contained in the 802.11 beacons.
     *  Inputs:
     *      1 : Input network struct buffer descriptor.
     *      2 : Input network struct buffer ptr.
     *      3 : Input tag0 encrypted buffer descriptor.
     *      4 : Input tag0 encrypted buffer ptr.
     *      5 : Input tag1 encrypted buffer descriptor.
     *      6 : Input tag1 encrypted buffer ptr.
     *     64 : Output buffer descriptor.
     *     65 : Output buffer ptr.
     *  Outputs:
     *      0 : Return header
     *      1 : Result of function, 0 on success, otherwise error code
     *      2, 3: output buffer return descriptor & ptr
     */
    void DecryptBeaconData(Kernel::HLERequestContext& ctx);

    ResultVal<std::shared_ptr<Kernel::Event>> Initialize(
        u32 sharedmem_size, const NodeInfo& node, u16 version,
        std::shared_ptr<Kernel::SharedMemory> sharedmem);

    Result BeginHostingNetwork(std::span<const u8> network_info_buffer, std::vector<u8> passphrase);

    void ConnectToNetwork(Kernel::HLERequestContext& ctx, u16 command_id,
                          std::span<const u8> network_info_buffer, u8 connection_type,
                          std::vector<u8> passphrase);

    void BeaconBroadcastCallback(std::uintptr_t user_data, s64 cycles_late);

    /**
     * Returns a list of received 802.11 beacon frames from the specified sender since the last
     * call.
     */
    std::list<Network::WifiPacket> GetReceivedBeacons(const MacAddress& sender);

    /*
     * Returns an available index in the nodes array for the
     * currently-hosted UDS network.
     */
    u16 GetNextAvailableNodeId();

    void BroadcastNodeMap();
    void HandleNodeMapPacket(const Network::WifiPacket& packet);
    void HandleBeaconFrame(const Network::WifiPacket& packet);
    void HandleAssociationResponseFrame(const Network::WifiPacket& packet);
    void HandleEAPoLPacket(const Network::WifiPacket& packet);
    void HandleSecureDataPacket(const Network::WifiPacket& packet);

    /*
     * Start a connection sequence with an UDS server. The sequence starts by sending an 802.11
     * authentication frame with SEQ1.
     */
    void StartConnectionSequence(const MacAddress& server);

    /// Sends an Association Response frame to the specified mac address
    void SendAssociationResponseFrame(const MacAddress& address);

    /*
     * Handles the authentication request frame and sends the authentication response and
     * association response frames. Once an Authentication frame with SEQ1 is received by the
     * server, it responds with an Authentication frame containing SEQ2, and immediately sends an
     * Association response frame containing the details of the access point and the assigned
     * association id for the new client.
     */
    void HandleAuthenticationFrame(const Network::WifiPacket& packet);

    /// Handles the deauthentication frames sent from clients to hosts, when they leave a session
    void HandleDeauthenticationFrame(const Network::WifiPacket& packet);

    void HandleDataFrame(const Network::WifiPacket& packet);

    /// Callback to parse and handle a received wifi packet.
    void OnWifiPacketReceived(const Network::WifiPacket& packet);

    boost::optional<Network::MacAddress> GetNodeMacAddress(u16 dest_node_id, u8 flags);

    // Event that is signaled every time the connection status changes.
    std::shared_ptr<Kernel::Event> connection_status_event;

    // Shared memory provided by the application to store the receive buffer.
    // This is not currently used.
    std::shared_ptr<Kernel::SharedMemory> recv_buffer_memory;

    // Connection status of this 3DS.
    ConnectionStatus connection_status{};

    std::atomic<bool> initialized{false};

    /* Node information about the current network.
     * The amount of elements in this vector is always the maximum number
     * of nodes specified in the network configuration.
     * The first node is always the host.
     */
    NodeList node_info;

    // Node information about our own system.
    NodeInfo current_node;

    struct BindNodeData {
        u32 bind_node_id;    ///< Id of the bind node associated with this data.
        u8 channel;          ///< Channel that this bind node was bound to.
        u16 network_node_id; ///< Node id this bind node is associated with, only packets from this
                             /// network node will be received.
        std::shared_ptr<Kernel::Event> event;         ///< Receive event for this bind node.
        std::deque<std::vector<u8>> received_packets; ///< List of packets received on this channel.
    };

    // Mapping of data channels to their internal data.
    std::unordered_map<u32, BindNodeData> channel_data;

    // The WiFi network channel that the network is currently on.
    // Since we're not actually interacting with physical radio waves, this is just a dummy value.
    u8 network_channel = DefaultNetworkChannel;

    // Information about the network that we're currently connected to.
    NetworkInfo network_info;

    // Mapping of mac addresses to their respective node_ids.
    struct Node {
        bool connected;
        u16 node_id;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar & connected;
            ar & node_id;
        }
        friend class boost::serialization::access;
    };

    std::map<MacAddress, Node> node_map;

    // Event that will generate and send the 802.11 beacon frames.
    Core::TimingEventType* beacon_broadcast_event;

    // Callback identifier for the OnWifiPacketReceived event.
    Network::RoomMember::CallbackHandle<Network::WifiPacket> wifi_packet_received;

    // Mutex to synchronize access to the connection status between the emulation thread and the
    // network thread.
    std::mutex connection_status_mutex;

    std::shared_ptr<Kernel::Event> connection_event;

    // Mutex to synchronize access to the list of received beacons between the emulation thread and
    // the network thread.
    std::mutex beacon_mutex;

    // List of the last <MaxBeaconFrames> beacons received from the network.
    std::list<Network::WifiPacket> received_beacons;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

} // namespace Service::NWM

SERVICE_CONSTRUCT(Service::NWM::NWM_UDS)
BOOST_CLASS_EXPORT_KEY(Service::NWM::NWM_UDS)
BOOST_CLASS_EXPORT_KEY(Service::NWM::NWM_UDS::ThreadCallback)
