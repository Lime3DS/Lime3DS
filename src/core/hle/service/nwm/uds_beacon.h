// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"

namespace Service::NWM {

using MacAddress = std::array<u8, 6>;
constexpr std::array<u8, 3> NintendoOUI = {0x00, 0x1F, 0x32};

/**
 * Internal vendor-specific tag ids as stored inside
 * VendorSpecific blocks in the Beacon frames.
 */
enum class NintendoTagId : u8 {
    Dummy = 20,
    NetworkInfo = 21,
    EncryptedData0 = 24,
    EncryptedData1 = 25,
};

struct BeaconEntryHeader {
    u32_le total_size;
    INSERT_PADDING_BYTES(1);
    u8 wifi_channel;
    INSERT_PADDING_BYTES(2);
    MacAddress mac_address;
    INSERT_PADDING_BYTES(6);
    u32_le unk_size;
    u32_le header_size;
};

static_assert(sizeof(BeaconEntryHeader) == 0x1C, "BeaconEntryHeader has incorrect size.");

struct BeaconDataReplyHeader {
    u32_le max_output_size;
    u32_le total_size;
    u32_le total_entries;
};

static_assert(sizeof(BeaconDataReplyHeader) == 12, "BeaconDataReplyHeader has incorrect size.");

#pragma pack(push, 1)
struct BeaconFrameHeader {
    // Number of microseconds the AP has been active.
    u64_le timestamp;
    // Interval between beacon transmissions, expressed in TU.
    u16_le beacon_interval;
    // Indicates the presence of optional capabilities.
    u16_le capabilities;
};
#pragma pack(pop)

static_assert(sizeof(BeaconFrameHeader) == 12, "BeaconFrameHeader has incorrect size.");

struct TagHeader {
    u8 tag_id;
    u8 length;
};

static_assert(sizeof(TagHeader) == 2, "TagHeader has incorrect size.");

struct DummyTag {
    TagHeader header;
    std::array<u8, 3> oui;
    u8 oui_type;
    std::array<u8, 3> data;
};

static_assert(sizeof(DummyTag) == 9, "DummyTag has incorrect size.");

struct NetworkInfoTag {
    TagHeader header;
    std::array<u8, 0x1F> network_info;
    std::array<u8, 0x14> sha_hash;
    u8 appdata_size;
};

static_assert(sizeof(NetworkInfoTag) == 54, "NetworkInfoTag has incorrect size.");

struct EncryptedDataTag {
    TagHeader header;
    std::array<u8, 3> oui;
    u8 oui_type;
};

static_assert(sizeof(EncryptedDataTag) == 6, "EncryptedDataTag has incorrect size.");

#pragma pack(push, 1)
// The raw bytes of this structure are the CTR used in the encryption (AES-CTR)
// of the beacon data stored in the EncryptedDataTags.
struct BeaconDataCryptoCTR {
    MacAddress host_mac;
    u32_le wlan_comm_id;
    u8 id;
    INSERT_PADDING_BYTES(1);
    u32_le network_id;
};

static_assert(sizeof(BeaconDataCryptoCTR) == 0x10, "BeaconDataCryptoCTR has incorrect size.");

struct BeaconNodeInfo {
    u64_be friend_code_seed;
    std::array<u16_be, 10> username;
    u16_be network_node_id;
};

static_assert(sizeof(BeaconNodeInfo) == 0x1E, "BeaconNodeInfo has incorrect size.");

struct BeaconData {
    std::array<u8, 0x10> md5_hash;
    u16_be bitmask;
};
#pragma pack(pop)

static_assert(sizeof(BeaconData) == 0x12, "BeaconData has incorrect size.");

/**
 * Decrypts the beacon data buffer for the network described by `network_info`.
 */
void DecryptBeacon(const NetworkInfo& network_info, std::vector<u8>& buffer);

/**
 * Generates an 802.11 beacon frame starting at the management frame header.
 * This frame contains information about the network and its connected clients.
 * @returns The generated frame.
 */
std::vector<u8> GenerateBeaconFrame(const NetworkInfo& network_info, const NodeList& nodes);

} // namespace Service::NWM
