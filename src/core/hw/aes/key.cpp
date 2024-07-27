// Copyright 2017 Citra Emulator Project
// Copyright 2024 Lime3DS Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>
#include <sstream>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/hle/service/fs/archive.h"
#include "core/hw/aes/arithmetic128.h"
#include "core/hw/aes/key.h"
#include "core/hw/rsa/rsa.h"

namespace HW::AES {

namespace {

// The generator constant was calculated using the 0x39 KeyX and KeyY retrieved from a 3DS and the
// normal key dumped from a Wii U solving the equation:
// NormalKey = (((KeyX ROL 2) XOR KeyY) + constant) ROL 87
// On a real 3DS the generation for the normal key is hardware based, and thus the constant can't
// get dumped. Generated normal keys are also not accessible on a 3DS. The used formula for
// calculating the constant is a software implementation of what the hardware generator does.
constexpr AESKey generator_constant = {{0x1F, 0xF9, 0xE9, 0xAA, 0xC5, 0xFE, 0x04, 0x08, 0x02, 0x45,
                                        0x91, 0xDC, 0x5D, 0x52, 0x76, 0x8A}};

AESKey HexToKey(const std::string& hex) {
    if (hex.size() < 32) {
        throw std::invalid_argument("hex string is too short");
    }

    AESKey key;
    for (std::size_t i = 0; i < key.size(); ++i) {
        key[i] = static_cast<u8>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }

    return key;
}

std::vector<u8> HexToVector(const std::string& hex) {
    std::vector<u8> vector(hex.size() / 2);
    for (std::size_t i = 0; i < vector.size(); ++i) {
        vector[i] = static_cast<u8>(std::stoi(hex.substr(i * 2, 2), nullptr, 16));
    }

    return vector;
}

std::optional<std::size_t> ParseCommonKeyName(const std::string& full_name) {
    std::size_t index;
    int end;
    if (std::sscanf(full_name.c_str(), "common%zd%n", &index, &end) == 1 &&
        end == static_cast<int>(full_name.size())) {
        return index;
    } else {
        return std::nullopt;
    }
}

std::optional<std::pair<std::size_t, std::string>> ParseNfcSecretName(
    const std::string& full_name) {
    std::size_t index;
    int end;
    if (std::sscanf(full_name.c_str(), "nfcSecret%zd%n", &index, &end) == 1) {
        return std::make_pair(index, full_name.substr(end));
    } else {
        return std::nullopt;
    }
}

std::optional<std::pair<std::size_t, char>> ParseKeySlotName(const std::string& full_name) {
    std::size_t slot;
    char type;
    int end;
    if (std::sscanf(full_name.c_str(), "slot0x%zXKey%c%n", &slot, &type, &end) == 2 &&
        end == static_cast<int>(full_name.size())) {
        return std::make_pair(slot, type);
    } else {
        return std::nullopt;
    }
}

struct KeySlot {
    std::optional<AESKey> x;
    std::optional<AESKey> y;
    std::optional<AESKey> normal;

    void SetKeyX(std::optional<AESKey> key) {
        x = key;
        GenerateNormalKey();
    }

    void SetKeyY(std::optional<AESKey> key) {
        y = key;
        GenerateNormalKey();
    }

    void SetNormalKey(std::optional<AESKey> key) {
        normal = key;
    }

    void GenerateNormalKey() {
        if (x && y) {
            normal = Lrot128(Add128(Xor128(Lrot128(*x, 2), *y), generator_constant), 87);
        } else {
            normal.reset();
        }
    }

