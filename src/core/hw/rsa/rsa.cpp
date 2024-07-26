// Copyright 2020 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sstream>
#include <cryptopp/hex.h>
#include <cryptopp/integer.h>
#include <cryptopp/nbtheory.h>
#include <cryptopp/sha.h>
#include <fmt/format.h>
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/hw/rsa/rsa.h"

namespace HW::RSA {

namespace {
std::vector<u8> HexToBytes(const std::string& hex) {
    std::vector<u8> bytes;

    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        u8 byte = static_cast<u8>(std::strtol(byteString.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
};
} // namespace

constexpr std::size_t SlotSize = 4;
std::array<RsaSlot, SlotSize> rsa_slots;

std::vector<u8> RsaSlot::GetSignature(std::span<const u8> message) const {
    CryptoPP::Integer sig =
        CryptoPP::ModularExponentiation(CryptoPP::Integer(message.data(), message.size()),
                                        CryptoPP::Integer(exponent.data(), exponent.size()),
                                        CryptoPP::Integer(modulus.data(), modulus.size()));
    std::stringstream ss;
    ss << std::hex << sig;
    CryptoPP::HexDecoder decoder;
    decoder.Put(reinterpret_cast<unsigned char*>(ss.str().data()), ss.str().size());
    decoder.MessageEnd();
    std::vector<u8> result(decoder.MaxRetrievable());
    decoder.Get(result.data(), result.size());
    return HexToBytes(ss.str());
}

void InitSlots() {
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    const std::string filepath = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + BOOTROM9;
    FileUtil::IOFile file(filepath, "rb");
    if (!file) {
        return;
    }

    const std::size_t length = file.GetSize();
    if (length != 65536) {
        LOG_ERROR(HW_AES, "Bootrom9 size is wrong: {}", length);
        return;
    }

    constexpr std::size_t RSA_MODULUS_POS = 0xB3E0;
    file.Seek(RSA_MODULUS_POS, SEEK_SET);
    std::vector<u8> modulus(256);
    file.ReadArray(modulus.data(), modulus.size());

    constexpr std::size_t RSA_EXPONENT_POS = 0xB4E0;
    file.Seek(RSA_EXPONENT_POS, SEEK_SET);
    std::vector<u8> exponent(256);
    file.ReadArray(exponent.data(), exponent.size());

    rsa_slots[0] = RsaSlot(std::move(exponent), std::move(modulus));
    // TODO(B3N30): Initalize the other slots. But since they aren't used at all, we can skip them
    // for now
}

RsaSlot GetSlot(std::size_t slot_id) {
    if (slot_id >= rsa_slots.size())
        return RsaSlot{};
    return rsa_slots[slot_id];
}

std::vector<u8> CreateASN1Message(std::span<const u8> data) {
    static constexpr std::array<u8, 224> asn1_header = {
        {0x00, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
         0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x30, 0x31, 0x30, 0x0D, 0x06,
         0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20}};

    std::vector<u8> message(asn1_header.begin(), asn1_header.end());
    CryptoPP::SHA256 sha;
    message.resize(message.size() + CryptoPP::SHA256::DIGESTSIZE);
    sha.CalculateDigest(message.data() + asn1_header.size(), data.data(), data.size());
    return message;
}

} // namespace HW::RSA
