// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <vector>
#include "common/common_types.h"

namespace HW::AES {

enum KeySlotID : std::size_t {

    // Used to decrypt the SSL client cert/private-key stored in ClCertA.
    SSLKey = 0x0D,

    // AES keyslots used to decrypt NCCH
    NCCHSecure1 = 0x2C,
    NCCHSecure2 = 0x25,
    NCCHSecure3 = 0x18,
    NCCHSecure4 = 0x1B,

    // AES Keyslot used to generate the UDS data frame CCMP key.
    UDSDataKey = 0x2D,

    // AES Keyslot used to encrypt the BOSS container data.
    BOSSDataKey = 0x38,

    // AES Keyslot used to calculate DLP data frame checksum and encrypt Amiibo key data.
    DLPNFCDataKey = 0x39,

    // AES Keyslot used to generate the StreetPass CCMP key.
    CECDDataKey = 0x2E,

    // AES Keyslot used by the friends module.
    FRDKey = 0x36,

    // AES keyslot used for APT:Wrap/Unwrap functions
    APTWrap = 0x31,

    // AES keyslot used for decrypting ticket title key
    TicketCommonKey = 0x3D,

    MaxKeySlotID = 0x40,
};

enum DlpNfcKeyY : std::size_t {
    // Download Play KeyY
    Dlp = 0,

    // NFC (Amiibo) KeyY
    Nfc = 1
};

struct NfcSecret {
    std::vector<u8> phrase;
    std::vector<u8> seed;
    std::vector<u8> hmac_key;
};

enum NfcSecretId : std::size_t {
    UnfixedInfo = 0,
    LockedSecret = 1,
};

constexpr std::size_t MaxCommonKeySlot = 6;
constexpr std::size_t NumDlpNfcKeyYs = 2;
constexpr std::size_t NumNfcSecrets = 2;

constexpr std::size_t AES_BLOCK_SIZE = 16;

using AESKey = std::array<u8, AES_BLOCK_SIZE>;
using AESIV = std::array<u8, AES_BLOCK_SIZE>;

void InitKeys(bool force = false);

void SetKeyX(std::size_t slot_id, const AESKey& key);
void SetKeyY(std::size_t slot_id, const AESKey& key);
void SetNormalKey(std::size_t slot_id, const AESKey& key);

bool IsKeyXAvailable(std::size_t slot_id);
bool IsNormalKeyAvailable(std::size_t slot_id);
AESKey GetNormalKey(std::size_t slot_id);

void SelectCommonKeyIndex(u8 index);
void SelectDlpNfcKeyYIndex(u8 index);

bool NfcSecretsAvailable();
const NfcSecret& GetNfcSecret(NfcSecretId secret_id);
const AESIV& GetNfcIv();

} // namespace HW::AES
