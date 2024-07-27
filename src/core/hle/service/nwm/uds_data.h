// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <span>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/nwm/uds_beacon.h"
#include "core/hle/service/service.h"

namespace Service::NWM {

enum class SAP : u8 { SNAPExtensionUsed = 0xAA };

enum class PDUControl : u8 { UnnumberedInformation = 3 };

enum class EtherType : u16 { SecureData = 0x876D, EAPoL = 0x888E };

/*
 * 802.2 header, UDS packets always use SNAP for these headers,
 * which means the dsap and ssap are always SNAPExtensionUsed (0xAA)
 * and the OUI is always 0.
 */
struct LLCHeader {
    SAP dsap = SAP::SNAPExtensionUsed;
    SAP ssap = SAP::SNAPExtensionUsed;
    PDUControl control = PDUControl::UnnumberedInformation;
    std::array<u8, 3> OUI = {};
    enum_be<EtherType> protocol;
};

static_assert(sizeof(LLCHeader) == 8, "LLCHeader has the wrong size");

/*
 * Nintendo SecureData header, every UDS packet contains one,
 * it is used to store metadata about the transmission such as
 * the source and destination network node ids.
 */
struct SecureDataHeader {
    // TODO(Subv): It is likely that the first 4 bytes of this header are
    // actually part of another container protocol.
    u16_be protocol_size;
    INSERT_PADDING_BYTES(2);
    u16_be securedata_size;
    u8 is_management;
    u8 data_channel;
    u16_be sequence_number;
    u16_be dest_node_id;
    u16_be src_node_id;

    u32 GetActualDataSize() const {
        return protocol_size - sizeof(SecureDataHeader);
    }
};

static_assert(sizeof(SecureDataHeader) == 14, "SecureDataHeader has the wrong size");

/*
 * The raw bytes of this structure are the CTR used in the encryption (AES-CTR)
 * process used to generate the CCMP key for data frame encryption.
 */
struct DataFrameCryptoCTR {
    u32_le wlan_comm_id;
    u32_le network_id;
    std::array<u8, 6> host_mac;
    u16_le id;
};

static_assert(sizeof(DataFrameCryptoCTR) == 16, "DataFrameCryptoCTR has the wrong size");

struct EAPoLNodeInfo {
    u64_be friend_code_seed;
    std::array<u16_be, 10> username;
    INSERT_PADDING_BYTES(4);
    u16_be network_node_id;
    INSERT_PADDING_BYTES(6);
};

static_assert(sizeof(EAPoLNodeInfo) == 0x28, "EAPoLNodeInfo has the wrong size");

constexpr u16 EAPoLStartMagic = 0x201;

/*
 * Nintendo EAPoLStartPacket, is used to initaliaze a connection between client and host
 */
struct EAPoLStartPacket {
    u16_be magic = EAPoLStartMagic;
    u16_be association_id;
    // This value is hardcoded to 1 in the NWM module.
    u16_be unknown = 1;
    INSERT_PADDING_BYTES(2);
    EAPoLNodeInfo node;
};

static_assert(sizeof(EAPoLStartPacket) == 0x30, "EAPoLStartPacket has the wrong size");

constexpr u16 EAPoLLogoffMagic = 0x202;

struct EAPoLLogoffPacket {
    u16_be magic = EAPoLLogoffMagic;
    INSERT_PADDING_BYTES(2);
    u16_be assigned_node_id;
    MacAddress client_mac_address;
    INSERT_PADDING_BYTES(6);
    u8 connected_nodes;
    u8 max_nodes;
    INSERT_PADDING_BYTES(4);

    std::array<EAPoLNodeInfo, UDSMaxNodes> nodes;
};

static_assert(sizeof(EAPoLLogoffPacket) == 0x298, "EAPoLLogoffPacket has the wrong size");

/**
 * Generates an unencrypted 802.11 data payload.
 * @returns The generated frame payload.
 */
std::vector<u8> GenerateDataPayload(std::span<const u8> data, u8 channel, u16 dest_node,
                                    u16 src_node, u16 sequence_number);

/*
 * Returns the SecureDataHeader stored in an 802.11 data frame.
 */
SecureDataHeader ParseSecureDataHeader(std::span<const u8> data);

/*
 * Generates an unencrypted 802.11 data frame body with the EAPoL-Start format for UDS
 * communication.
 * @returns The generated frame body.
 */
std::vector<u8> GenerateEAPoLStartFrame(u16 association_id, const NodeInfo& node_info);

/*
 * Returns the EtherType of the specified 802.11 frame.
 */
EtherType GetFrameEtherType(std::span<const u8> frame);

/*
 * Returns the EAPoL type (Start / Logoff) of the specified 802.11 frame.
 * Note: The frame *must* be an EAPoL frame.
 */
u16 GetEAPoLFrameType(std::span<const u8> frame);

/*
 * Returns a deserialized NodeInfo structure from the information inside an EAPoL-Start packet
 * encapsulated in an 802.11 data frame.
 */
NodeInfo DeserializeNodeInfoFromFrame(std::span<const u8> frame);

/*
 * Returns a NodeInfo constructed from the data in the specified EAPoLNodeInfo.
 */
NodeInfo DeserializeNodeInfo(const EAPoLNodeInfo& node);

/*
 * Generates an unencrypted 802.11 data frame body with the EAPoL-Logoff format for UDS
 * communication.
 * @returns The generated frame body.
 */
std::vector<u8> GenerateEAPoLLogoffFrame(const MacAddress& mac_address, u16 network_node_id,
                                         const NodeList& nodes, u8 max_nodes, u8 total_nodes);

/*
 * Returns a EAPoLLogoffPacket representing the specified 802.11-encapsulated data frame.
 */
EAPoLLogoffPacket ParseEAPoLLogoffFrame(std::span<const u8> frame);

} // namespace Service::NWM
