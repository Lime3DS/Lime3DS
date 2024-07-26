// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/hle/service/nwm/uds_connection.h"

namespace Service::NWM {

// Note: These values were taken from a packet capture of an o3DS XL
// broadcasting a Super Smash Bros. 4 lobby.
constexpr u16 DefaultExtraCapabilities = 0x0431;

std::vector<u8> GenerateAuthenticationFrame(AuthenticationSeq seq) {
    AuthenticationFrame frame{};
    frame.auth_seq = seq;

    std::vector<u8> data(sizeof(frame));
    std::memcpy(data.data(), &frame, sizeof(frame));

    return data;
}

AuthenticationSeq GetAuthenticationSeqNumber(std::span<const u8> body) {
    AuthenticationFrame frame;
    std::memcpy(&frame, body.data(), sizeof(frame));

    return frame.auth_seq;
}

/**
 * Generates an SSID tag of an 802.11 Beacon frame with an 8-byte character representation of the
 * specified network id as the SSID value.
 * @param network_id The network id to use.
 * @returns A buffer with the SSID tag.
 */
static std::vector<u8> GenerateSSIDTag(u32 network_id) {
    constexpr u8 SSIDSize = 8;

    struct {
        u8 id = static_cast<u8>(TagId::SSID);
        u8 size = SSIDSize;
    } tag_header;

    std::vector<u8> buffer(sizeof(tag_header) + SSIDSize);

    std::memcpy(buffer.data(), &tag_header, sizeof(tag_header));

    std::string network_name = fmt::format("{0:08X}", network_id);

    std::memcpy(buffer.data() + sizeof(tag_header), network_name.c_str(), SSIDSize);

    return buffer;
}

std::vector<u8> GenerateAssocResponseFrame(AssocStatus status, u16 association_id, u32 network_id) {
    AssociationResponseFrame frame{};
    frame.capabilities = DefaultExtraCapabilities;
    frame.status_code = status;
    // The association id is ORed with this magic value (0xC000)
    constexpr u16 AssociationIdMagic = 0xC000;
    frame.assoc_id = association_id | AssociationIdMagic;

    std::vector<u8> data(sizeof(frame));
    std::memcpy(data.data(), &frame, sizeof(frame));

    auto ssid_tag = GenerateSSIDTag(network_id);
    data.insert(data.end(), ssid_tag.begin(), ssid_tag.end());

    // TODO(Subv): Add the SupportedRates tag.
    // TODO(Subv): Add the DSParameterSet tag.
    // TODO(Subv): Add the ERPInformation tag.
    return data;
}

std::tuple<AssocStatus, u16> GetAssociationResult(std::span<const u8> body) {
    AssociationResponseFrame frame;
    std::memcpy(&frame, body.data(), sizeof(frame));

    constexpr u16 AssociationIdMask = 0x3FFF;
    return std::make_tuple(frame.status_code, frame.assoc_id & AssociationIdMask);
}

} // namespace Service::NWM