    void Clear() {
        x.reset();
        y.reset();
        normal.reset();
    }
};

std::array<KeySlot, KeySlotID::MaxKeySlotID> key_slots;
std::array<std::optional<AESKey>, MaxCommonKeySlot> common_key_y_slots;
std::array<std::optional<AESKey>, NumDlpNfcKeyYs> dlp_nfc_key_y_slots;
std::array<NfcSecret, NumNfcSecrets> nfc_secrets;
AESIV nfc_iv;

struct KeyDesc {
    char key_type;
    std::size_t slot_id;
    // This key is identical to the key with the same key_type and slot_id -1
    bool same_as_before;
};

void LoadBootromKeys() {
    constexpr std::array<KeyDesc, 80> keys = {
        {{'X', 0x2C, false}, {'X', 0x2D, true},  {'X', 0x2E, true},  {'X', 0x2F, true},
         {'X', 0x30, false}, {'X', 0x31, true},  {'X', 0x32, true},  {'X', 0x33, true},
         {'X', 0x34, false}, {'X', 0x35, true},  {'X', 0x36, true},  {'X', 0x37, true},
         {'X', 0x38, false}, {'X', 0x39, true},  {'X', 0x3A, true},  {'X', 0x3B, true},
         {'X', 0x3C, false}, {'X', 0x3D, false}, {'X', 0x3E, false}, {'X', 0x3F, false},
         {'Y', 0x4, false},  {'Y', 0x5, false},  {'Y', 0x6, false},  {'Y', 0x7, false},
         {'Y', 0x8, false},  {'Y', 0x9, false},  {'Y', 0xA, false},  {'Y', 0xB, false},
         {'N', 0xC, false},  {'N', 0xD, true},   {'N', 0xE, true},   {'N', 0xF, true},
         {'N', 0x10, false}, {'N', 0x11, true},  {'N', 0x12, true},  {'N', 0x13, true},
         {'N', 0x14, false}, {'N', 0x15, false}, {'N', 0x16, false}, {'N', 0x17, false},
         {'N', 0x18, false}, {'N', 0x19, true},  {'N', 0x1A, true},  {'N', 0x1B, true},
         {'N', 0x1C, false}, {'N', 0x1D, true},  {'N', 0x1E, true},  {'N', 0x1F, true},
         {'N', 0x20, false}, {'N', 0x21, true},  {'N', 0x22, true},  {'N', 0x23, true},
         {'N', 0x24, false}, {'N', 0x25, true},  {'N', 0x26, true},  {'N', 0x27, true},
         {'N', 0x28, true},  {'N', 0x29, false}, {'N', 0x2A, false}, {'N', 0x2B, false},
         {'N', 0x2C, false}, {'N', 0x2D, true},  {'N', 0x2E, true},  {'N', 0x2F, true},
         {'N', 0x30, false}, {'N', 0x31, true},  {'N', 0x32, true},  {'N', 0x33, true},
         {'N', 0x34, false}, {'N', 0x35, true},  {'N', 0x36, true},  {'N', 0x37, true},
         {'N', 0x38, false}, {'N', 0x39, true},  {'N', 0x3A, true},  {'N', 0x3B, true},
         {'N', 0x3C, true},  {'N', 0x3D, false}, {'N', 0x3E, false}, {'N', 0x3F, false}}};

    // Bootrom sets all these keys when executed, but later some of the normal keys get overwritten
    // by other applications e.g. process9. These normal keys thus aren't used by any application
    // and have no value for emulation

    const std::string filepath = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + BOOTROM9;
    auto file = FileUtil::IOFile(filepath, "rb");
    if (!file) {
        return;
    }

    const std::size_t length = file.GetSize();
    if (length != 65536) {
        LOG_ERROR(HW_AES, "Bootrom9 size is wrong: {}", length);
        return;
    }

    constexpr std::size_t KEY_SECTION_START = 55760;
    file.Seek(KEY_SECTION_START, SEEK_SET); // Jump to the key section

    AESKey new_key;
    for (const auto& key : keys) {
        if (!key.same_as_before) {
            file.ReadArray(new_key.data(), new_key.size());
            if (!file) {
                LOG_ERROR(HW_AES, "Reading from Bootrom9 failed");
                return;
            }
        }

        LOG_DEBUG(HW_AES, "Loaded Slot{:#02x} Key{} from Bootrom9.", key.slot_id, key.key_type);

        switch (key.key_type) {
        case 'X':
            key_slots.at(key.slot_id).SetKeyX(new_key);
            break;
        case 'Y':
            key_slots.at(key.slot_id).SetKeyY(new_key);
            break;
        case 'N':
            key_slots.at(key.slot_id).SetNormalKey(new_key);
            break;
        default:
            LOG_ERROR(HW_AES, "Invalid key type {}", key.key_type);
            break;
        }
    }
}

void LoadPresetKeys() {
    const std::string filepath = FileUtil::GetUserPath(FileUtil::UserPath::SysDataDir) + AES_KEYS;
    FileUtil::CreateFullPath(filepath); // Create path if not already created

    boost::iostreams::stream<boost::iostreams::file_descriptor_source> file;
    FileUtil::OpenFStream<std::ios_base::in>(file, filepath);
    if (!file.is_open()) {
        return;
    }

    while (!file.eof()) {
        std::string line;
        std::getline(file, line);

        // Ignore empty or commented lines.
        if (line.empty() || line.starts_with("#")) {
            continue;
        }

        const auto parts = Common::SplitString(line, '=');
        if (parts.size() != 2) {
            LOG_ERROR(HW_AES, "Failed to parse {}", line);
            continue;
        }

        const std::string& name = parts[0];

        const auto nfc_secret = ParseNfcSecretName(name);
        if (nfc_secret) {
            auto value = HexToVector(parts[1]);
            if (nfc_secret->first >= nfc_secrets.size()) {
                LOG_ERROR(HW_AES, "Invalid NFC secret index {}", nfc_secret->first);
            } else if (nfc_secret->second == "Phrase") {
                nfc_secrets[nfc_secret->first].phrase = value;
            } else if (nfc_secret->second == "Seed") {
                nfc_secrets[nfc_secret->first].seed = value;
            } else if (nfc_secret->second == "HmacKey") {
                nfc_secrets[nfc_secret->first].hmac_key = value;
            } else {
                LOG_ERROR(HW_AES, "Invalid NFC secret '{}'", name);
            }
            continue;
        }

        AESKey key;
        try {
            key = HexToKey(parts[1]);
        } catch (const std::logic_error& e) {
            LOG_ERROR(HW_AES, "Invalid key {}: {}", parts[1], e.what());
            continue;
        }

        const auto common_key = ParseCommonKeyName(name);
        if (common_key) {
            if (common_key >= common_key_y_slots.size()) {
                LOG_ERROR(HW_AES, "Invalid common key index {}", common_key.value());
            } else {
                common_key_y_slots[common_key.value()] = key;
            }
            continue;
        }

        if (name == "dlpKeyY") {
            dlp_nfc_key_y_slots[DlpNfcKeyY::Dlp] = key;
            continue;
        }

        if (name == "nfcKeyY") {
            dlp_nfc_key_y_slots[DlpNfcKeyY::Nfc] = key;
            continue;
        }

        if (name == "nfcIv") {
            nfc_iv = key;
            continue;
        }

        const auto key_slot = ParseKeySlotName(name);
        if (!key_slot) {
            LOG_ERROR(HW_AES, "Invalid key name '{}'", name);
            continue;
        }

        if (key_slot->first >= MaxKeySlotID) {
            LOG_ERROR(HW_AES, "Out of range key slot ID {:#X}", key_slot->first);
            continue;
        }

        switch (key_slot->second) {
        case 'X':
            key_slots.at(key_slot->first).SetKeyX(key);
            break;
        case 'Y':
            key_slots.at(key_slot->first).SetKeyY(key);
            break;
        case 'N':
            key_slots.at(key_slot->first).SetNormalKey(key);
            break;
        default:
            LOG_ERROR(HW_AES, "Invalid key type '{}'", key_slot->second);
            break;
        }
    }
}

} // namespace

void InitKeys(bool force) {
    static bool initialized = false;
    if (initialized && !force) {
        return;
    }
    initialized = true;
    HW::RSA::InitSlots();
    LoadBootromKeys();
    LoadPresetKeys();
}

void SetKeyX(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetKeyX(key);
}

void SetKeyY(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetKeyY(key);
}

void SetNormalKey(std::size_t slot_id, const AESKey& key) {
    key_slots.at(slot_id).SetNormalKey(key);
}

bool IsKeyXAvailable(std::size_t slot_id) {
    return key_slots.at(slot_id).x.has_value();
}

bool IsNormalKeyAvailable(std::size_t slot_id) {
    return key_slots.at(slot_id).normal.has_value();
}

AESKey GetNormalKey(std::size_t slot_id) {
    return key_slots.at(slot_id).normal.value_or(AESKey{});
}

void SelectCommonKeyIndex(u8 index) {
    key_slots[KeySlotID::TicketCommonKey].SetKeyY(common_key_y_slots.at(index));
}

void SelectDlpNfcKeyYIndex(u8 index) {
    key_slots[KeySlotID::DLPNFCDataKey].SetKeyY(dlp_nfc_key_y_slots.at(index));
}

bool NfcSecretsAvailable() {
    auto missing_secret =
        std::find_if(nfc_secrets.begin(), nfc_secrets.end(), [](auto& nfc_secret) {
            return nfc_secret.phrase.empty() || nfc_secret.seed.empty() ||
                   nfc_secret.hmac_key.empty();
        });
    SelectDlpNfcKeyYIndex(DlpNfcKeyY::Nfc);
    return IsNormalKeyAvailable(KeySlotID::DLPNFCDataKey) && missing_secret == nfc_secrets.end();
}

const NfcSecret& GetNfcSecret(NfcSecretId secret_id) {
    return nfc_secrets[secret_id];
}

const AESIV& GetNfcIv() {
    return nfc_iv;
}

} // namespace HW::AES
