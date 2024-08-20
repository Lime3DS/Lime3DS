// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <cstring>
#include <cryptopp/aes.h>
#include <cryptopp/md5.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include "common/assert.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/hle/service/nwm/uds_beacon.h"

namespace Service::NWM {

constexpr u64 DefaultNetworkUptime = 900000000; // 15 minutes in microseconds.

// Note: These values were taken from a packet capture of an o3DS XL
// broadcasting a Super Smash Bros. 4 lobby.
constexpr u16 DefaultExtraCapabilities = 0x0431;

// Size of the SSID broadcast by an UDS beacon frame.
constexpr u8 UDSBeaconSSIDSize = 8;

// The maximum size of the data stored in the EncryptedData0 tag (24).
constexpr u32 EncryptedDataSizeCutoff = 0xFA;

/**
 * NWM Beacon data encryption key, taken from the NWM module code.
 * We stub this with an all-zeros key as that is enough for Lime3DS's purpose.
 * The real key can be used here to generate beacons that will be accepted by
 * a real 3ds.
 */
constexpr std::array<u8, CryptoPP::AES::BLOCKSIZE> nwm_beacon_key = {};

/**
 * Generates a buffer with the fixed parameters of an 802.11 Beacon frame
 * using dummy values.
 * @returns A buffer with the fixed parameters of the beacon frame.
 */
std::vector<u8> GenerateFixedParameters() {
    std::vector<u8> buffer(sizeof(BeaconFrameHeader));

    BeaconFrameHeader header{};
    // Use a fixed default time for now.
    // TODO(Subv): Perhaps use the difference between now and the time the network was started?
    header.timestamp = DefaultNetworkUptime;
    header.beacon_interval = DefaultBeaconInterval;
    header.capabilities = DefaultExtraCapabilities;

    std::memcpy(buffer.data(), &header, sizeof(header));

    return buffer;
}

/**
 * Generates an SSID tag of an 802.11 Beacon frame with an 8-byte all-zero SSID value.
 * @returns A buffer with the SSID tag.
 */
std::vector<u8> GenerateSSIDTag() {
    std::vector<u8> buffer(sizeof(TagHeader) + UDSBeaconSSIDSize);

    TagHeader tag_header{};
    tag_header.tag_id = static_cast<u8>(TagId::SSID);
    tag_header.length = UDSBeaconSSIDSize;

    std::memcpy(buffer.data(), &tag_header, sizeof(TagHeader));

    // The rest of the buffer is already filled with zeros.

    return buffer;
}

/**
 * Generates a buffer with the basic tagged parameters of an 802.11 Beacon frame
 * such as SSID, Rate Information, Country Information, etc.
 * @returns A buffer with the tagged parameters of the beacon frame.
 */
std::vector<u8> GenerateBasicTaggedParameters() {
    // Append the SSID tag
    std::vector<u8> buffer = GenerateSSIDTag();

    // TODO(Subv): Add the SupportedRates tag.
    // TODO(Subv): Add the DSParameterSet tag.
    // TODO(Subv): Add the TrafficIndicationMap tag.
    // TODO(Subv): Add the CountryInformation tag.
    // TODO(Subv): Add the ERPInformation tag.

    return buffer;
}

/**
 * Generates a buffer with the Dummy Nintendo tag.
 * It is currently unknown what this tag does.
 * TODO(Subv): Figure out if this is needed and what it does.
 * @returns A buffer with the Nintendo tagged parameters of the beacon frame.
 */
std::vector<u8> GenerateNintendoDummyTag() {
    // Note: These values were taken from a packet capture of an o3DS XL
    // broadcasting a Super Smash Bros. 4 lobby.
    constexpr std::array<u8, 3> dummy_data = {0x0A, 0x00, 0x00};

    DummyTag tag{};
    tag.header.tag_id = static_cast<u8>(TagId::VendorSpecific);
    tag.header.length = sizeof(DummyTag) - sizeof(TagHeader);
    tag.oui_type = static_cast<u8>(NintendoTagId::Dummy);
    tag.oui = NintendoOUI;
    tag.data = dummy_data;

    std::vector<u8> buffer(sizeof(DummyTag));
    std::memcpy(buffer.data(), &tag, sizeof(DummyTag));

    return buffer;
}

/**
 * Generates a buffer with the Network Info Nintendo tag.
 * This tag contains the network information of the network that is being broadcast.
 * It also contains the application data provided by the application that opened the network.
 * @returns A buffer with the Nintendo network info parameter of the beacon frame.
 */
std::vector<u8> GenerateNintendoNetworkInfoTag(const NetworkInfo& network_info) {
    NetworkInfoTag tag{};
    tag.header.tag_id = static_cast<u8>(TagId::VendorSpecific);
    tag.header.length =
        sizeof(NetworkInfoTag) - sizeof(TagHeader) + network_info.application_data_size;
    tag.appdata_size = network_info.application_data_size;
    // Set the hash to zero initially, it will be updated once we calculate it.
    tag.sha_hash = {};

    // Ensure the network structure has the correct OUI and OUI type.
    ASSERT(network_info.oui_type == static_cast<u8>(NintendoTagId::NetworkInfo));
    ASSERT(network_info.oui_value == NintendoOUI);

    // Ensure the application data size is less than the maximum value.
    ASSERT_MSG(network_info.application_data_size <= ApplicationDataSize, "Data size is too big.");

    // This tag contains the network info structure starting at the OUI.
    std::memcpy(tag.network_info.data(), &network_info.oui_value, tag.network_info.size());

    // Copy the tag and the data so we can calculate the SHA1 over it.
    std::vector<u8> buffer(sizeof(tag) + network_info.application_data_size);
    std::memcpy(buffer.data(), &tag, sizeof(tag));
    std::memcpy(buffer.data() + sizeof(tag), network_info.application_data.data(),
                network_info.application_data_size);

    // Calculate the SHA1 of the contents of the tag.
    std::array<u8, CryptoPP::SHA1::DIGESTSIZE> hash;
    CryptoPP::SHA1().CalculateDigest(hash.data(),
                                     buffer.data() + offsetof(NetworkInfoTag, network_info),
                                     buffer.size() - sizeof(TagHeader));

    // Copy it directly into the buffer, overwriting the zeros that we had previously placed there.
    std::memcpy(buffer.data() + offsetof(NetworkInfoTag, sha_hash), hash.data(), hash.size());

    return buffer;
}

/*
 * Calculates the CTR used for the AES-CTR encryption of the data stored in the
 * EncryptedDataTags.
 * @returns The CTR used for beacon crypto.
 */
std::array<u8, CryptoPP::AES::BLOCKSIZE> GetBeaconCryptoCTR(const NetworkInfo& network_info) {
    BeaconDataCryptoCTR data{};

    data.host_mac = network_info.host_mac_address;
    data.wlan_comm_id = network_info.wlan_comm_id;
    data.id = network_info.id;
    data.network_id = network_info.network_id;

    std::array<u8, CryptoPP::AES::BLOCKSIZE> hash;
    std::memcpy(hash.data(), &data, sizeof(data));

    return hash;
}

/*
 * Serializes the node information into the format needed for network transfer,
 * and then encrypts it with the NWM key.
 * @returns The serialized and encrypted node information.
 */
std::vector<u8> GeneratedEncryptedData(const NetworkInfo& network_info, const NodeList& nodes) {
    std::vector<u8> buffer(sizeof(BeaconData));

    BeaconData data{};
    std::memcpy(buffer.data(), &data, sizeof(BeaconData));

    for (const NodeInfo& node : nodes) {
        // Serialize each node and convert the data from
        // host byte-order to Big Endian.
        BeaconNodeInfo info{};
        info.friend_code_seed = node.friend_code_seed;
        info.network_node_id = node.network_node_id;
        for (std::size_t i = 0; i < info.username.size(); ++i) {
            info.username[i] = node.username[i];
        }

        buffer.insert(buffer.end(), reinterpret_cast<u8*>(&info),
                      reinterpret_cast<u8*>(&info) + sizeof(info));
    }

    // Calculate the MD5 hash of the data in the buffer, not including the hash field.
    std::array<u8, CryptoPP::Weak::MD5::DIGESTSIZE> hash;
    CryptoPP::Weak::MD5().CalculateDigest(hash.data(),
                                          buffer.data() + offsetof(BeaconData, bitmask),
                                          buffer.size() - sizeof(data.md5_hash));

    // Copy the hash into the buffer.
    std::memcpy(buffer.data(), hash.data(), hash.size());

    // Encrypt the data using AES-CTR and the NWM beacon key.
    using CryptoPP::AES;
    std::array<u8, AES::BLOCKSIZE> counter = GetBeaconCryptoCTR(network_info);
    CryptoPP::CTR_Mode<AES>::Encryption aes;
    aes.SetKeyWithIV(nwm_beacon_key.data(), AES::BLOCKSIZE, counter.data());
    aes.ProcessData(buffer.data(), buffer.data(), buffer.size());

    return buffer;
}

void DecryptBeacon(const NetworkInfo& network_info, std::vector<u8>& buffer) {
    // Decrypt the data using AES-CTR and the NWM beacon key.
    using CryptoPP::AES;
    std::array<u8, AES::BLOCKSIZE> counter = GetBeaconCryptoCTR(network_info);
    CryptoPP::CTR_Mode<AES>::Decryption aes;
    aes.SetKeyWithIV(nwm_beacon_key.data(), AES::BLOCKSIZE, counter.data());
    aes.ProcessData(buffer.data(), buffer.data(), buffer.size());
}

/**
 * Generates a buffer with the Network Info Nintendo tag.
 * This tag contains the first portion of the encrypted payload in the 802.11 beacon frame.
 * The encrypted payload contains information about the nodes currently connected to the network.
 * @returns A buffer with the first Nintendo encrypted data parameters of the beacon frame.
 */
std::vector<u8> GenerateNintendoFirstEncryptedDataTag(const NetworkInfo& network_info,
                                                      const NodeList& nodes) {
    const std::size_t payload_size =
        std::min<std::size_t>(EncryptedDataSizeCutoff, nodes.size() * sizeof(NodeInfo));

    EncryptedDataTag tag{};
    tag.header.tag_id = static_cast<u8>(TagId::VendorSpecific);
    tag.header.length = static_cast<u8>(sizeof(tag) - sizeof(TagHeader) + payload_size);
    tag.oui_type = static_cast<u8>(NintendoTagId::EncryptedData0);
    tag.oui = NintendoOUI;

    std::vector<u8> buffer(sizeof(tag) + payload_size);
    std::memcpy(buffer.data(), &tag, sizeof(tag));

    std::vector<u8> encrypted_data = GeneratedEncryptedData(network_info, nodes);
    std::memcpy(buffer.data() + sizeof(tag), encrypted_data.data(), payload_size);

    return buffer;
}

/**
 * Generates a buffer with the Network Info Nintendo tag.
 * This tag contains the second portion of the encrypted payload in the 802.11 beacon frame.
 * The encrypted payload contains information about the nodes currently connected to the network.
 * This tag is only present if the payload size is greater than EncryptedDataSizeCutoff (0xFA)
 * bytes.
 * @returns A buffer with the second Nintendo encrypted data parameters of the beacon frame.
 */
std::vector<u8> GenerateNintendoSecondEncryptedDataTag(const NetworkInfo& network_info,
                                                       const NodeList& nodes) {
    // This tag is only present if the payload is larger than EncryptedDataSizeCutoff (0xFA).
    if (nodes.size() * sizeof(NodeInfo) <= EncryptedDataSizeCutoff)
        return {};

    const std::size_t payload_size = nodes.size() * sizeof(NodeInfo) - EncryptedDataSizeCutoff;

    const std::size_t tag_length = sizeof(EncryptedDataTag) - sizeof(TagHeader) + payload_size;

    // TODO(Subv): What does the 3DS do when a game has too much data to fit into the tag?
    ASSERT_MSG(tag_length <= 255, "Data is too big.");

    EncryptedDataTag tag{};
    tag.header.tag_id = static_cast<u8>(TagId::VendorSpecific);
    tag.header.length = static_cast<u8>(tag_length);
    tag.oui_type = static_cast<u8>(NintendoTagId::EncryptedData1);
    tag.oui = NintendoOUI;

    std::vector<u8> buffer(sizeof(tag) + payload_size);
    std::memcpy(buffer.data(), &tag, sizeof(tag));

    std::vector<u8> encrypted_data = GeneratedEncryptedData(network_info, nodes);
    std::memcpy(buffer.data() + sizeof(tag), encrypted_data.data() + EncryptedDataSizeCutoff,
                payload_size);

    return buffer;
}

/**
 * Generates a buffer with the Nintendo tagged parameters of an 802.11 Beacon frame
 * for UDS communication.
 * @returns A buffer with the Nintendo tagged parameters of the beacon frame.
 */
std::vector<u8> GenerateNintendoTaggedParameters(const NetworkInfo& network_info,
                                                 const NodeList& nodes) {
    ASSERT_MSG(network_info.max_nodes == nodes.size(), "Inconsistent network state.");

    std::vector<u8> buffer = GenerateNintendoDummyTag();
    std::vector<u8> network_info_tag = GenerateNintendoNetworkInfoTag(network_info);
    std::vector<u8> first_data_tag = GenerateNintendoFirstEncryptedDataTag(network_info, nodes);
    std::vector<u8> second_data_tag = GenerateNintendoSecondEncryptedDataTag(network_info, nodes);

    buffer.insert(buffer.end(), network_info_tag.begin(), network_info_tag.end());
    buffer.insert(buffer.end(), first_data_tag.begin(), first_data_tag.end());
    buffer.insert(buffer.end(), second_data_tag.begin(), second_data_tag.end());

    return buffer;
}

std::vector<u8> GenerateBeaconFrame(const NetworkInfo& network_info, const NodeList& nodes) {
    std::vector<u8> buffer = GenerateFixedParameters();
    std::vector<u8> basic_tags = GenerateBasicTaggedParameters();
    std::vector<u8> nintendo_tags = GenerateNintendoTaggedParameters(network_info, nodes);

    buffer.insert(buffer.end(), basic_tags.begin(), basic_tags.end());
    buffer.insert(buffer.end(), nintendo_tags.begin(), nintendo_tags.end());

    return buffer;
}

} // namespace Service::NWM
